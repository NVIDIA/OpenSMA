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
from ubs_gmk import expand as ex
import os
import subprocess

def ubs_gen_linker_script():
    gcov_insert = '''
#ifdef UBS_COVERAGE
  .gcov_info      :
  {
    PROVIDE (__gcov_info_start = .);
    KEEP (*(.gcov_info))
    PROVIDE (__gcov_info_end = .);
  }
#endif
'''

    fname = ex('$(UBS_LINKER_SCRIPT_AUTOGEN)')
    want_coverage = ex('$(GD_UBS_COVERAGE)')
    ld = ex('$(UBS_LD)')
    ubs.mkdirp(os.path.dirname(fname))
    ubs.ubs_info('autogenerating linker script\n')

    with open(fname, "wt") as fout:
        proc = subprocess.Popen([ld, '--verbose'], 
                                stdout=subprocess.PIPE, 
                                stderr=subprocess.PIPE,
                                universal_newlines=True)
        skip = True
        while True:
            lines = proc.stdout.readlines()
            for line in lines:
                if not skip:
                    if want_coverage and line.find("__bss_start = .") != -1:
                        fout.write(gcov_insert)
                    if line.startswith('========'):
                        skip = True
                    else:
                        fout.write(line)
                        
                if line.startswith('========'):
                    skip = False


            if proc.poll() is not None:
                break


    return None

ubs_gen_linker_script()    
