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

import os
import sys
import subprocess

tab = "    "

def run_test(command=''):
    try:
        result = subprocess.run(command, shell=True, capture_output=True, text=True)
        if result.returncode == 0:
            return True, result.stdout
        else:
            return False, result.stdout + result.stderr
    except Exception as e:
        return False, str(e)
    
def run_suite(test_cases=[]):
    global total
    successes = []
    outputs = []
    total += len(test_cases)
    for test_case in test_cases:
        success, output = run_test(test_case)
        successes.append(success)
        outputs.append(output)
    return successes, outputs

def module_add_executor_run_validator_suite():
    global passed
    print(f"{tab}Suite 1: To test the executor runs the validator")
    test_cases = ["./ubs module-add MODULE=pdk::mod::MCTP::v1.0.0"]
    successes, outputs = run_suite(test_cases)
    states = []
    for output in outputs:
        if "module-add info: running validator" in output:
            states.append("passed")
            passed += 1
        else:
            states.append("failed")
    print_result(test_cases, states)
    clean()

def module_add_validator_suite():
    global passed
    print(f"{tab}Suite 2: To test the validator validates the MODULE format correctly")
    # correct module format
    test_cases = ["./ubs module-add MODULE=pdk::mod::MCTP::v1.0.0",
                  "./ubs module-add MODULE=pdk::hal::I2C::v1.0.0"]
    successes, outputs = run_suite(test_cases)
    states = []
    for output in outputs:
        if "module-add error" not in output:
            states.append("passed")
            passed += 1
        else:
            print(output)
            states.append("failed")
    print_result(test_cases, states)
    clean()

    # wrong module format
    test_cases = ["./ubs module-add",
                  "./ubs module-add MODULE=hal::I2C::v1.0.0",
                  "./ubs module-add MODULE=pdk::I2C::v1.0.0",
                  "./ubs module-add MODULE=pdk::mod::hal::1.0.0",
                  "./ubs module-add MODULE=I2C::v1.0.0",
                  "./ubs module-add MODULE=I2C::v1.0.0"]
    successes, outputs = run_suite(test_cases)
    states = []
    for output in outputs:
        if "module-add error" in output:
            states.append("passed")
            passed += 1
        else:
            states.append("failed")
    print_result(test_cases, states)
    clean()

def module_add_executor_run_generator_suite():
    global passed
    print(f"{tab}Suite 3: To test the executor runs the genertors")
    test_cases = ["./ubs module-add MODULE=pdk::mod::MCTP::v1.0.0",
                  "./ubs module-add MODULE=pdk::hal::I2C::v1.0.0"]
    successes, outputs = run_suite(test_cases)
    states = []
    for output in outputs:
        if "module-add info: running conf generator" in output and "module-add info: running .h macro generator" in output and "module-add info: running .ads macro generator" in output:
            states.append("passed")
            passed += 1
        else:
            states.append("failed")
    print_result(test_cases, states)
    clean()

def module_add_conf_generator_suite():
    global passed
    print(f"{tab}Suite 4: To test the conf generator generates the correct content (.mk file)")
    test_cases = {
         "commands": ["./ubs module-add MODULE=pdk::mod::MCTP::v1.0.0"],
         "output_files": ["etc/projects/project-version.mk"],
         "expected_contents": [f"GD_API_MAJOR_MCTP := 1\n"\
                               f"GD_API_MINOR_MCTP := 0\n"\
                               f"GD_API_BUILD_MCTP := 0\n"\
                               f"GD_API_VERSION_MCTP := \"v1.0.0\"\n"]
    }
    successes, outputs = run_suite(test_cases["commands"])
    states = []
    for i, fname in enumerate(test_cases["output_files"]):
        if os.path.exists(fname):
            with open(fname, "r") as fin:
                content = fin.read()
                if content == test_cases["expected_contents"][i]:
                    states.append("passed")
                    passed += 1
                else:
                    states.append("failed")
        else:
            states.append("failed")
    print_result(test_cases["commands"], states)
    clean()

    test_cases = {
         "commands": ["./ubs module-add MODULE=pdk::mod::MCTP::v1.0.0; ./ubs module-add MODULE=pdk::hal::I2C::v1.0.0"],
         "output_files": ["etc/projects/project-version.mk"],
         "expected_contents": [f"GD_API_MAJOR_MCTP := 1\n"\
                               f"GD_API_MINOR_MCTP := 0\n"\
                               f"GD_API_BUILD_MCTP := 0\n"\
                               f"GD_API_VERSION_MCTP := \"v1.0.0\"\n"\
                               f"GD_API_MAJOR_I2C := 1\n"\
                               f"GD_API_MINOR_I2C := 0\n"\
                               f"GD_API_BUILD_I2C := 0\n"\
                               f"GD_API_VERSION_I2C := \"v1.0.0\"\n"]
    }
    successes, outputs = run_suite(test_cases["commands"])
    states = []
    for i, fname in enumerate(test_cases["output_files"]):
        if os.path.exists(fname):
            with open(fname, "r") as fin:
                content = fin.read()
                if content == test_cases["expected_contents"][i]:
                    states.append("passed")
                    passed += 1
                else:
                    states.append("failed")
        else:
            states.append("failed")
    print_result(test_cases["commands"], states)
    clean()

