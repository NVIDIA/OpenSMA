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
import ubs_gmk

# decorate a function with this to make it directly callable from make
def ubs_export(func):
    ubs_gmk.register(func)
    return func

@ubs_export
def ubs_module_validator(module=''):
    '''
    Purpose: 
        To validate the format of MODULE from ubs's module-add command
    Input: 
        The MODULE variable
    Output:
        Validation passed: add the module infomation in environmental vaiables
        Validation failed: show error message and exit 1
    '''
    print("module-add info: running validator")
    mlist = module.split("::")
    if len(mlist) != 4:
        print(f'module-add error: specify the module with MODULE=pdk::{{mod/hal}}::{{MODULE_NAME}}::v{{major}}.{{minor}}.{{build}}')
        return "exit 1"
    pdk, mtype, mname, version = mlist
    if pdk != 'pdk' or not (mtype == 'hal' or mtype == 'mod'):
        print(f'module-add error: specify the module and its version with MODULE=pdk::{{mod/hal}}::{{MODULE_NAME}}::v{{major}}.{{minor}}.{{build}}')
        return "exit 1"
    if version[0] != 'v':
        print(f'module-add error: the module version should be v{{major}}.{{minor}}.{{build}}')
        return "exit 1"
    vlist = version[1:].split('.')
    if len(vlist) != 3:
        print(f'module-add error: the module version should be v{{major}}.{{minor}}.{{build}}')
        return "exit 1"
    major, minor, build = vlist
    ubs_gmk.eval(f'UBS_MODULE_NAME := {(mname)}')
    ubs_gmk.eval(f'UBS_MODULE_TYPE := {mtype}')
    ubs_gmk.eval(f'UBS_MODULE_VER_MAJOR := {int(major)}')
    ubs_gmk.eval(f'UBS_MODULE_VER_MINOR := {int(minor)}')
    ubs_gmk.eval(f'UBS_MODULE_VER_BUILD := {int(build)}')

@ubs_export
def ubs_module_conf_generator(dirs=''):
    '''
    Purpose: 
        To save the module version information in the config file using ubs's global define mechnisms
    Input: 
        The path and name of the config file
    Output:
        module does not exist: add the module inforamtion in the config file
        module already exists: show warning message and replace the module inforamtion in the config file
    '''
    print("module-add info: running conf generator")
    module_name = ubs_gmk.expand('$(UBS_MODULE_NAME)')
    module_ver_major = ubs_gmk.expand('$(UBS_MODULE_VER_MAJOR)')
    module_ver_minor = ubs_gmk.expand('$(UBS_MODULE_VER_MINOR)')
    module_ver_build = ubs_gmk.expand('$(UBS_MODULE_VER_BUILD)')
    module_ver_information = {
        f"GD_API_MAJOR_{module_name}": module_ver_major,
        f"GD_API_MINOR_{module_name}": module_ver_minor,
        f"GD_API_BUILD_{module_name}": module_ver_build,
        f"GD_API_VERSION_{module_name}": f"\"v{module_ver_major}.{module_ver_minor}.{module_ver_build}\""
    }
    data = {}
    for dir in dirs.split():
        fname = os.path.join(dir, 'project-version.mk')
        if os.path.exists(fname):
            with open(fname, 'rt') as fin:
                for line in fin:
                    if ':=' in line:
                        key, value = line.strip().split(' := ', 1)
                        data[key] = value
    module_exists = any(key in data for key in module_ver_information)
    data.update(module_ver_information)
    for dir in dirs.split():
        fname = os.path.join(dir, 'project-version.mk')
        if os.path.exists(dir):
            with open(fname, 'wt') as fout:
                for key, value in data.items():
                    fout.write(f"{key} := {value}\n")
    if module_exists:
        print(f"module-add warning: module {module_name}'s version information is replaced")
    else:
        print(f"module-add info: module {module_name}'s version information is added")

