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

import ubs
import ubs_gmk
import os
import re

def ex(x) : return ubs_gmk.expand(f'$(strip $({x}))')

# tab = indent length, l = list
def format_list(tab, l):
    if len(l) == 0:
        return '()'

    tab = ' ' * (tab + 1)
    s = f'("{l[0]}"'
    if len(l) > 1:
        s += ','
        flags = [ f'"{x}"' for x in l[1:] ]
        flags = f",\n{tab}".join(flags)
        s += f'\n{tab}{flags})'
    else:
        s += ')'
    return s

def languages():
    langs = []
    # check for file types in current project
    if ex('UBS_SRCS_ADA'): langs.append('"Ada"')
    if ex('UBS_SRCS_ASM'): langs.append('"Asm_Cpp"')
    if ex('UBS_SRCS_CC'):  langs.append('"C"')
    if ex('UBS_SRCS_CXX'): langs.append('"C++"')
    return ", ".join(langs)

def main():
    return ubs_gmk.expand(f'$(notdir $(UBS_MAIN))')

def source_dirs():
    dirs = ex('UBS_ADA_INCLUDES').split()
    dirs += ubs_gmk.expand('$(sort $(dir $(UBS_SRCS_CXX) $(UBS_SRCS_CC) $(UBS_SRCS_ASM)))').split()
    dirs += ex('UBS_PATH_GEN').split()
    return format_list(30, dirs);

def source_files(tool=''):
    files = ubs_gmk.expand('$(notdir $(UBS_SRCS_ADA)) ')
    if tool != 'gnatsas':
        files += ubs_gmk.expand('$(notdir $(UBS_SRCS_CXX) $(UBS_SRCS_CC) $(UBS_SRCS_ASM))')
        # add all .ads for gnatdoc
        allfiles = ubs_gmk.expand('$(notdir $(UBS_SRCS_ADB:.adb=.ads))').split()
        for f in allfiles:
            if ubs.ubs_find(ex('UBS_PATH_SRC'), rf".*{re.escape(f)}"): # if the .ads file exists
                files += " " + f
    return format_list(30,files.split());


def executable():
    exe = ubs_gmk.expand('$(notdir $(UBS_TARGET))')
    return exe

# TODO: make this configurable
def pretty_printer():
    corepdk_format_cfg = "corepdk_etc/mk/gnatpp-format"
    gnatpp_format_cfg = f"etc/gnatpp-format"
    ubs_gnatpp_format_cfg = f"{ex('UBS')}/{gnatpp_format_cfg}"
    if os.path.exists(corepdk_format_cfg):
        with open(corepdk_format_cfg, 'r') as file:
            switches = file.readlines()
    elif os.path.exists(gnatpp_format_cfg):
        with open(gnatpp_format_cfg, 'r') as file:
            switches = file.readlines()
    elif os.path.exists(ubs_gnatpp_format_cfg):
        with open(ubs_gnatpp_format_cfg, 'r') as file:
            switches = file.readlines()
    else:
        ubs.ubs_warn(f"using default CorePDK Ada formatting\n")
        switches = [
          "--wide-character-encoding=8",
          "--indentation=3", 
          "--indent-continuation=2",
          "--keyword-lower-case",
          "--pragma-lower-case",
          "--attribute-mixed-case",
          "--enum-mixed-case",
          "--type-mixed-case",
          "--number-mixed-case",
          "--name-mixed-case",

          "--alignment", 
          "--align-modes",
          "--no-compact",
          "--RM-style-spacing",
          "--no-separate-is", 
          "--no-separate-return", 
          "--no-separate-then", 
          "--no-separate-loop-then",
          "--vertical-named-aggregates",

          "--par-threshold=3", 
          "--call-threshold=1",
          "--comments-unchanged",
          "--no-end-id",
          "--eol=unix", 
          "--max-line-length=96",
          ];
    # export to make as UBS_GNAT_PP_FLAGS
    ubs_gmk.eval(f"UBS_GNAT_PP_FLAGS += {' '.join(s.replace('\n', '') for s in switches)}")
    # generate same for gpr project
    content = ",\n".join([f'      "{x.replace('\n', '')}"' for x in switches ])
    return f"""
  package Pretty_Printer is
    for Default_Switches ("Ada") use (
{content}
    );
  end Pretty_Printer; 
"""