def module_add_cpp_macro_generator_suite():
    global passed
    print(f"{tab}Suite 5: To test the macro generator generates the correct content (.cpp file)")
    test_cases = {
         "commands": ["./ubs module-add MODULE=pdk::mod::MCTP::v1.0.0"],
         "output_files": ["build/default-x86-dev/gen/pdk-cmn-api-project.h"],
         "expected_contents": ["/*\n"
                               " * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.\n"
                               " * All rights reserved.\n"
                               " * SPDX-License-Identifier: Apache-2.0\n"
                               " *\n"
                               " * Licensed under the Apache License, Version 2.0 (the \"License\");\n"
                               " * you may not use this file except in compliance with the License.\n"
                               " * You may obtain a copy of the License at\n"
                               " *\n"
                               " * http://www.apache.org/licenses/LICENSE-2.0\n"
                               " *\n"
                               " * Unless required by applicable law or agreed to in writing, software\n"
                               " * distributed under the License is distributed on an \"AS IS\" BASIS,\n"
                               " * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n"
                               " * See the License for the specific language governing permissions and\n"
                               " * limitations under the License.\n"
                               " */\n"
                               "#pragma once\n"
                               "#define PDK_CMN_API_MAKE_VERSION(major, minor, patch) ((major) * 10000 + (minor) * 100 + (patch))\n"
                               "#define PDK_CMN_API_MCTP_CURRENT_VERSION PDK_CMN_API_MAKE_VERSION(1, 0, 0)\n"]
    }
    successes, outputs = run_suite(test_cases["commands"])
    states = []
    for i, fname in enumerate(test_cases["output_files"]):
        if os.path.exists(fname):
            with open(fname, "r") as fin:
                content = fin.read()
                if content == test_cases["expected_contents"][i]:
                    states.append("passed")
                    passed += 1
                else:
                    states.append("failed")
        else:
            states.append("failed")
    print_result(test_cases["commands"], states)
    clean()
    test_cases = {
         "commands": ["./ubs module-add MODULE=pdk::mod::MCTP::v1.0.0; ./ubs module-add MODULE=pdk::hal::I2C::v1.0.0"],
         "output_files": ["build/default-x86-dev/gen/pdk-cmn-api-project.h"],
         "expected_contents": ["/*\n"
                               " * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.\n"
                               " * All rights reserved.\n"
                               " * SPDX-License-Identifier: Apache-2.0\n"
                               " *\n"
                               " * Licensed under the Apache License, Version 2.0 (the \"License\");\n"
                               " * you may not use this file except in compliance with the License.\n"
                               " * You may obtain a copy of the License at\n"
                               " *\n"
                               " * http://www.apache.org/licenses/LICENSE-2.0\n"
                               " *\n"
                               " * Unless required by applicable law or agreed to in writing, software\n"
                               " * distributed under the License is distributed on an \"AS IS\" BASIS,\n"
                               " * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n"
                               " * See the License for the specific language governing permissions and\n"
                               " * limitations under the License.\n"
                               " */\n"
                               "#pragma once\n"
                               "#define PDK_CMN_API_MAKE_VERSION(major, minor, patch) ((major) * 10000 + (minor) * 100 + (patch))\n"
                               "#define PDK_CMN_API_MCTP_CURRENT_VERSION PDK_CMN_API_MAKE_VERSION(1, 0, 0)\n"
                               "#define PDK_CMN_API_I2C_CURRENT_VERSION PDK_CMN_API_MAKE_VERSION(1, 0, 0)\n"]
    }
    successes, outputs = run_suite(test_cases["commands"])
    states = []
    for i, fname in enumerate(test_cases["output_files"]):
        if os.path.exists(fname):
            with open(fname, "r") as fin:
                content = fin.read()
                if content == test_cases["expected_contents"][i]:
                    states.append("passed")
                    passed += 1
                else:
                    states.append("failed")
        else:
            states.append("failed")
    print_result(test_cases["commands"], states)
    clean()

