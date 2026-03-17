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

import struct
from datetime import datetime
import json
import sys
import argparse
from bitarray import bitarray
from bitarray.util import ba2int
import os
import binascii

PLDMFW_PACAKGE_UUID = 'F018878CCB7D49439800A02F059ACA02'
PLDMFW_FORMAT_V1_0_0 = 1
DESCRIPTOR_TYPE_UUID = 0x0002
DESCRIPTOR_TYPE_PCI_VENDOR_ID = 0x0000
DESCRIPTOR_TYPE_INA_ENTERPRISE_ID = 0x0001
DESCRIPTOR_TYPE_PCI_DEVICE_ID = 0x0100
DESCRIPTOR_TYPE_PCI_SUBSYS_VENDOR_ID = 0x0101
DESCRIPTOR_TYPE_PCI_SUBSYS_ID = 0x0102
DESCRIPTOR_TYPE_VENDOR = 0xFFFF
DESCRIPTOR_TYPE_VENDOR_TITLE_GLACIERDSD = 'GLACIERDSD'
DESCRIPTOR_TYPE_VENDOR_TITLE_APSKU = 'APSKU'
DESCRIPTOR_TYPE_VENDOR_TITLE_ECSKU = 'ECSKU'
DESCRIPTOR_TYPE_VENDOR_TITLE_RECOVERY = 'RECOVERY'
STRING_TYPE_ASCII = 1

