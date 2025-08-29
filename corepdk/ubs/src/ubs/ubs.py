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
import glob
import re
import ubs_gmk
import re

global_defines = {}
global_features = {}
expect_fatal = False

# decorate a function with this to make it directly callable from make
def ubs_export(func):
    ubs_gmk.register(func)
    return func


# -- immediate python printers -----------------------------------------------------------------
@ubs_export
def ubs_info(msg):
    fmt = ubs_gmk.expand("$(ubs-info-fmt)")
    print(f'{fmt} {msg}', end='', flush=True)

@ubs_export
def ubs_warn(msg):
    fmt = ubs_gmk.expand("$(ubs-warn-fmt)")
    print(f'{fmt} {msg}', end='', flush=True)

@ubs_export
def ubs_error(msg):
    fmt = ubs_gmk.expand("$(ubs-error-fmt)")
    print(f'{fmt} {msg}', end='', flush=True)

@ubs_export
def ubs_fatal(msg):
    global expect_fatal
    fmt = ubs_gmk.expand("$(ubs-fatal-fmt)")
    print(f'\n{fmt} {msg}\n', end='', flush=True)
    if expect_fatal:
        print(f'\n{fmt} Expected fatal, ignoring', end='', flush=True)
        expect_fatal = False
    else:
        exit(2)

@ubs_export
def ubs_expect_fatal(reset=True):
    """do not use for internal unittesting only"""
    global expect_fatal
    expect_fatal = reset

@ubs_export
def ubs_find(root,regex,flags=re.IGNORECASE):
    """recursively scan a directory, returning a string with all files that match the regex"""

    # use os.scandir for max perf, as it could be a large filesystem
    def scandir_r(directory,regex):
        for entry in os.scandir(directory):
            name = os.path.join(directory,entry.name)
            if entry.is_dir(follow_symlinks=False):
                yield from scandir_r(entry.path, regex)
            elif entry.is_file() and regex.match(name):
                yield entry.path

    return " ".join(scandir_r(root, re.compile(regex, flags)))

@ubs_export
def ubs_banner(msg):
    w = int(ubs_gmk.expand('$(UBS_WIDTH)'))
    logo = u'' + ubs_gmk.expand('$(UBS_LOGO)')
    sp   = u" " * (w - len(msg)*2 - 8)
    wmsg = u''.join(chr(ord(char) + 0xFF00 - 0x20) for char in msg)
    s  = f'{"\u2500" * w}\n'
    s += f'{logo}{sp}{wmsg}\n'
    s += f'{"\u2500" * w}'
    print(s)
    return None

def _ubs_dash_type(type):
    pre = ''
    post = ''
    match type[0]:
        case '-': dash = "\u2500"
        case '_': dash = "\u2501"
        case '=': dash = "\u2550"
        case '.': dash = "\u254C"
        case _  : dash = type
    if len(type) == 2:
        match type[1]:
            case 'f': pre = ubs_gmk.expand('$(UBS_FAINT)')
            case 'b': pre = ubs_gmk.expand('$(UBS_BOLD)')
        post = ubs_gmk.expand('$(UBS_N)')

    return pre,dash,post

@ubs_export
def ubs_line(type):
    w = int(ubs_gmk.expand('$(UBS_WIDTH)'))
    pre,dash,post = _ubs_dash_type(type)
    print(f'{dash * w}', flush=True)
    return None

# -- returns string to print ------------------------------------------------------------------
@ubs_export
def ubs_banner_str(msg):
    pr = ubs_gmk.expand('$(UBS_PRINT)')
    w = int(ubs_gmk.expand('$(UBS_WIDTH)'))
    logo = u'' + ubs_gmk.expand('$(UBS_LOGO)')
    sp   = u" " * (w - len(msg)*2 - 8)
    wmsg = u''.join(chr(ord(char) + 0xFF00 - 0x20) for char in msg)
    s  = f'{pr} "{"\u2500" * w}\\n'
    s += f'{logo}{sp}{wmsg}\\n'
    s += f'{"\u2500" * w}\\n"'
    return s

@ubs_export
def ubs_line_str(type):
    pr = ubs_gmk.expand('$(UBS_PRINT)')
    w = int(ubs_gmk.expand('$(UBS_WIDTH)'))
    pre,dash,post= _ubs_dash_type(type)
    return f'{pr} "{pre}{dash * w}{post}\\n"'