@ubs_export
def ubs_module_cpp_macro_generator(fname=''):
    '''
    Purpose: 
        To generate the macros in the .h file to support compile time version comparisons for cpp
    Input: 
        The path of the .h file
    Output:
        .h file does not exist: write a new file with MAKE_VERSION macro and the module version macro in the .h file
        .h file already exists: add or replace the module version macro in the .h file
    '''
    print("module-add info: running .h macro generator")
    module_name = ubs_gmk.expand('$(UBS_MODULE_NAME)')
    module_ver_major = ubs_gmk.expand('$(UBS_MODULE_VER_MAJOR)')
    module_ver_minor = ubs_gmk.expand('$(UBS_MODULE_VER_MINOR)')
    module_ver_build = ubs_gmk.expand('$(UBS_MODULE_VER_BUILD)')
    module_macro = f"#define PDK_CMN_API_{module_name}_CURRENT_VERSION PDK_CMN_API_MAKE_VERSION({module_ver_major}, {module_ver_minor}, {module_ver_build})\n"
    lines = []
    module_exist = False
    if os.path.exists(fname):
        with open(fname, 'rt') as fin:
            for line in fin:
                if module_name in line:
                    lines.append(module_macro)
                    module_exist = True
                else:
                    lines.append(line)
        with open(fname, 'wt') as fout:
            for line in lines:
                fout.write(line)
            if not module_exist:
                fout.write(module_macro)
    else:
        with open(fname, 'wt') as fout:
            fout.write("/*\n"
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
                       "#pragma once\n")
            fout.write(f"#define PDK_CMN_API_MAKE_VERSION(major, minor, patch) ((major) * 10000 + (minor) * 100 + (patch))\n")
            fout.write(module_macro)

@ubs_export
def ubs_module_ada_macro_generator(fname=''):
    '''
    Purpose: 
        To generate the macros in the .ads file to support compile time version comparisons for ada language
    Input: 
        The path of the .ads file
    Output:
        .ads file does not exist: write a new file with MAKE_VERSION macro and the module version macro in the .ads file
        .ads file already exists: add or replace the module version macro in the .ads file
    '''
    print("module-add info: running .ads macro generator")
    module_name = ubs_gmk.expand('$(UBS_MODULE_NAME)')
    module_ver_major = ubs_gmk.expand('$(UBS_MODULE_VER_MAJOR)')
    module_ver_minor = ubs_gmk.expand('$(UBS_MODULE_VER_MINOR)')
    module_ver_build = ubs_gmk.expand('$(UBS_MODULE_VER_BUILD)')
    module_macro = f"   {module_name}_CURRENT_VERSION : constant Integer := MAKE_VERSION({module_ver_major}, {module_ver_minor}, {module_ver_build});\n"
    lines = []
    module_exist = False
    if os.path.exists(fname):
        with open(fname, 'rt') as fin:
            for line in fin:
                if module_name in line:
                    lines.append(module_macro)
                    module_exist = True
                else:
                    lines.append(line)
        with open(fname, 'wt') as fout:
            for line in lines:
                if not module_exist and "end" in line:
                    fout.write(module_macro)
                fout.write(line)
    else:
        with open(fname, 'wt') as fout:
            fout.write(f"package Pdk.Cmn.Api_Version is\n")
            fout.write(f"   function MAKE_VERSION (major, minor, build : Integer) return Integer is\n")
            fout.write(f"      (major*10000 + minor*100 + patch);\n")
            fout.write(module_macro)
            fout.write(f"end;\n")

@ubs_export
def ubs_module_retriever(dirs=''):
    '''
    Purpose: 
        To retrieve the module version information from the config file
    Input: 
        The path of the config file
    Output:
        config file already exists: print out the module version information
        config file does not exist: show error message and exit 1
    '''
    print("module-list info: running retriever")
    for dir in dirs.split():
        fname = os.path.join(dir, 'project-version.mk')
        if os.path.exists(fname):
            with open(fname, 'r') as file:
                lines = file.readlines()
            for line in lines:
                line = line.strip()
                if line.startswith("GD_API_VERSION_"):
                    parts = line.split(":=")
                    if len(parts) == 2:
                        module_name = parts[0].replace("GD_API_VERSION_", "").strip()
                        version_value = parts[1].strip().strip('"')
                        print(f"module-list info: API_VERSION_{module_name} = {version_value}")
            return "exit 0"
        else:
            print(f"module-list error: no module was added yet")
            return "exit 1"