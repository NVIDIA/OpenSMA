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
import json
from ruamel.yaml import YAML
from spsdk.utils.plugins import PluginsManager
from spsdk.crypto.hash import EnumHashAlgorithm
from spsdk.crypto.keys import PublicKey

class Signer:
    repository = os.path.realpath(os.path.dirname(__file__))

    def __init__(self,
                 workspace: str,
                 env: str,
                 key_dir: str,
                 key_label: str,
                 isk: int = None,
                 rotk: int = None,
                 fw_bin: str = None,
                 cert_block: str = None,
                 enc_key: str = None,
                 outfile: str = None,
                 cust_cfg: str = None,
                 family: str = None,
                 pqc: bool = False,
                 pqc_key_label: str = None) -> None:
        """
        Args:
            env        (str): hsm or dummy
            key_dir    (str): Directory of keys
            key_label  (str): Key label (e.g. MCXN556_S00_ECDSA_P384)
            isk        (int): ISK number for signing, can be None if using ROTK
            rotk       (int): ROTK number for signing, can be None if using ISK
            fw_bin     (str): Location of unsigned firmware binary
            cert_block (str): Location of the certificate block
            outfile    (str): Output location for signed firmware
            cust_cfg   (str): Input config file to overwrite the signing template
        """
        self.workspace = workspace  # Workspace for local signing temporary files
        self.env = env
        self.key_dir = key_dir
        self.key_label = key_label
        self.isk = isk
        self.rotk = rotk
        self.fw_bin = fw_bin
        self.cert_block = cert_block
        self.enc_key = enc_key
        self.outfile = outfile
        self.cust_cfg = cust_cfg
        self.family = family
        self.pqc = pqc
        self.pqc_key_label = pqc_key_label
        self._load_sign_plugin()

    def tmppath(self, f):
        return os.path.join(self.workspace, f)

    def _get_allowed_cust_settings(self) -> list:
        """ Get allowed configuration that can be overwritten by user.
            This method should be overridden by each signer if it supports
            customize fields.

        Returns:
            list: list of the fields
        """
        return []

    def _verify_cust_settings(self, settings: dict) -> bool:
        """ Common validation of customize settings from user

        Args:
            settings (dict): customize settings from user

        Returns:
            bool: True if validation pass, False for failure
        """
        allowlist = self._get_allowed_cust_settings()
        for setting in settings.keys():
            if setting not in allowlist:
                print(f'Invalid settings in customize field: {setting}')
                return False
        return True

    def apply_cust_config(self, config: dict) -> dict:
        """ Apply customize config that input by user, the method
            will make sure the fields are allowed to be overwritten

        Args:
            config (dict): Config file that input by user

        Raises:
            KeyError: Error when there is a disallowed field

        Returns:
            dict: Signing config after applies customize settings
        """
        if not self.cust_cfg:
            return config
        cust_file = open(self.cust_cfg, 'r')
        settings = json.load(cust_file)
        if not self._verify_cust_settings(settings):
            raise KeyError('Invalid settings in customize field')
        for setting in settings.keys():
            config[setting] = settings[setting]
        return config

    def load_template(self, template) -> dict:
        """ Load the template in config folder for signing firmware

        Returns:
            dict: data in the template, can be overwritten later
        """
        template_file = os.path.join(self.repository, 'config', template)
        with open(template_file, 'r') as stream:
            yaml = YAML()
            data = yaml.load(stream)
        return data

    def get_sign_provider(self, key_type: str = 'secp384r1') -> str:
        """ Get sign provider to sign the firmware

        Returns:
            str: sign provider setting for yaml config
        """
        assert key_type in ['secp384r1', 'mldsa87'], f'Invalid key type: {key_type}'
        isk_sign = 1 if self.isk is not None else 0
        key_num = self.isk if isk_sign else self.rotk
        if key_type == 'mldsa87':
            key_label = self.pqc_key_label
        elif key_type == 'secp384r1':
            key_label = self.key_label
        return f'type=nvsp;key_number={key_num};key_label={key_label};env={self.env};' + \
            f'key_type={key_type};isk_sign={isk_sign};key_dir={self.key_dir}'

    def _load_sign_plugin(self) -> None:
        """ Load the plugin for signing
        """
        plugin_file = os.path.join(self.repository, 'plugin', 'nvsp.py')
        PluginsManager().load_from_source_file(plugin_file)

    def get_public_key_name(self) -> str:
        if self.isk is not None:
            return f'{self.key_label}_ISK_{self.isk:02d}'
        else:
            return f'{self.key_label}_ROTK_{self.rotk:02d}'

    def verify_signature(self, pubkey: str, fwdata: str, sigfile: str) -> int:
        with open(fwdata, 'rb') as data_stream:
            data = data_stream.read()
        with open(sigfile, 'rb') as sig_stream:
            signature = sig_stream.read()

        public_key = PublicKey.load(pubkey)
        return 0 if public_key.verify_signature(
            signature,
            data,
            algorithm=EnumHashAlgorithm.SHA384,
        ) else 1

    def get_srk_table_array(self, key_type: str = 'secp384r1') -> list:
        assert key_type in ['secp384r1', 'mldsa87'], f'Invalid key type: {key_type}'
        if key_type == 'secp384r1':
            return [f'{self.key_dir}/{self.key_label}_ROTK_00_pub.pem',
                    f'{self.key_dir}/{self.key_label}_ROTK_01_pub.pem',
                    f'{self.key_dir}/{self.key_label}_ROTK_02_pub.pem',
                    f'{self.key_dir}/{self.key_label}_ROTK_03_pub.pem']
        elif key_type == 'mldsa87':
            return [f'{self.key_dir}/{self.pqc_key_label}_ROTK_00_pub.pem',
                    f'{self.key_dir}/{self.pqc_key_label}_ROTK_01_pub.pem',
                    f'{self.key_dir}/{self.pqc_key_label}_ROTK_02_pub.pem',
                    f'{self.key_dir}/{self.pqc_key_label}_ROTK_03_pub.pem']