@ubs_export
def ubs_list_str(type):
    global ubs_global_defines
    def byfile(dirs):
        ret = []
        internal = ubs_gmk.expand('$(UBS)') == '.'
        for dir in dirs.split():
            for f in glob.glob(f'{dir}/*.mk'):
                if internal or f.find('ubs-selftests.mk') == -1:
                    ret.append(os.path.basename(os.path.splitext(f)[0]))
        return ret

    def has_hex(v):
        if isinstance(v,str): 
            return ''
        return f' (0x{hex(v)})'

    l = []
    match type:
        case 'projects': 
            l = byfile(ubs_gmk.expand('$(UBS_PATH_PROJECTS)'))
        case 'platforms': 
            l = byfile(ubs_gmk.expand('$(UBS_PATH_PLATFORMS)'))
        case 'sources': 
            l = ubs_gmk.expand('$(UBS_SRCS)').split()
        case 'global-defines': 
            if len(global_defines):
                width = len(max(global_defines,key=len))
                l = [ f'{k:>{width}} = {v}{has_hex(v)}' for k,v in global_defines.items() ]
        case _: ubs_fatal('unknown ubs_list_str type')

    l.sort()
    pr = ubs_gmk.expand('$(ubs-info)')
    s = "\n".join([f'{pr} "  - {s.replace('"','\\"')}\\n"' for s in l])
    return s

# -- custom flags for a specified file -------------------------------------------------------
ubs_customized_files = {}
def _ubs_gen_rule(cc,src,flags,dev,rel):
    idx = len(ubs_customized_files)
    mk  = f'UBS_{cc}_FLAGS_{idx}     := {flags}\n'
    mk += f'UBS_{cc}_FLAGS_dev_{idx} := {dev}\n'
    mk += f'UBS_{cc}_FLAGS_rel_{idx} := {rel}\n'
    mk += f'UBS_{cc}_FLAGS_{idx}     += $(UBS_{cc}_FLAGS_$(UBS_MODE)_{idx})\n'
    mk += f'$(UBS_PATH_OBJ)/{src}.o : {src}\n'
    mk += f'\t$(call ubs-compile-flags,{cc},$(UBS_{cc}_FLAGS_{idx}))\n'
    ubs_customized_files[src] = f'UBS_{cc}_FLAGS_{idx}'
    ubs_gmk.eval(mk)
    
def _ubs_gen_rule_with_filter(cc, src, flags, filter):
    idx = len(ubs_customized_files)
    base_flag  = f'UBS_{cc}_FLAGS_{idx}_BASE'
    final_flag = f'UBS_{cc}_FLAGS_{idx}'
    # remove the filter flag from the original flags
    mk  = f'{base_flag} = $(filter-out {filter},$(UBS_{cc}_FLAGS))\n'
    mk += f'{final_flag} = $({base_flag}) {flags}\n'
    
    # Ada files drop source extension for object files: file.adb -> file.o
    # C++ files keep source extension: file.cpp -> file.cpp.o
    if cc == 'ADA':
        # Strip .adb/.ads extension for Ada object files
        base, ext = os.path.splitext(src)
        obj_target = f'{base}.o'
    else:
        # Keep full name for C++ object files
        obj_target = f'{src}.o'
    
    mk += f'$(UBS_PATH_OBJ)/{obj_target} : {src}\n'
    # DEBUG: Uncomment the line below to see which custom flags are applied during build
    # mk += f'\techo "UBS_{cc}_FLAGS_{idx} := $({final_flag})"\n'
    mk += f'\t$(call ubs-compile-flags,{cc},$({final_flag}))\n'
    ubs_customized_files[src] = final_flag
    
    # DEBUG: Uncomment the lines below to see generated makefile rules for custom flags
    # ubs_info(f'DEBUG: Generated makefile rules for {cc} file {src}:\n')
    # for line in mk.strip().split('\n'):
    #     ubs_info(f'  {line}\n')
    
    ubs_gmk.eval(mk)

@ubs_export
def ubs_custom_flags(src, flags='', dev='', rel=''):
    base,ext = os.path.splitext(src);
    match ext:
        case '.c'  : _ubs_gen_rule('CC', src,flags,dev,rel)
        case '.cpp': _ubs_gen_rule('CXX',src,flags,dev,rel)
        case '.adb': _ubs_gen_rule('ADA',src,flags,dev,rel)
        case '.ads': _ubs_gen_rule('ADA',src,flags,dev,rel)

    return None