class pldmpkg:
    @staticmethod
    def check_supported_options(desc, supported, cfgoptions):
        for i in cfgoptions:
            if not i in supported:
                raise Exception('{0}: bit {1} not supported'.format(desc, i))

    @staticmethod
    def components_bytes(count):
        return (count + 7) >> 3

    @staticmethod
    def bitarray_to_int(options):
        a = bitarray(32, endian='little')
        a.setall(0)
        for i in options:
            a[i] = 1
        return ba2int(a)

    @staticmethod
    def print_raw(data):
        for i in range(len(data)):
            if i != 0 and (i % 16) == 0:
                print('')
            print('{0:02X} '.format(data[i]), end = '')
        print('')

    class component:
        def __init__(self):
            self.version = ''
            self.image_size = 0
            self.offset = 0
            self.identifier = 0
            self.classifier = 0
            self.activation = []
            self.options = []
            self.comparison = 0xFFFFFFFF

        def parse(self, comp, image):
            if not os.path.exists(image):
               raise Exception('image file {0} not exist'.format(image))

            self.image_size = os.stat(image).st_size
            self.version = comp['Version']
            self.identifier = int(comp['Identifier'], 16)
            self.classifier = int(comp['Classifier'], 16)

            supported = [0, 1]
            self.options = comp['Options']
            pldmpkg.check_supported_options('Component options', supported, self.options)

            supported = [0, 1, 2, 3, 4, 5]
            self.activation = comp['ActivationMethod']
            pldmpkg.check_supported_options('Component Activation Method', supported, self.options)

            #optional fields
            if 'ComparisonStamp' in comp:
                self.comparison = int(comp['ComparisonStamp'], 16)

        def size(self):
            # fixed size is 22 bytes, the only variable is version
            return 22 + len(self.version)

        def get_image_size(self):
            return self.image_size

        def update_offset(self, offset):
            self.offset = offset

        def print(self):
            print('version: {0}'.format(self.version))
            print('image size: {0} ({1})'.format(self.image_size, hex(self.image_size)))
            print('classifier: {0} ({1})'.format(self.classifier, hex(self.classifier)))
            print('identifier: {0} ({1})'.format(self.identifier, hex(self.identifier)))
            print('activation: {0}'.format(self.activation))
            print('options: {0}'.format(self.options))

        def get_raw(self):
            options = 0
            activate = 0
            for i in self.options:
                options |= (1 << i)
            for i in self.activation:
                activate |= (1 << i)
            fmt = '<HHIHHIIBB{0}s'.format(len(self.version))
            return struct.pack(fmt, self.classifier, self.identifier, self.comparison,
                       options, activate, self.offset, self.image_size, STRING_TYPE_ASCII,
                       len(self.version), self.version.encode('ascii'))

    class uuid_descriptor:
        def parse(self, desc):
            self.raw = desc['Data']
            self.data = bytearray.fromhex(self.raw)
            if len(self.data) != 16:
                raise Exception('UUID {0} size is not 16 bytes'.format(self.data))

        def size(self):
            # type (2 bytes), length (2 bytes), UUID (16 bytes)
            return 2 + 2 + 16

        def print(self):
            print('UUID Descriptor')
            print('Data: {0}'.format(self.data))

        def get_raw(self):
            return struct.pack('<HH16s', DESCRIPTOR_TYPE_UUID, 16, self.data)

    class ina_descriptor:
        def parse(self, desc):
            self.data = int(desc['Data'], 16)
            #self.data = bytearray.fromhex('{0:08X}'.format(val))

        def print(self):
            print('INA Enterprise ID Descriptor')
            print('ID: 0x{0:08X}'.format(self.data))

        def size(self):
            return 8

        def get_raw(self):
            return struct.pack('<HHI', DESCRIPTOR_TYPE_INA_ENTERPRISE_ID, 4, self.data)

    class pci_vendor_id_descriptor:
        def parse(self, desc):
            self.data = int(desc['Data'], 16)
            #self.data = bytearray.fromhex('{0:08X}'.format(val))

        def print(self):
            print('PCI vendor ID Descriptor')
            print('ID: 0x{0:08X}'.format(self.data))

        def size(self):
            # type (2 bytes), length (2 bytes), pci (2 bytes)
            return 2 + 2 + 2

        def get_raw(self):
            return struct.pack('<HHH', DESCRIPTOR_TYPE_PCI_VENDOR_ID, 2, self.data)

    class pci_device_id_descriptor:
        def parse(self, desc):
            self.data = int(desc['Data'], 16)
            #self.data = bytearray.fromhex('{0:08X}'.format(val))

        def print(self):
            print('PCI device ID Descriptor')
            print('ID: 0x{0:08X}'.format(self.data))

        def size(self):
            # type (2 bytes), length (2 bytes), pci (2 bytes)
            return 2 + 2 + 2

        def get_raw(self):
            return struct.pack('<HHH', DESCRIPTOR_TYPE_PCI_DEVICE_ID, 2, self.data)

    class pci_subsys_vendor_descriptor:
        def parse(self, desc):
            self.data = int(desc['Data'], 16)
            #self.data = bytearray.fromhex('{0:08X}'.format(val))

        def print(self):
            print('PCI subsys vendor ID Descriptor')
            print('ID: 0x{0:08X}'.format(self.data))

        def size(self):
            # type (2 bytes), length (2 bytes), pci (2 bytes)
            return 2 + 2 + 2

        def get_raw(self):
            return struct.pack('<HHH', DESCRIPTOR_TYPE_PCI_SUBSYS_VENDOR_ID, 2, self.data)

    class pci_subsys_id_descriptor:
        def parse(self, desc):
            self.data = int(desc['Data'], 16)
            #self.data = bytearray.fromhex('{0:08X}'.format(val))

        def print(self):
            print('PCI subsys ID Descriptor')
            print('ID: 0x{0:08X}'.format(self.data))

        def size(self):
            # type (2 bytes), length (2 bytes), pci (2 bytes)
            return 2 + 2 + 2

        def get_raw(self):
            return struct.pack('<HHH', DESCRIPTOR_TYPE_PCI_SUBSYS_ID, 2, self.data)

    class vendor_descriptor_apsku:
        def __init__(self):
            self.title = ''

        def parse(self, desc):
            self.title = desc['Title']
            self.data = int(desc['Data'], 16)

        def print(self):
            print('Vendor-Defined APSKU Descriptor (0xFFFF)')
            print('Title: {0}'.format(self.title))
            print('Data: 0x{0:08X}'.format(self.data))

        def size(self):
            # the last 4 for Data
            return 6 + len(self.title) + 4

        def get_raw(self):
            fmt = '<HHBB{0}sI'.format(len(self.title))
            r = struct.pack(fmt,
                DESCRIPTOR_TYPE_VENDOR,
                2+len(self.title)+4,
                STRING_TYPE_ASCII,
                len(self.title),
                self.title.encode('ascii'),
                self.data)
            return r
    class vendor_descriptor_recovery:
        def __init__(self):
            self.title = ''

        def parse(self, desc):
            self.title = desc['Title'].lower()
            self.raw = desc['Data']
            self.data = bytearray.fromhex(self.raw)

        def print(self):
            print('Vendor-Defined Recovery Descriptor (0xFFFF)')
            print('Title: {0}'.format(self.title))
            print('Data: 0x{0:08X}'.format(self.data))

        def size(self):
            # the last 8 for Data
            return 6 + len(self.title) + 8

        def get_raw(self):
            fmt = '<HHBB{0}s8s'.format(len(self.title))
            r = struct.pack(fmt,
                DESCRIPTOR_TYPE_VENDOR,
                2+len(self.title)+8,
                STRING_TYPE_ASCII,
                len(self.title),
                self.title.encode('ascii'),
                self.data)
            return r
        
    class vendor_descriptor_ecsku:
        def __init__(self):
            self.title = ''

        def parse(self, desc):
            self.title = desc['Title']
            self.data = int(desc['Data'], 16)

        def print(self):
            print('Vendor-Defined ECSKU Descriptor (0xFFFF)')
            print('Title: {0}'.format(self.title))
            print('Data: 0x{0:08X}'.format(self.data))

        def size(self):
            # the last 4 for Data
            return 6 + len(self.title) + 4

        def get_raw(self):
            fmt = '<HHBB{0}sI'.format(len(self.title))
            r = struct.pack(fmt,
                DESCRIPTOR_TYPE_VENDOR,
                2+len(self.title)+4,
                STRING_TYPE_ASCII,
                len(self.title),
                self.title.encode('ascii'),
                self.data)
            return r

    class vendor_descriptor_glacierdsd:
        def __init__(self):
            self.title = ''

        def parse(self, desc):
            self.title = desc['Title']
            self.data = int(desc['Data'], 16)

        def print(self):
            print('Vendor-Defined Descriptor (0xFFFF)')
            print('Title: {0}'.format(self.title))
            print('Data: 0x{0:02X}'.format(self.data))

        def size(self):
            # the last 1 for VendorDefinedDescriptorData, which we use one byte only
            return 6 + len(self.title) + 1

        def get_raw(self):
            fmt = '<HHBB{0}sB'.format(len(self.title))
            r = struct.pack(fmt,
                DESCRIPTOR_TYPE_VENDOR,
                2+len(self.title)+1,
                STRING_TYPE_ASCII,
                len(self.title),
                self.title.encode('ascii'),
                self.data)
            return r

    class device:
        def __init__(self, comp_count):
            self.comp_bytes = pldmpkg.components_bytes(comp_count)
            self.imageset_version = ''
            self.descriptors = []
            self.packagedata = bytearray([])
            self.applicable_comps = []
            self.update_options = []

        def parse(self, fd):
            descs = fd['Descriptors']
            obj = None
            for desc in descs:
                typ = int(desc['Type'], 16)
                if typ == DESCRIPTOR_TYPE_UUID:
                    obj = pldmpkg.uuid_descriptor()
                elif typ == DESCRIPTOR_TYPE_INA_ENTERPRISE_ID:
                    obj = pldmpkg.ina_descriptor()
                elif typ == DESCRIPTOR_TYPE_PCI_VENDOR_ID:
                    obj = pldmpkg.pci_vendor_id_descriptor()
                elif typ == DESCRIPTOR_TYPE_PCI_DEVICE_ID:
                    obj = pldmpkg.pci_device_id_descriptor()
                elif typ == DESCRIPTOR_TYPE_PCI_SUBSYS_VENDOR_ID:
                    obj = pldmpkg.pci_subsys_vendor_descriptor()
                elif typ == DESCRIPTOR_TYPE_PCI_SUBSYS_ID:
                    obj = pldmpkg.pci_subsys_id_descriptor()
                elif typ == DESCRIPTOR_TYPE_VENDOR:
                    if desc['Title'] == DESCRIPTOR_TYPE_VENDOR_TITLE_GLACIERDSD:
                        obj = pldmpkg.vendor_descriptor_glacierdsd()
                    elif desc['Title'] == DESCRIPTOR_TYPE_VENDOR_TITLE_APSKU:
                        obj = pldmpkg.vendor_descriptor_apsku()
                    elif desc['Title'] == DESCRIPTOR_TYPE_VENDOR_TITLE_ECSKU:
                        obj = pldmpkg.vendor_descriptor_ecsku()
                    elif desc['Title'] == DESCRIPTOR_TYPE_VENDOR_TITLE_RECOVERY:
                        obj = pldmpkg.vendor_descriptor_recovery()
                    else:
                        raise Exception('Descriptor vendor type {0} {} not supported yet'.format(typ, desc['Title']))
                else:
                    raise Exception('Descriptor type {0} not supported yet'.format(typ))
                obj.parse(desc)
                self.descriptors += [obj]
            # updaet options
            supported = [0]
            self.update_options = fd['UpdateOptions']
            pldmpkg.check_supported_options('Device Update Options', supported, self.update_options)

            # applicable components
            self.applicable_comps = fd['ApplicableComponents']

            # component image set version
            if 'ImageSetVersion' in fd:
                self.imageset_version = fd['ImageSetVersion']
            # firmware device package
            if 'PackageData' in fd:
                self.packagedata = bytearray.fromhex(fd['PackageData'])

        def size(self):
            length = 2 + 1 + 4 + 1 + 1 + 2 + self.comp_bytes + len(self.imageset_version)
            for d in self.descriptors:
                length += d.size()
            return length + len(self.packagedata)
                     
        def print(self):
            print('descriptor count: {0}'.format(len(self.descriptors)))
            for d in self.descriptors:
                d.print()

        def get_raw(self):
            desc = bytearray()
            for d in self.descriptors:
                r = d.get_raw()
                desc += r

            # support 1 byte of applicable components for now
            if len(self.applicable_comps) != 1:
                raise Exception('support 8 bit applicable components for now')
            mask = bytearray(1)
            for i in self.applicable_comps:
                mask[0] = (1 << i)
            fmt = '<HBIBBHB'
            ret = struct.pack(fmt, self.size(), len(self.descriptors), pldmpkg.bitarray_to_int(self.update_options),
                      STRING_TYPE_ASCII, len(self.imageset_version), len(self.packagedata), mask[0])
            if len(self.imageset_version) != 0:
                ret += bytearray(self.imageset_version.encode('ascii'))
            ret += desc
            ret += self.packagedata
            return ret

    class component_area:
        def __init__(self, images):
            self.components = []
            self.images = images

        def get_comp_count(self):
            return len(self.components)

        def print(self):
            print('component count: {0}'.format(len(self.components)))
            for c in self.components:
                c.print()

        def size(self):
            s = 2
            for c in self.components:
                s += c.size()
            return s

        def get_raw(self):
            r = struct.pack('<H', len(self.components))
            for c in self.components:
                r += c.get_raw()
            return r

        def update_base(self, base):
            for c in self.components:
                c.update_offset(base)
                base += c.get_image_size()

        def parse(self, cfg):
            # Components must exist
            comps = cfg['Components']
            if len(comps) != len(self.images):
                raise Exception('image count not matching component count in cfg file')

            # parse components first to avoid dependency
            for comp, image in zip(comps, self.images):
                c = pldmpkg.component()
                c.parse(comp, image)
                self.components += [c]

    class firmwaredevice_area:
        def __init__(self, num_comps):
            self.num_comps = num_comps
            self.devices = []

        def parse(self, cfg):
            fds = cfg['FirmwareDevices']
            for fd in fds:
                f = pldmpkg.device(self.num_comps)
                f.parse(fd)
                self.devices += [f]

        def print(self):
            print('device count: {0}'.format(len(self.devices)))
            for f in self.devices:
                f.print()
                print('size = {0}'.format(f.size()))

        def size(self):
            s = 1
            for f in self.devices:
                s += f.size()
            return s

        def get_raw(self):
            r = bytearray([len(self.devices)])
            for f in self.devices:
                r += f.get_raw()
            return r

    class header_area:
        def __init__(self, num_comps):
            now = datetime.now()
            time = now.time()
            date = now.date()
            msbytes = time.microsecond.to_bytes(3, byteorder = 'little')

            self.release_date = struct.pack('<hBBBBBBBBHB',
                0, msbytes[0], msbytes[1], msbytes[2], time.second, time.minute, time.hour,
                date.day, date.month, date.year, 0)
            self.comp_bytes = pldmpkg.components_bytes(num_comps)

        def parse(self, cfg, other_area_size):
            self.version = cfg['PackageVersion']
            # 4 is for the checksum at the end of header (after component area) (4 bytes)
            self.whole_header_size = self.size() + other_area_size + 4

        def size(self):
            return 16 + 1 + 2 + len(self.release_date) + 2 + 1 + 1 + len(self.version)

        def get_whole_header_size(self):
            return self.whole_header_size

        def get_raw(self):
            fmt = '<16sBH{0}sHBB{1}s'.format(len(self.release_date), len(self.version))
            r = struct.pack(fmt,
                      bytearray.fromhex(PLDMFW_PACAKGE_UUID),
                      PLDMFW_FORMAT_V1_0_0,
                      self.whole_header_size,
                      self.release_date,
                      self.comp_bytes * 8, # number of bits
                      STRING_TYPE_ASCII,
                      len(self.version),
                      self.version.encode('ascii'))
            return r

    def __init__(self):
        self.comp_area = None
        self.fd_area = None
        self.hd_area = None
        self.images = None

    def parse_component_area(self, cfg, images):
        self.comp_area = pldmpkg.component_area(images)
        self.comp_area.parse(cfg)

    def parse_firmwaredevice_area(self, cfg):
         self.fd_area = pldmpkg.firmwaredevice_area(self.comp_area.get_comp_count())
         self.fd_area.parse(cfg)

    def parse_header_area(self, cfg, other_area_size):
        self.hd_area = pldmpkg.header_area(self.comp_area.get_comp_count())
        self.hd_area.parse(cfg, other_area_size)
        
    def print(self):
        self.hd_area.print()
        self.fd_area.print()
        self.comp_area.print()

    def parse(self, cfg, images):
        # save image file to use when generating package file
        self.images = images

        # componenet must be parsed first for header to use the size info
        self.parse_component_area(cfg, images)
        self.parse_firmwaredevice_area(cfg)
        # header must be parsed in the last place because it needs info from other area
        self.parse_header_area(cfg, self.comp_area.size() + self.fd_area.size())

        # once parsing is done, needs to relocate the image offset
        base = self.hd_area.get_whole_header_size()
        self.comp_area.update_base(base)

    def get_raw(self):
        r = self.hd_area.get_raw()
        r += self.fd_area.get_raw()
        r += self.comp_area.get_raw()
        checksum = binascii.crc32(r)
        r += struct.pack('<I', checksum)
        return r

    def createpkg(self, path):
        fout = open(path, 'w+b')
        r = self.get_raw()
        fout.write(r)
        # now append image
        for f in self.images:
           fin = open(f, 'rb')
           fout.write(fin.read())
           fin.close()

        fout.close()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('cfg', help="path of config file")
    parser.add_argument('images', nargs='+',
        help = 'one or more firmware image paths, must match config file component count')
    parser.add_argument('--pkgver', help = 'override version in package header area')
    parser.add_argument('--compver', help = 'override first component version')
    parser.add_argument('--apsku', help = 'overide ap_sku_id')
    parser.add_argument('--ssdid', help = 'overide ssdid')
    parser.add_argument('--compstamp', help = 'override comparison stamp')
    parser.add_argument('--out', default = 'fwpkg.bin', help = 'specify output file name, fwpkg.bin')

    args = parser.parse_args()

    with open(args.cfg) as f:
        try:
            cfg = json.load(f)
        except BaseException:
            sys.exit('ERROR: invalid JSON format for {0}'.format(args.cfg))

    # override version
    if not args.pkgver is None:
        cfg['PackageVersion'] = args.pkgver

    if not args.compver is None:
        cfg['Components'][0]['Version'] = args.compver
        cfg['FirmwareDevices'][0]['ImageSetVersion'] = args.compver

    if not args.apsku is None: 
        print("AP_SKU: ", args.apsku)
        for i in range(len(cfg['FirmwareDevices'][0]['Descriptors'])):
            if int(cfg['FirmwareDevices'][0]['Descriptors'][i]['Type'], 16) == DESCRIPTOR_TYPE_VENDOR:
                if cfg['FirmwareDevices'][0]['Descriptors'][i]['Title'] == DESCRIPTOR_TYPE_VENDOR_TITLE_APSKU:
                    cfg['FirmwareDevices'][0]['Descriptors'][i]['Data'] = args.apsku

    if not args.ssdid is None:
        print("SSDID: ", args.ssdid)
        for i in range(len(cfg['FirmwareDevices'][0]['Descriptors'])):
            if int(cfg['FirmwareDevices'][0]['Descriptors'][i]['Type'], 16) == DESCRIPTOR_TYPE_PCI_SUBSYS_ID:
                cfg['FirmwareDevices'][0]['Descriptors'][i]['Data'] = args.ssdid

    if not args.compstamp is None:
        print("Comparison Stamp: ", args.compstamp)
        cfg["Components"][0]["ComparisonStamp"] = args.compstamp

    pkg = pldmpkg()
    pkg.parse(cfg, args.images)
    pkg.createpkg(args.out)

if __name__ == "__main__":
    main()