class MbiSigner(Signer):
    def _get_allowed_cust_settings(self):
        return ["outputImageExecutionAddress",
                "fuse_version",
                "imageVersion"]

    def get_public_key_name(self) -> str:
        if self.isk is not None:
            return f'{self.key_label}_ISK_{self.isk:02d}_pub.pem'
        return f'{self.key_label}_ROTK_{self.rotk:02d}_pub.pem'

    def verify(self) -> int:
        # Write a raw binary for openssl to sign
        signed_fw_size = os.stat(self.fw_bin).st_size
        sig_zize = 96
        fw_data_size = signed_fw_size - sig_zize

        with open(self.fw_bin, 'rb') as fwfile:
            fwdata = self.tmppath('data.bin')
            sig = self.tmppath('sig.bin')
            with open(fwdata, 'wb+') as f:
                f.write(fwfile.read(fw_data_size))
                f.close()
            with open(sig, 'wb+') as f:
                f.write(fwfile.read(sig_zize))
                f.close()
            fwfile.close()

        # Verify signature
        pubkey = f'{self.key_dir}/{self.get_public_key_name()}'
        return self.verify_signature(pubkey, fwdata, sig)

    def sign(self):
        from spsdk.apps.nxpimage_apps import nxpimage_mbi as nxpimage
        from spsdk.utils.config import Config

        config = self.load_template(f'{self.family}_xip_signed.yaml')
        config['masterBootOutputFile'] = os.path.realpath(self.outfile)
        config['inputImageFile'] = self.fw_bin
        config['signer'] = self.get_sign_provider(key_type='secp384r1')
        config['srk_table']['srk_array'] = self.get_srk_table_array(key_type='secp384r1')

        if self.pqc:
            if 'srk_table_#2' not in config or not isinstance(config['srk_table_#2'], dict):
                config['srk_table']['srk_table_#2'] = {}
            config['srk_table']['srk_table_#2']['srk_array'] = self.get_srk_table_array(key_type='mldsa87')
            config['srk_table']['srk_table_#2']['flag_ca'] = False
            if 'signer_#2' not in config or not isinstance(config['signer_#2'], str):
                config['signer_#2'] = ''
            config['signer_#2'] = self.get_sign_provider(key_type='mldsa87')

        if self.isk is not None:
            config['used_srk_id'] = 0  # Only use ROTK0 to sign ISK
            if self.pqc:
                cert_blk_label = self.pqc_key_label.replace('MLDSA_87', 'PQC')
                config['certificate'] = f'{self.key_dir}/{cert_blk_label}_ISK_{self.isk:02d}_cert_block.bin'
            else:
                config['certificate'] = f'{self.key_dir}/{self.key_label}_ISK_{self.isk:02d}_cert_block.bin'
        else:
            config['used_srk_id'] = self.rotk
        self.apply_cust_config(config)
        # Dump cert block yaml config file
        cfgfile = self.tmppath(f'{self.family}_fw_sign.yaml')
        with open(cfgfile, 'w+') as outstream:
            yaml = YAML()
            yaml.dump(config, outstream)

        # Generate MBI signed firmware
        nxpimage.mbi_export(config=Config.create_from_file(cfgfile))
        return 0


