#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
# All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


import sys
import logging as log
import argparse
import re
import xml.etree.ElementTree as ET
import xml.dom.minidom as DOM
import os

ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
all_files = []
file_path_dict = {}

def log_exit(message):
    log.fatal(f"[to-junit-xml]: {message}")
    sys.exit(1)

def strip_colours(line):
    return ansi_escape.sub('',line)

class TestCase(object):
    """ 
    only handles gitlab supported juint xml 
    https://gitlab.com/gitlab-org/gitlab/-/blob/master/lib/gitlab/ci/parsers/test/junit.rb
    """
    def __init__(self, name, classname, time_secs=0, file=None, stdout=None, stderr=None):
        self.name = name
        self.classname = classname
        self.file = file
        self.time_secs = time_secs
        self.stdout = stdout
        self.stderr = stderr

        self.errors = []
        self.failures = []
        self.skipped = []

    def add_error(self, message=None, output=None, error_type=None):
        error = {}
        error["message"] = message
        error["output"] = output
        error["type"] = error_type
        if message or output:
            self.errors.append(error)

    def add_failure(self, message=None, output=None, failure_type=None):
        failure = {}
        failure["message"] = message
        failure["output"] = output
        failure["type"] = failure_type
        if message or output:
            self.failures.append(failure)

    def add_skipped(self, message=None, output=None):
        skipped = {}
        skipped["message"] = message
        skipped["output"] = output
        if message or output:
            self.skipped.append(skipped)

    def is_failure(self): return len(self.failures) > 0
    def is_error(self): return len(self.errors) > 0
    def is_skipped(self): return len(self.skipped) > 0

    def handle_result(self, results, type, elems):
        for res in results:
            if res["output"] or res["message"]:
                attrs = {"type": type}
                if res["message"]: attrs["message"] = res["message"]
                if res["type"]:    attrs["type"]    = res["type"]
                elem = ET.Element(type, attrs)
                if res["output"]: elem.text = res["output"]

                elems.append(elem)

    def to_xml(self):
        attribs = dict()
        attribs["name"] = self.name
        if self.classname:  attribs["classname"] = self.classname
        if self.file:       attribs["file"]      = self.file
        if self.time_secs:  attribs["time"]      = "%f" % self.time_secs

        elem = ET.Element("testcase", attribs)

        self.handle_result(self.failures, "failure", elem)
        self.handle_result(self.errors, "error", elem)
        self.handle_result(self.skipped, "skipped", elem)

        if self.stdout:
            stdout_element = ET.Element("system-out")
            stdout_element.text = self.stdout
            elem.append(stdout_element)

        if self.stderr:
            stderr_element = ET.Element("system-err")
            stderr_element.text = self.stderr
            elem.append(stderr_element)
        return elem

    def __str__(self):
        xml = ET.tostring(self.to_xml())
        xml = DOM.parseString(xml)
        xml = xml.toprettyxml()
        return xml

class TestSuite(object):

    def __init__(self, name):
        self.name = name
        self.test_cases = list()

    def to_xml(self):
        # build the test suite element
        attribs = dict()
        attribs["name"] = self.name
        attribs["errors"] = len([c for c in self.test_cases if c.is_error()])
        attribs["failures"] = len([c for c in self.test_cases if c.is_failure()])
        attribs["skipped"] = len([c for c in self.test_cases if c.is_skipped()])
        attribs["tests"] = len(self.test_cases)
        attribs["time"] = sum(c.time_secs for c in self.test_cases if c.time_secs)

        attribs = {k: str(v) for k, v in attribs.items()}

        xml = ET.Element("testsuite", attribs)

        # test cases
        for case in self.test_cases:
            xml.append(case.to_xml())

        return xml

