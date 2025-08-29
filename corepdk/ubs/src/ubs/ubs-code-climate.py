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


import logging as log
import argparse
import json

def severity(s):
    if s == "Audit": return "info";
    if s == "Low": return "minor";
    if s == "Medium": return "major";
    if s == "High": return "critical";
    return "info"

def filename(f=""):
    return f.replace("builds/gfw/chips/mcu/mcxn236/", "")

def parse(issue):
    cc = {}
    cc["type"] = "issue"
    ev = issue["events"][0]
    for event in issue["events"]:
        if event["main"] == True:
            ev = event
            break
    cc["description"] = ev["eventDescription"]
    cc["check_name"] = f"{issue["checkerName"]}:{ev['eventTag']}"
    cc["fingerprint"] = issue["mergeKey"]
    cc["severity"] = severity(issue["checkerProperties"]["impact"])

    cc["location"] = {
        "path": filename(issue["mainEventFilePathname"]),
        "lines": {
            "begin": issue["mainEventLineNumber"]
        }
    }

    return cc

def covert(infile, outfile):
    cc = []
    with open(infile, "r") as fin:
        coverity = json.load(fin)
        for issue in coverity["issues"]:
            cc.append(parse(issue))
            log.info(cc[-1])

    with open(outfile, "w") as fout:
        json.dump(cc, fout, indent=2)

if __name__ == "__main__":
    p = argparse.ArgumentParser(prog='to-code-climate' ,
        description='coverts coverity output to codeclimate',
        epilog='Report bugs to <ssaulters@nvidia.com>')

    p.add_argument("-v", "--verbose",   help="verbose output", action="store_true")
    p.add_argument("infile", nargs=1,   help='file to covert')
    p.add_argument("outfile", nargs=1,  help='where to save')
    args = p.parse_args()
    args.infile = args.infile[0]
    args.outfile = args.outfile[0]

    # setup logger
    if args.verbose:
        log.basicConfig(format='%(levelname)s: %(message)s', level=log.INFO)
        log.info('logging set to verbose')
    else:
        log.basicConfig(format='%(levelname)s: %(message)s', level=log.FATAL)

    covert(args.infile, args.outfile)

