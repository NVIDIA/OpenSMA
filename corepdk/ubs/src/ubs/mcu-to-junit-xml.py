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

ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')

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
def parse(line, project):
    line = strip_colours(line).rstrip()

    # <<SuiteName>>
    m = re.match(r'<<([0-9a-zA-Z_]+)>>.*', line)
    if m != None:
        suite = TestSuite(f'{project}.{m.group(1)}')
        suites.append(suite)
        log.info(suite)
        return

    #  - file:SuiteName::TestName
    m = re.match(r' - ([/\-_a-z0-9.]+):(\w+)::(\w+)\s+\[', line)
    if m != None:
        name = m.group(3)
        classname = f'{project}.{m.group(2)}'
        test = TestCase(name, classname, file=m.group(1))
        suites[-1].test_cases.append(test)
        log.info(test)
        return
    # FAIL file.cc:12
    m = re.match(r'FAIL\s+([a-z0-9_\-./]+):(\d+)', line)
    if m != None:
        suites[-1].test_cases[-1].add_failure("Failure", line, "FAIL")
        return
    # WARN file.cc:12
    m = re.match(r'WARN\s+([a-z0-9_\-./]+):(\d+)', line)
    if m != None:
        suites[-1].test_cases[-1].add_error("Warning", line, "WARN")
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

    p.add_argument("-v", "--verbose",   help="verbose output", action="store_true")
    p.add_argument("-t", "--tee",       help="tee input to output", action="store_true")
    p.add_argument("-p", "--project",   help="project name", required=True)
    p.add_argument("file", nargs=1,     help='file to store xml')
    args = p.parse_args()
    args.file = args.file[0]

    # setup logger
    if args.verbose:
        log.basicConfig(format='%(levelname)s: %(message)s', level=log.INFO)
        log.info('logging set to verbose')
    else:
        log.basicConfig(format='%(levelname)s: %(message)s', level=log.FATAL)

    # parse stdin
    for line in sys.stdin:
        if args.tee: 
            print(line, end='') # tee output
        parse(line, args.project)

    # create db
    create_database(args.file)