suites = []
last_suite = None
content = None
cur_file = ""
full_path = ""
def parse(line):
    global cur_file
    global full_path
    global content
    global last_suite
    line = strip_colours(line).rstrip()
    parts = re.match(r'^([PWF]):([a-zA-Z0-9_/-]+)\.([a-zA-Z]+):([0-9]+)$', line)
    if parts:
        status = parts.group(1)
        filename = parts.group(2)
        extension = parts.group(3)
        line_num = parts.group(4)
        #log.info(f"{parts.group(1)}, {parts.group(2)}, {parts.group(3)}, {parts.group(4)}")
    else:
        log_exit(f"{line} fail to parse.")

    file = filename + "." + extension
    if file != cur_file:
        cur_file = file
        # open the file
        full_path = file_path_dict[cur_file]
        if full_path and os.path.isfile(full_path):
            file = open(full_path)
            content = file.readlines()
        else:
            log_exit(f"{cur_file} is not found.")

    ###### parse upward
    line_cursor = int(line_num)
    name = ""
    classname = ""
    ## .adb
    if extension == "adb":
        while line_cursor > 0 and (not classname or not name):
            match = re.match(r"^\s*package\s+body\s+([\w.]+)\.\w+\s+is", content[line_cursor-1])
            if match: classname = match.group(1) # suite name

            match = re.match(r"^\s*procedure\s+(\w+)\s+is", content[line_cursor-1])    
            if match: name = match.group(1) # test name
            line_cursor -= 1
    
    ## .cpp
    if extension == "cpp":
        while line_cursor > 0:
            match = re.match(r"UBS_TEST\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)", content[line_cursor-1])
            if match:
                classname = match.group(1) # suite name
                name = match.group(2) # test name
                break
            line_cursor -= 1
    ######

    if last_suite != classname:
        last_suite = classname
        suite = TestSuite(f'{classname}')
        suites.append(suite)
        log.info(suite)

    test = TestCase(f'{name}', classname, file=cur_file)
    suites[-1].test_cases.append(test)
    log.info(test)
    
    if status == "W":
        suites[-1].test_cases[-1].add_error("Warning", f"{full_path}:{line_num}", "WARN")
        return

    if status == "F":
        suites[-1].test_cases[-1].add_failure("Failure", f"{full_path}:{line_num}", "FAIL")
        return

    return


def create_database(fout):
    xml = ET.Element("testsuites")
    attribs = {"disabled": 0, "errors": 0, "failures": 0, "tests": 0, "time": 0.0}
    for suite in suites:
        suite_xml = suite.to_xml()
        for key in ["disabled", "errors", "failures", "tests"]:
            attribs[key] += int(suite_xml.get(key, 0))
        attribs["time"] += float(suite_xml.get("time", 0.0))
        xml.append(suite_xml)
    for key, value in attribs.items():
        xml.set(key, str(value))

    xml_string = ET.tostring(xml)
    xml_string = DOM.parseString(xml_string)
    xml_string = xml_string.toprettyxml()
        
    if fout == '-':
        print(xml_string)
    else:
        with open(fout, 'w') as f:
            f.write(xml_string)


if __name__ == "__main__":
    p = argparse.ArgumentParser(prog='to-junit-xml' ,
        description='coverts unittest output to JUNIT xml',
        epilog='Report bugs to <ssaulters@nvidia.com>')

    p.add_argument("-v", "--verbose", help="verbose output", action="store_true")
    p.add_argument("-t", "--tee", help="tee input to output", action="store_true")
    p.add_argument("-s", "--sources", required=True, help="sources file path")
    p.add_argument("file", nargs=1, help='file to store xml')
    args = p.parse_args()
    args.file = args.file[0]

    # setup logger
    if args.verbose:
        log.basicConfig(format='%(levelname)s: %(message)s', level=log.INFO)
        log.info('logging set to verbose')
    else:
        log.basicConfig(format='%(levelname)s: %(message)s', level=log.FATAL)

    try:
        with open(args.sources) as fin:
            all_files = fin.read().split()
    except:
        log_exit(f"{args.sources} fails to open.")

    # prepare source files path
    for f in all_files:
        name, suffix = os.path.splitext(f)
        if suffix == '.adb' or suffix == '.ads':
            # use unique ada filename as key
            file_path_dict[os.path.basename(f)] = f
        else:
            # use relative path + filename as key
            file_path_dict[f] = f

    # parse stdin
    for line in sys.stdin:
        if args.tee: 
            print(line, end='') # tee output
        if line != "\n": parse(line)

    # create db
    create_database(args.file)