def module_add_ada_macro_generator_suite():
    global passed
    print(f"{tab}Suite 6: To test the macro generator generates the correct content (.ads file)")
    test_cases = {
         "commands": ["./ubs module-add MODULE=pdk::mod::MCTP::v1.0.0"],
         "output_files": ["build/default-x86-dev/gen/pdk-cmn-api-project.ads"],
         "expected_contents": [f"package Pdk.Cmn.Api_Version is\n"\
                               f"   function MAKE_VERSION (major, minor, build : Integer) return Integer is\n"\
                               f"      (major*10000 + minor*100 + patch);\n"\
                               f"   MCTP_CURRENT_VERSION : constant Integer := MAKE_VERSION(1, 0, 0);\n"\
                               f"end;\n"]
    }
    successes, outputs = run_suite(test_cases["commands"])
    states = []
    for i, fname in enumerate(test_cases["output_files"]):
        if os.path.exists(fname):
            with open(fname, "r") as fin:
                content = fin.read()
                if content == test_cases["expected_contents"][i]:
                    states.append("passed")
                    passed += 1
                else:
                    states.append("failed")
        else:
            states.append("failed")
    print_result(test_cases["commands"], states)
    clean()
    test_cases = {
         "commands": ["./ubs module-add MODULE=pdk::mod::MCTP::v1.0.0; ./ubs module-add MODULE=pdk::hal::I2C::v1.0.0"],
         "output_files": ["build/default-x86-dev/gen/pdk-cmn-api-project.ads"],
         "expected_contents": [f"package Pdk.Cmn.Api_Version is\n"\
                               f"   function MAKE_VERSION (major, minor, build : Integer) return Integer is\n"\
                               f"      (major*10000 + minor*100 + patch);\n"\
                               f"   MCTP_CURRENT_VERSION : constant Integer := MAKE_VERSION(1, 0, 0);\n"\
                               f"   I2C_CURRENT_VERSION : constant Integer := MAKE_VERSION(1, 0, 0);\n"\
                               f"end;\n"]
    }
    successes, outputs = run_suite(test_cases["commands"])
    states = []
    for i, fname in enumerate(test_cases["output_files"]):
        if os.path.exists(fname):
            with open(fname, "r") as fin:
                content = fin.read()
                if content == test_cases["expected_contents"][i]:
                    states.append("passed")
                    passed += 1
                else:
                    states.append("failed")
        else:
            states.append("failed")
    print_result(test_cases["commands"], states)
    clean()

