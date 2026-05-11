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


import argparse
import json
import struct
from jsonschema import validate
from pathlib import Path
from typing import Dict

class NvHeader:

    class Symbol:
        def __init__(self, config) -> None:
            self._value = config['data'][self.__class__.__name__]

        def binary(self):
            pass

    class SyncNumber(Symbol):
        def binary(self) -> bytes:
            return struct.pack(f'4s', self._value.encode('ascii'))

    class HeaderVersion(Symbol):
        def binary(self) -> bytes:
            return struct.pack('<H', self._value)

    class FwVersion(Symbol):
        def binary(self) -> bytes:
            major, minor, patch, build = [int(x, 10) for x in self._value.split('.')]
            return struct.pack('<HBHH', major, minor, patch, build)

    class BuildMode(Symbol):
        def binary(self) -> bytes:
            return struct.pack('<BBH', self._value, 0, 0)

    class ApSkuId(Symbol):
        def binary(self) -> bytes:
            return struct.pack('<I', int(self._value, 16))

    class PciVendorId(Symbol):
        def binary(self) -> bytes:
            return struct.pack('<H', int(self._value, 16))

    class PciDeviceId(Symbol):
        def binary(self) -> bytes:
            return struct.pack('<H', int(self._value, 16))

    class PciSubsystemVendorId(Symbol):
        def binary(self) -> bytes:
            return struct.pack('<H', int(self._value, 16))

    class PciSubsystemDeviceId(Symbol):
        def binary(self) -> bytes:
            return struct.pack('<H', int(self._value, 16))
    
    class Reserved2:
        def __init__(self, config) -> None:
            pass
        def binary(self) -> bytes:
            return struct.pack('<I', 0)

    class NvHeaderLength(Symbol):
        def binary(self) -> bytes:
            return struct.pack('<H', self._value)

    def __init__(self, config: Dict) -> None:
        self._config = config

    def generate(self, out: Path) -> None:
        validate(self._config['data'], self._config['schema'])
        binary = self.SyncNumber(self._config).binary()
        binary += self.HeaderVersion(self._config).binary()
        binary += self.FwVersion(self._config).binary()
        binary += self.BuildMode(self._config).binary()
        binary += self.ApSkuId(self._config).binary()
        binary += self.PciVendorId(self._config).binary()
        binary += self.PciDeviceId(self._config).binary()
        binary += self.PciSubsystemVendorId(self._config).binary()
        binary += self.PciSubsystemDeviceId(self._config).binary()
        binary += self.Reserved2(self._config).binary()
        binary += self.NvHeaderLength(self._config).binary()
        with Path(out).open('w') as fp:
            fp.write(f'const unsigned char nvheader[] __attribute__((section(".header"))) = {{\n')
            for i, byte in enumerate(binary):
                if i != 0 and i % 16 == 0:
                    fp.write('\n')
                fp.write(f' 0x{byte:02X},')
            fp.write('\n};\n')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('infile', help = 'input json config file')
    parser.add_argument('outfile', nargs='?', default = 'nvheader.c', help = 'output binary file name, default is nvheader.c')
    args = parser.parse_args()
    with Path(args.infile).open() as fp:
        config = json.load(fp)
    header = NvHeader(config)
    header.generate(args.outfile)


if __name__ == '__main__':
    main()