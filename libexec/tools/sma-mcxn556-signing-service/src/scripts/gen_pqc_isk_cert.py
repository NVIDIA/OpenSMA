# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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
from spsdk.utils.plugins import PluginsManager
from spsdk.utils.config import Config
from spsdk.apps.nxpimage_apps.nxpimage_ahab import ahab_cert_block_export

class IskCertGenerator:
    workspace = os.path.realpath(os.path.dirname(__file__))

    def __init__(self,
                 env: str,
                 key_dir: str,
                 ecdsa_label: str,
                 mldsa_label: str,
                 rotk: int,
                 outfile: str,
                 isk: int = None) -> None:
        """
        Args:
            env       (str): Environment of signing, dummy for local signing
            key_dir   (str): Directory of the keys (e.g. keystore/s0_dummy)
            ecdsa_label (str): Key label (e.g. MCXN556_S00_ECDSA_P384)
            mldsa_label (str): Key label (e.g. MCXN556_S00_MLDSA_87)
            rotk      (int): Main ROTK number for signing
            outfile   (str): outfile file for certificate block
            isk       (int, optional): Main ISK number for signing. Defaults to None.
        """
        self.env = env
        self.key_dir = key_dir
        self.ecdsa_label = ecdsa_label
        self.mldsa_label = mldsa_label
        self.rotk = rotk
        self.outfile = outfile
        self.isk = isk
        if not os.path.isdir(self.tmppath('')):
            os.mkdir(self.tmppath(''))

    def tmppath(self, f):
        return os.path.join('/tmp/mcusign', f)

    def load_template(self) -> dict:
        """ Load the template in config folder for generating AHAB certificate

        Returns:
            dict: data in the template, can be overwritten later
        """
        template = os.path.join(self.workspace, 'config', 'pqc_isk_cert.yaml')
        with open(template, 'r') as stream:
            yaml = YAML()
            data = yaml.load(stream)
        return data

    def get_ecdsa_pubkey_location(self, key_num: int) -> str:
        """Locate the ISK public key file in the key store."""
        keystore = os.path.realpath(self.key_dir)
        keyname = f'ISK_{key_num:02d}'
        return os.path.join(keystore, f'{self.ecdsa_label}_{keyname}_pub.pem')

    def get_ecdsa_sign_provider(self) -> str:
        """Signature provider string for signing ISK certificate with ROTK."""
        return (
            f'type=nvsp;key_number={self.rotk};key_label={self.ecdsa_label};'
            f'key_type=secp384r1;isk_sign=0;key_dir={self.key_dir};env={self.env}'
        )

    def get_mldsa_pubkey_location(self, key_num: int) -> str:
        """Locate the ISK public key file in the key store."""
        keystore = os.path.realpath(self.key_dir)
        keyname = f'ISK_{key_num:02d}'
        return os.path.join(keystore, f'{self.mldsa_label}_{keyname}_pub.pem')

    def get_mldsa_sign_provider(self) -> str:
        """Signature provider string for signing ISK certificate with ROTK."""
        return (
            f'type=nvsp;key_number={self.rotk};key_label={self.mldsa_label};'
            f'key_type=mldsa87;isk_sign=0;key_dir={self.key_dir};env={self.env}'
        )

    def get_sign_plugin(self) -> str:
        """Location of the local signature provider plugin."""
        return os.path.join(self.workspace, 'plugin', 'nvsp.py')

    def generate(self) -> None:
        """Generate the AHAB ISK certificate blob."""
        config = self.load_template()
        # Set ISK public key and signer
        assert self.isk is not None, 'ISK key index must be specified for AHAB ISK certificate generation'
        config['public_key_0'] = self.get_ecdsa_pubkey_location(self.isk)
        config['signer_0'] = self.get_ecdsa_sign_provider()
        config['public_key_1'] = self.get_mldsa_pubkey_location(self.isk)
        config['signer_1'] = self.get_mldsa_sign_provider()
        config['fuse_version'] = 1 << (self.isk - 1) if self.isk > 0 else 0

        # Dump ISK certificate yaml config file
        cfgfile = self.tmppath('pqc_isk_cert.yaml')
        with open(cfgfile, 'w+') as outstream:
            yaml = YAML()
            yaml.dump(config, outstream)

        # Load local signature provider plugin and generate AHAB certificate blob
        PluginsManager().load_from_source_file(self.get_sign_plugin())
        cfg = Config.create_from_file(cfgfile)
        # Ensure key directory is in search paths
        if self.key_dir not in cfg.search_paths:
            cfg.search_paths.append(self.key_dir)
        ahab_cert_block_export(cfg, os.path.realpath(self.outfile))


def parse_args(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument('--env',
                        type=str,
                        required=False,
                        default='dummy',
                        choices=['hsm', 'dummy'],
                        help='Environment, default = dummy')
    parser.add_argument('--key_dir',
                        type=str,
                        required=True,
                        help='Key set for the keys (e.g. s0_dummy)')
    parser.add_argument('--ecdsa_label',
                        type=str,
                        required=True,
                        help='Key label (e.g. MCXN556_S00_ECDSA_P384)')
    parser.add_argument('--mldsa_label',
                        type=str,
                        required=True,
                        help='Key label (e.g. MCXN556_S00_MLDSA_87)')
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
    args = parser.parse_args(argv)
    return args


def main(argv):
    args = parse_args(argv)
    generator = IskCertGenerator(args.env, args.key_dir, args.ecdsa_label, args.mldsa_label, args.rotk, args.outfile, args.isk)
    generator.generate()


if __name__ == '__main__':
    main(sys.argv[1:])