class Sb40Signer(Signer):
    def _get_allowed_cust_settings(self):
        return ["commands", "fuse_version"]

    def get_public_key_name(self) -> str:
        return f'{self.key_label}_ROTK_{self.rotk:02d}_pub.pem'

    def sign(self):
        config = self.load_template('sb40_sign.yaml')
        if self.isk is not None:
            if self.pqc:
                cert_blk_label = self.pqc_key_label.replace('MLDSA_87', 'PQC')
                config['certificate'] = f'{self.key_dir}/{cert_blk_label}_ISK_{self.isk:02d}_cert_block.bin'
            else:
                config['certificate'] = f'{self.key_dir}/{self.key_label}_ISK_{self.isk:02d}_cert_block.bin'
        config['signer'] = self.get_sign_provider()
        config['containerOutputFile'] = os.path.realpath(self.outfile)
        config['srk_table']['srk_array'] = self.get_srk_table_array(key_type='secp384r1')
        config['containerKeyBlobEncryptionKey'] = f'type=file;file_path={os.path.realpath(self.enc_key)}'
        if self.pqc:
            if 'srk_table_#2' not in config or not isinstance(config['srk_table_#2'], dict):
                config['srk_table']['srk_table_#2'] = {}
            config['srk_table']['srk_table_#2']['srk_array'] = self.get_srk_table_array(key_type='mldsa87')
            config['srk_table']['srk_table_#2']['flag_ca'] = False
            if 'signer_#2' not in config or not isinstance(config['signer_#2'], str):
                config['signer_#2'] = ''
            config['signer_#2'] = self.get_sign_provider(key_type='mldsa87')
        self.apply_cust_config(config)

        # Dump cert block yaml config file
        cfgfile = self.tmppath('sb4_config.yaml')
        with open(cfgfile, 'w+') as outstream:
            yaml = YAML()
            yaml.dump(config, outstream)

        from spsdk.apps.nxpimage_apps import nxpimage_sb as nxpimage
        from spsdk.utils.config import Config
        # Generate SB4.0 file
        nxpimage.sb40_export(config=Config.create_from_file(cfgfile))


