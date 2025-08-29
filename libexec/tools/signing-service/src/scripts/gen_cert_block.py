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
import os
import argparse
from ruamel.yaml import YAML
from spsdk.apps import nxpimage

# temp folders should go under the cwd, which is a dir under /tmp when called by the signing service
# this ensures any temp files/folders created here get cleaned up by the signing service
TEMP_FOLDER = os.getcwd() + '/mcusign/'

class CertBlockGenerator:
    workspace = os.path.realpath(os.path.dirname(__file__))

    def __init__(self,
                 env: str,
                 key_dir: str,
                 key_label: str,
                 rotk: int,
                 outfile: str,
                 isk: int = None,
                 family: str = 'mcxn23x') -> None:
        """
        Args:
            env       (str): Environment of signing, hsm or dummy
            key_dir   (str): Directory of the keys (e.g. keystore/s0_dummy)
            key_label (str): Key label (e.g. MCXN236_S0_ECDSA_P384)
            rotk      (int): Main ROTK number for signing
            outfile   (str): outfile file for certificate block
            isk       (int, optional): Main ISK number for signing. Defaults to None.
        """
        self.env = env
        self.key_dir = key_dir
        self.key_label = key_label
        self.rotk = rotk
        self.outfile = outfile
        self.isk = isk
        self.family = family
        if not os.path.isdir(TEMP_FOLDER):
            os.mkdir(TEMP_FOLDER)

    def tmppath(self, f):
        return os.path.join(TEMP_FOLDER, f)

    def load_template(self) -> dict:
        """ Load the template in config folder for generating cert block

        Returns:
            dict: data in the template, can be overwritten later
        """
        template = os.path.join(self.workspace, 'config', 'cert_block.yaml')
        with open(template, 'r') as stream:
            yaml = YAML()
            data = yaml.load(stream)
        return data

    def get_pubkey_location(self, key_num: int, isk: bool = False) -> str:
        """ Locate the key file in the key store

        Args:
            key_num (int): Key number
            isk (bool, optional): Get ISK (Image signing key) or ROTK. Defaults to False.

        Returns:
            str: location of the key file
        """
        keystore = os.path.realpath(self.key_dir)
        keyname = f'ISK_{key_num:02d}' if isk else f'ROTK_{key_num:02d}'
        return os.path.join(keystore, f'{self.key_label}_{keyname}_pub.pem')

    def get_isk_sign_provider(self) -> str:
        """ Get ISK sign provider to sign the ISK certificate

        Returns:
            str: sign provider setting for yaml config
        """
        return f'type=nvsp;key_number={self.rotk};key_label={self.key_label};' + \
               f'key_type=secp384r1;isk_sign=0;key_dir={self.key_dir};env={self.env}'

    def get_sign_plugin(self) -> str:
        """ Locate the plugin for signing

        Returns:
            str: Location of the plugin file
        """
        return os.path.join(self.workspace, 'plugin', 'nvsp.py')

    def generate(self) -> None:
        """ Generate the certificate block """
        config = self.load_template()

        config['mainRootCertId'] = self.rotk
        config['rootCertificate0File'] = self.get_pubkey_location(0)
        config['rootCertificate1File'] = self.get_pubkey_location(1)
        config['rootCertificate2File'] = self.get_pubkey_location(2)
        config['rootCertificate3File'] = self.get_pubkey_location(3)
        config['containerOutputFile'] = os.path.realpath(self.outfile)

        # Additional fields for using ISK
        if self.isk is not None:
            config['useIsk'] = True
            # Convert key number to bitmask for key contraints
            # (unary encoded counter, e.g. 0 -> 0000, 1 -> 0001, 2 -> 0011, 3 -> 0111)
            config['iskCertificateConstraint'] = (1 << self.isk) - 1
            config['iskPublicKey'] = self.get_pubkey_location(self.isk, isk=True)
            config['signProvider'] = self.get_isk_sign_provider()
        else:
            config['useIsk'] = False

        # Dump cert block yaml config file
        cfgfile = self.tmppath('cert_block.yaml')
        with open(cfgfile, 'w+') as outstream:
            yaml = YAML()
            yaml.dump(config, outstream)

        # Generate cert block
        nxpimage.cert_block_export(config=cfgfile,
                                   family=self.family,
                                   plugin=self.get_sign_plugin())


def parse_args(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument('--env',
                        type=str,
                        required=False,
                        default='hsm',
                        choices=['hsm', 'dummy'],
                        help='Environment, default = hsm')
    parser.add_argument('--key_dir',
                        type=str,
                        required=True,
                        help='Key set for the keys (e.g. s0_dummy)')
    parser.add_argument('--key_label',
                        type=str,
                        required=True,
                        help='Key label (e.g. MCXN236_S0_ECDSA_P384)')
    parser.add_argument('--rotk',
                        type=int,
                        required=True,
                        help='Main key number of the ROTK')
    parser.add_argument('--isk',
                        type=int,
                        required=False,
                        help='Main key number of the ISK. If this is set, it will sign the certificate block for ISK')
    parser.add_argument('--outfile',
                        type=str,
                        required=True,
                        help='outfile file for certificate block')
    parser.add_argument('--family',
                        type=str,
                        required=False,
                        default='mcxn23x',
                        help='Family of the device')
    args = parser.parse_args(argv)
    return args


def main(argv):
    args = parse_args(argv)
    generator = CertBlockGenerator(args.env, args.key_dir, args.key_label, args.rotk, args.outfile, args.isk)
    generator.generate()


if __name__ == '__main__':
    main(sys.argv[1:])