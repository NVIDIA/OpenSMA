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
import argparse
import logging as log
import clang.cindex
import re

def check_comment(comment):
    """Check if the comment follows Doxygen syntax."""
    if comment is None:
        return False
    return comment.startswith("/**") or comment.startswith("///")

def find_undoc(node):
    """Recursively traverse the AST and find undocumented entities."""
    # Skip nodes from other files
    if node.location.file and node.location.file.name != node.translation_unit.spelling:
        return

    if node.kind in [clang.cindex.CursorKind.FUNCTION_DECL, 
                     clang.cindex.CursorKind.CLASS_DECL,
                     clang.cindex.CursorKind.STRUCT_DECL,
                     clang.cindex.CursorKind.ENUM_DECL,
                     clang.cindex.CursorKind.NAMESPACE]:
        
        # Ignore forward declarations
        if node.is_definition():
            # Only check namespace comments in header files
            is_header_file = node.location.file.name.endswith(('.hpp', '.h'))
            if node.kind == clang.cindex.CursorKind.NAMESPACE and not is_header_file:
                return
            
            comment = node.raw_comment
            if not check_comment(comment):
                if args.verbose: 
                    log.info(f"{node.location.file.name}:{node.location.line}:{str(node.kind).split('.')[-1]} {node.spelling} is not documented")
                else: 
                    print(f"{node.location.file.name}:{node.location.line}:{str(node.kind).split('.')[-1]} {node.spelling} is not documented")
    
    for child in node.get_children():
        find_undoc(child)

def process_file(file_path):
    """Parse a single source file and detect undocumented declarations."""
    ext = os.path.splitext(file_path)[1].lower()
    if re.search(r'-test\.(cpp|hpp|h|c)$', os.path.basename(file_path)):
        return

    if ext not in ['.cpp', '.hpp', '.h', '.c']:
        return
        
    index = clang.cindex.Index.create()
    try:
        # Choose parse arguments based on file extension
        if ext in ['.cpp', '.hpp']: # .cpp and .hpp files
            parse_args = ['-x', 'c++', '-std=c++17']
        else:  # .c or .h files
            parse_args = ['-x', 'c', '-std=c99']
            
        translation_unit = index.parse(file_path, args=parse_args)
        find_undoc(translation_unit.cursor)
    except Exception as e:
        if args.verbose:
            log.error(f"Fail to process file {file_path}: {str(e)}")
        else:
            print(f"Fail to process file {file_path}: {str(e)}")
        sys.exit(1)

def process_directory(directory):
    """Recursively process all C++ files in the given directory."""
    for root, dirs, files in os.walk(directory):
        for file in files:
            file_path = os.path.join(root, file)
            process_file(file_path)

def main(path):
    """Process a file or directory to detect undocumented declarations."""
    if os.path.isfile(path):
        process_file(path)
    elif os.path.isdir(path):
        process_directory(path)
    else:
        if args.verbose:
            log.error(f"{path} is neither a valid file nor directory")
        else:
            print(f"{path} is neither a valid file nor directory")
        sys.exit(1)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog='missing-cdoc-detector',
        description='Detect missing Doxygen documentation in C/C++ source files.')

    parser.add_argument("-v", "--verbose", help="verbose output", action="store_true")
    parser.add_argument("input", nargs=1,help="input source file or directory")
    
    args = parser.parse_args()
    args.input = args.input[0]

    # setup logger
    if args.verbose:
        log.basicConfig(format='%(levelname)s: %(message)s', level=log.INFO)
        log.info('logging set to verbose')
    # Check if path exists
    if not os.path.exists(args.input):
        log.error(f"{args.input} does not exist")
        sys.exit(1)

    log.info(f"Processing path: {args.input}")
    main(args.input)
    sys.exit(0)