def parse_args(argv):
    parser = argparse.ArgumentParser()
    subcmd = parser.add_subparsers(dest='operation', required=True)
    # Common arguments
    common_parser = argparse.ArgumentParser(add_help=False)
    common_parser.add_argument('--env',
                               type=str,
                               required=False,
                               default='dummy',
                               choices=['hsm', 'dummy'],
                               help='Environment, default = dummy')
    common_parser.add_argument('--key_dir',
                               type=str,
                               required=True,
                               help='Key set for the keys (e.g. keystore/s0_dummy)')
    common_parser.add_argument('--key_label',
                               type=str,
                               required=True,
                               help='Key label (e.g. MCXN556_S00_ECDSA_P384)')
    common_parser.add_argument('--pqc',
                               required=False,
                               action='store_true',
                               help='Enable PQC signing firmware')
    common_parser.add_argument('--pqc_key_label',
                               type=str,
                               required=False,
                               default=None,
                               help='PQC key label (e.g. MCXN556_S00_MLDSA_87)')
    common_parser.add_argument('--rotk',
                               type=int,
                               required=False,
                               default=None,
                               help='ROTK number for signing firmware')
    common_parser.add_argument('--isk',
                               type=int,
                               required=False,
                               default=None,
                               help='ISK number for signing firmware')
    common_parser.add_argument('--workspace',
                               type=str,
                               required=True,
                               help='workspace of the signing process')
    common_parser.add_argument('--family',
                               type=str,
                               required=False,
                               default='mcxn556s',
                               help='Family of the device',
                               choices=['mcxn23x', 'mcxn547', 'mcxn556s'])

    # Arguments for signing
    sign_parser = subcmd.add_parser('sign',
                                    help='Sign the MCU firmware',
                                    parents=[common_parser])
    sign_parser.add_argument('--outfile',
                             type=str,
                             required=True,
                             help='Output file (signed FW) location')
    sign_parser.add_argument('--fw_bin',
                             type=str,
                             required=True,
                             help='Unsigned firmware binary for signing')
    sign_parser.add_argument('--config',
                             type=str,
                             required=False,
                             default=None,
                             help='JSON format config file')

    # Arguments for verification
    verify_parser = subcmd.add_parser('verify',
                                      help='Verify the signed MCU firmware',
                                      parents=[common_parser])
    verify_parser.add_argument('--fw_bin',
                               type=str,
                               required=True,
                               help='Signed MBI firmware for verification')

    # Arguments for generating SB4.0 file
    gensb40_parser = subcmd.add_parser('gensb40',
                                       help='Generate SB 4.0 file',
                                       parents=[common_parser])
    gensb40_parser.add_argument('--enc_key',
                                type=str,
                                required=True,
                                help='AES encryption key file to encrypt SB blocks')
    gensb40_parser.add_argument('--config',
                                type=str,
                                required=True,
                                help='JSON format config file')
    gensb40_parser.add_argument('--outfile',
                                type=str,
                                required=True,
                                help='Output file (SB4.0) location')
    args = parser.parse_args(argv)

    if args.operation in ['sign', 'verify', 'gensb40']:
        # Either ISK or ROTK should be used for signing
        if not ((args.isk  is not None and args.rotk is None) or \
                (args.rotk is not None and args.isk  is None)):
            raise ValueError('error: Either --isk or --rotk should be provided.')
    return args


def main(argv) -> int:
    args = parse_args(argv)
    if args.operation == 'sign':
        signer = MbiSigner(args.workspace,
                           args.env,
                           args.key_dir,
                           args.key_label,
                           isk=args.isk,
                           rotk=args.rotk,
                           fw_bin=args.fw_bin,
                           cust_cfg=args.config,
                           outfile=args.outfile,
                           family=args.family,
                           pqc=args.pqc,
                           pqc_key_label=args.pqc_key_label)
        rc = signer.sign()
    elif args.operation == 'verify':
        signer = MbiSigner(args.workspace,
                           args.env,
                           args.key_dir,
                           args.key_label,
                           isk=args.isk,
                           rotk=args.rotk,
                           fw_bin=args.fw_bin,
                           family=args.family,
                           pqc=args.pqc,
                           pqc_key_label=args.pqc_key_label)
        rc = signer.verify()
        print('Verification passed.' if rc == 0 else 'Verification failed.')
    elif args.operation == 'gensb40':
        signer = Sb40Signer(args.workspace,
                            args.env,
                            args.key_dir,
                            args.key_label,
                            isk=args.isk,
                            rotk=args.rotk,
                            enc_key=args.enc_key,
                            cust_cfg=args.config,
                            outfile=args.outfile,
                            family=args.family,
                            pqc=args.pqc,
                            pqc_key_label=args.pqc_key_label)
        rc = signer.sign()
    return rc


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