@ubs_export
def ubs_coverage_flags(src):
    files = []
    if os.path.isdir(src):
        # include all files in the src directory
        files = glob.glob(f'{src}/*.c', recursive=True) + \
                glob.glob(f'{src}/*.cpp', recursive=True) + \
                glob.glob(f'{src}/*.adb', recursive=True)
    else :
        files = [src]
        
    # exclude files that have -test in the filename
    files = [file for file in files if '-test' not in file]

    for file in files:
        base, ext = os.path.splitext(file)

        match ext:
            case '.c':    cc = 'CC'
            case '.cpp':  cc = 'CXX'
            case '.adb' | '.ads': cc = 'ADA'
            case _: return None

        base_flags = f'$(UBS_{cc}_FLAGS)'
        coverage_flags = '-Og --coverage -ftest-coverage -fprofile-arcs -fprofile-info-section'
        if cc == 'ADA':
            coverage_flags += ' -gnateS'
        ubs_info(f'adding coverage_flags to {file}\n') 
        _ubs_gen_rule_with_filter(cc, file, coverage_flags, '-O%')

    return None

# -- requirements check ----------------------------------------------------------------------
def _ubs_check(bin, heartbeat):
    if os.path.isfile(bin) and os.access(bin, os.X_OK):
        print(heartbeat, end='', flush=True)
        return bin

    path_env = ubs_gmk.expand('$(PATH)')
    dirs = path_env.split(os.pathsep)
    
    for dir in dirs:
        fn = os.path.join(dir, bin)
        if os.path.isfile(fn) and os.access(fn, os.X_OK):
            print(heartbeat, end='', flush=True)
            return fn
    return None

@ubs_export
def ubs_check_exec(bin, heartbeat='.'):
    res = _ubs_check(bin, heartbeat)
    if res == None:
        ubs_fatal(f'required file {bin} not found')

    return res

@ubs_export
def ubs_check_var(var, heartbeat='.'):
    val = ubs_gmk.expand(f'$({var})')
    if val == None or len(val.strip()) == 0:
        ubs_fatal(f'required var not set or found {var} := {val}')

    print(heartbeat, end='', flush=True)
    return val

@ubs_export
def ubs_check_project_file(prj):
    prj += '.mk'
    paths = ubs_gmk.expand('$(UBS_PATH_PROJECTS)')
    project = None
    for p in paths.split():
        fn = os.path.join(p,prj)
        if os.path.exists(fn):
            if project != None:
                ubs_warn(f"duplicate project name: {project} using {fn}\n")
            project = fn

    if project == None:
        ubs_fatal(f"project '{prj}' not found in search paths")

    ubs_gmk.eval(f'UBS_PROJECT_FILE := {project}');

    return project

@ubs_export
def ubs_check_platform_file(prj):
    prj += '.mk'
    paths = ubs_gmk.expand('$(UBS_PATH_PLATFORMS)')
    project = None
    for p in paths.split():
        fn = os.path.join(p,prj)
        if os.path.exists(fn):
            if project != None:
                ubs_warn(f"duplicate platform name: {project} using {fn}\n")
            project = fn

    if project == None:
        ubs_fatal(f"platform '{prj}' not found in search paths")

    ubs_gmk.eval(f'UBS_PROJECT_FILE := {project}');
    return project

@ubs_export
def ubs_choose_file(fn):
    """looks for $(PWD)/fn then $(UBS)/fn else fails."""
    root = ubs_gmk.expand('$(UBS)')
    if os.path.exists(fn): return fn
    ubs_fn = os.path.join(root, fn)
    if os.path.exists(ubs_fn): return ubs_fn
    ubs_fatal(f"required file not found '{fn}'")
    return None

@ubs_export
def ubs_include_plugins(verbose=False):
    """checks for UBS extensions"""
    dir = ubs_gmk.expand('$(UBS_PATH_ROOT)/plugins')
    ldir = "./etc/plugins"
    plugins = []
    for f in glob.glob(f'{dir}/*.mk'):
        fname = os.path.basename(f)
        local_file = os.path.join(ldir,fname)
        if os.path.exists(local_file):
            if verbose:
                ubs_warn(f'overriding UBS extension with local extension: {f}\n')
        else:
            plugins.append(f)

    for f in glob.glob(f'{ldir}/*.mk'):
        plugins.append(f)
    
    for f in plugins:
        if verbose:
            ubs_info(f'adding UBS extension: {f}\n')
            ubs_gmk.eval(f'include {f}')

@ubs_export
def ubs_find_unittests(srcs):
    """searches for any -test unittest file for each source file."""
    srcs = srcs.split()
    uts = []
    cpp_uts = 0 
    pat = ubs_gmk.expand('$(strip $(UBS_UNITTEST_SUFFIX))') # -test
    for file in srcs:
        fn,ext = os.path.splitext(file)
        for ext in ['.adb', '.cpp']: # we only support .cpp and .c as unittest extensions
            f = f'{fn}{pat}{ext}'
            if os.path.exists(f):
                ch,fn = ubs_fixup_source(f)
                uts.append(fn)
                if ext == ".cpp":
                    with open(f, "rt") as fcpp:
                        cpp_uts += sum(l.startswith('UBS_TEST') for l in fcpp)

    # add the count of cpp unittests
    ubs_gmk.eval(f'GD_UBS_UNITTEST_CPP_COUNT := {cpp_uts}')
    return " ".join(uts)