def module_add_executor_git_operation_suite():
    global passed
    print(f"{tab}Suite 7: To test the executor perform the git operations correctly")
    # existing module with existing version
    test_cases = ["./ubs module-add MODULE=pdk::mod::MCTP::v1.0.0",
                  "./ubs module-add MODULE=pdk::hal::I2C::v1.0.0"]
    successes, outputs = run_suite(test_cases)
    states = []
    for output in outputs:
        if "module-add info: module added" in output and "module-add info: module checkouted to the specify version" in output:
            states.append("passed")
            passed += 1
        else:
            states.append("failed")
    print_result(test_cases, states)
    clean()
    # non-existing module
    test_cases = ["./ubs module-add MODULE=pdk::mod::MCT::v1.0.0",
                  "./ubs module-add MODULE=pdk::hal::I3C::v1.0.0",
                  "./ubs module-add MODULE=pdk::mod::I2C::v1.0.0",
                  "./ubs module-add MODULE=pdk::hal::MCTP::v1.0.0"]
    successes, outputs = run_suite(test_cases)
    states = []
    for output in outputs:
        if "module-add error: module does not exist" in output:
            states.append("passed")
            passed += 1
        else:
            states.append("failed")
    print_result(test_cases, states)
    clean()
    # existing module with non-existing version
    test_cases = ["./ubs module-add MODULE=pdk::mod::MCTP::v1.0.1",
                  "./ubs module-add MODULE=pdk::hal::I2C::v1.1.0"]
    successes, outputs = run_suite(test_cases)
    states = []
    for output in outputs:
        if "module-add error: module version does not exists" and "module-add info: module deleted" in output:
            states.append("passed")
            passed += 1
        else:
            states.append("failed")
    print_result(test_cases, states)
    clean()

def module_add_test():
    print(f"module-add tests:")
    module_add_executor_run_validator_suite()
    module_add_validator_suite()
    module_add_executor_run_generator_suite()
    module_add_conf_generator_suite()
    module_add_cpp_macro_generator_suite()
    module_add_ada_macro_generator_suite()
    module_add_executor_git_operation_suite()

def module_list_executor_run_retriever_suite():
    global passed
    print(f"{tab}Suite 1: To test the executor runs the retriever")
    test_cases = ["./ubs module-list"]
    successes, outputs = run_suite(test_cases)
    states = []
    for output in outputs:
        if "module-list info: running retriever" in output:
            states.append("passed")
            passed += 1
        else:
            states.append("failed")
    print_result(test_cases, states)
    clean()

def module_list_retriever_suite():
    global passed
    print(f"{tab}Suite 2: To test the retriever prints out the correct information")
    # calling module-list when no module added
    test_cases = ["./ubs module-list"]
    successes, outputs = run_suite(test_cases)
    states = []
    for output in outputs:
        if "module-list error: no module was added yet" in output:
            states.append("passed")
            passed += 1
        else:
            states.append("failed")
    print_result(test_cases, states)
    clean()
    # calling module-list when some module added
    test_cases = ["./ubs module-add MODULE=pdk::mod::MCTP::v1.0.0; ./ubs module-list"]
    successes, outputs = run_suite(test_cases)
    states = []
    for output in outputs:
        if "API_VERSION_MCTP = v1.0.0" in output:
            states.append("passed")
            passed += 1
        else:
            states.append("failed")
    print_result(test_cases, states)
    clean()
    
def module_list_test():
    print(f"module-list tests")
    module_list_executor_run_retriever_suite()
    module_list_retriever_suite()

def print_result(test_cases=[], states=[]):
    for i in range(len(test_cases)):
        print(f"{tab}{tab}test case: {test_cases[i]}")
        print(f"{tab}{tab}test result: {states[i]}")

def clean(modules=["MCTP", "I2C"]):
    for module in modules:
        os.system(f"git submodule deinit -f share/mod/{module} > /dev/null 2>&1")
        os.system(f"rm -rf .git/modules/share/mod/{module} > /dev/null 2>&1")
        os.system(f"git rm -rf share/mod/{module} > /dev/null 2>&1")
    os.system(f"rm -rf etc/projects/project-version.mk > /dev/null 2>&1")
    os.system(f"rm -rf build/default-x86-dev/gen/pdk-cmn-api-project.h > /dev/null 2>&1")
    os.system(f"rm -rf build/default-x86-dev/gen/pdk-cmn-api-project.ads > /dev/null 2>&1")
    


def main():
    global passed
    global total
    passed = 0
    total = 0
    module_add_test()
    module_list_test()
    print(f"Test Summary: {passed}/{total} passed.")
    # will failed in UBS pipeline if returning a negative number
    return passed-total

if __name__ == '__main__':
    sys.exit(main())