def switches(tab):
    # TODO: fix this properly, gprbuild runs from UBS_PATH_OBJ
    includes = {
        f'{ex('UBS_PATH_OBJ')}/': './',
        f'{ex('UBS_PATH_GEN')}/': '../gen/'
    }

    langs = {'ADA':'Ada', 'CXX': 'C++', 'CC': 'C', 'ASM': 'Asm_Cpp'}
    s = ''
    for lang,val in langs.items():
        id = ex(f'UBS_SRCS_{lang}')

        if len(id) > 0:
            flags = ex(f"UBS_{lang}_FLAGS").split()

            # deal with paths
            for idx,f in enumerate(flags):
                for key,rep in includes.items():
                    flags[idx] = flags[idx].replace(key,rep)
                # these must be absolute paths because gnatprove/sas all run from different folders
                if f.startswith("-I"):
                    flags[idx] = f"-I{os.path.abspath(f[2:])}"

            if len(flags) > 0: 
                l = f'{tab}for Switches("{val}") use '
                s += l + format_list(len(l), flags) + ';\n'
      

    # check for any file overrides
    for src,var in ubs.ubs_customized_files.items():
        fn = os.path.basename(src)
        flags = ex(var).split()
        if len(flags) > 0:
            l = f'{tab}for Switches("{fn}") use '
            s += l + format_list(len(l), flags) + ';\n'

    return s
    

# this is used for tooling only
def ubs_gpr_project_gen(fn):
    ubs.ubs_info(f"autogenerating gpr project: {fn}\n")
    project_type = ex('UBS_TYPE')
    with open(fn, "wt") as fout:
        fout.write('-- UBS: autogen gpr project\n')
        fout.write(f'project Default is\n\n')
        fout.write(f'  Tool := External ("GPR_TOOL", "");\n')
        fout.write(f'  for Create_Missing_Dirs use "True";\n')
        fout.write(f'  for Target              use "{ex("ADA_TARGET")}";\n')
        fout.write(f'  for Runtime("Ada")      use "{ex("ADA_RTS")}";\n')
        fout.write(f'  for Languages           use ({languages()});\n')
        fout.write(f'  for Object_Dir          use "{ex("UBS_PATH_OBJ")}";\n')
        fout.write(f'  for Source_Dirs         use {source_dirs()};\n')
        if project_type == 'library':
             fout.write(f"""
    case Tool is
        when "gnatsas" =>
        for Source_Files use {source_files("gnatsas")};
        when others => 
        for Source_Files use {source_files()};
    end case;\n\n""")
        else:
            fout.write(f"""
    case Tool is
        when "gnatsas" =>
        for Source_Files use {source_files("gnatsas")};
        when others => 
        for Main         use ("{main()}");
        for Source_Files use {source_files()};
    end case;\n\n""")
        fout.write(f'  package Builder is\n')
        fout.write(f'    for Executable_Suffix use "";\n')
        fout.write(f'    for Executable("{main()}") use "{executable()}";\n')
        fout.write(f'  end Builder;\n\n')
        fout.write(f'  package Compiler is\n')
        fout.write(switches('    '))
        fout.write(f'  end Compiler;\n\n')
        fout.write(pretty_printer())
        fout.write(f'\nend Default;\n')

    return None

def ubs_compile_flags_gen(fn):
    ubs.ubs_info(f"autogenerating clang config: {fn}\n")
    flags = ubs_gmk.expand('$(sort $(strip $(UBS_CXX_FLAGS) $(UBS_CC_FLAGS)))').split()

    def check(s):
        for f in ["-f", "-m"]:
            if s.startswith(f):
                return False
        return True

    with open(fn, "wt") as fout:
        for flag in flags:
            if flag.startswith('-D'):
                flag = flag.replace('\\"', '"')
            if check(flag):
                fout.write(f'{flag}\n')
    return None


ubs_gpr_project_gen(ex("UBS_GPR_PROJECT"))
ubs_compile_flags_gen("compile_flags.txt")