# used internally to convert a python type to something make can process
def _ubs_python_to_make(obj):
    if obj is True: return '1'
    elif obj is False or obj is None: return ''
    elif isinstance(obj, str): return obj
    elif isinstance(obj, bytes): return obj.decode()
    elif isinstance(obj, bytearray): return bytes(obj).decode()
    else:
        try:
            return bytes(memoryview(obj)).decode()
        except TypeError:
            try:
                return str(obj)
            except:
                return ''

# removes any duplicate paths and normalizes
def _ubs_paths_normalize(dirs):
    dirs = dirs.split()
    real = { idx: os.path.realpath(x) for idx,x in enumerate(dirs) }
    seen = set()
    for key,val in list(real.items()):
        if val in seen:
            del real[key]
        seen.add(val)
    
    return " ".join([ os.path.normpath(dirs[i]) for i in real.keys() ])

# normalises all UBS_PATH_* variables
def _ubs_fixup_paths():
    paths = ubs_gmk.expand('$(filter UBS_PATH_%,$(.VARIABLES))').split()
    paths.append('UBS')
    for path in paths:
        p = ubs_gmk.expand(f'$({path})')
        p = _ubs_paths_normalize(p)
        ubs_gmk.eval(f'{path} := {p}')

def ubs_fixup_source(fn):
    cwd = os.path.realpath(os.path.curdir)
    rsrc = os.path.realpath(fn)
    rdir = os.path.dirname(rsrc)
    if os.path.commonprefix([cwd,rdir]) != cwd:
        return (True, rsrc)
    return (False, os.path.normpath(fn))

# checks for files below the project dir
# if they are found, replace relative paths with full paths
@ubs_export
def ubs_fixup_sources(files=''):
    changed = False
    if files == '':
        srcs = ubs_gmk.expand('$(sort $(UBS_SRCS))').split()
    else:
        srcs = files.split()
    cwd  = os.path.realpath(os.path.curdir)
    for idx,src in enumerate(srcs):
        ch,srcs[idx] = ubs_fixup_source(src)
        if ch: changed = True

    if changed: 
        ubs_warn(f'detected source files below root path, using full pathnames\n')

    srcs = ' '.join(srcs)
    ubs_gmk.eval(f'UBS_SRCS := {srcs}')

@ubs_export
def ubs_fixup_includes(var):
    """
    1. check for existance and warn
    2. check for un normalized paths and warn/replace
    3. replace and lower paths with their realpath names
    """
    paths = ubs_gmk.expand(f'$({var})')
    out = []

    for p in paths.split():
        if not os.path.exists(p):
            ubs_warn(f"{var} path '{p}' does not exist\n")

        if os.path.normpath(p) != p: 
            ubs_warn(f"normalizing {var} path '{p}'\n")
            
        changed,fn = ubs_fixup_source(p)
        if changed: 
            ubs_warn(f"detected {var} '{p}' below root path, using full path\n")
            p = fn

        out.append(p)

    out = ' '.join(out)
    ubs_gmk.eval(f'{var} := {out}')


def mkdirp(p):
    if not os.path.exists(p):
        os.makedirs(p)

def _ubs_banner(fname):
    dir = ubs_gmk.expand('$(UBS_PATH_THEME)')
    with open(f'{dir}/{fname}') as f:
        print(f.read(), end='', flush=True)
    return None

def ubs_main():
    makecmdgoal = ubs_gmk.expand('$(MAKECMDGOALS)')
    project_type = ubs_gmk.expand('$(UBS_TYPE)')
    if project_type == 'module':
        etc_name = 'corepdk_etc/mk'  # module-level folder structure
        dirs = [f'{etc_name}/projects', f'{etc_name}/platforms', 'src', 'doc']
    else:
        etc_name = 'etc'  # project-level folder structure
        dirs = [f'{etc_name}/projects', f'{etc_name}/platforms', 'src', 'doc']

    if makecmdgoal not in ['clean', 'distclean']:
        for p in dirs:
            if not os.path.exists(p):
                mkdirp(p)

    _ubs_banner('banner')
    _ubs_fixup_paths()
    ubs_info("ubs python module initialized\n")

ubs_main()
