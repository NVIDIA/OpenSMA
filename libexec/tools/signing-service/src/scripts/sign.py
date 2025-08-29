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
import subprocess
import json
from ruamel.yaml import YAML
from spsdk.apps import nxpimage
import shutil

class Signer:
    repository = os.path.realpath(os.path.dirname(__file__))

    def __init__(self,
                 workspace: str,
                 env: str,
                 key_dir: str,
                 key_label: str,
                 isk: int = None,
                 rotk: int = None,
                 assetkey: int = None,
                 fw_bin: str = None,
                 cert_block: str = None,
                 enc_key: str = None,
                 outfile: str = None,
                 cust_cfg: str = None,
                 family: str = 'mcxn23x') -> None:
        """
        Args:
            env        (str): hsm or dummy
            key_dir    (str): Directory of keys
            key_label  (str): Key label (e.g. MCXN236_S0_ECDSA_P384)
            isk        (int): ISK number for signing, can be None if using ROTK
            rotk       (int): ROTK number for signing, can be None if using ISK
            fw_bin     (str): Location of unsigned firmware binary
            cert_block (str): Location of the certificate block
            outfile    (str): Output location for signed firmware
            cust_cfg   (str): Input config file to overwrite the signing template
        """
        self.workspace = workspace  # Workspace should under the /tmp when it's called by signing service
        self.env = env
        self.key_dir = key_dir
        self.key_label = key_label
        self.isk = isk
        self.rotk = rotk
        self.assetkey = assetkey
        self.fw_bin = fw_bin
        self.cert_block = cert_block
        self.enc_key = enc_key
        self.outfile = outfile
        self.cust_cfg = cust_cfg
        self.family = family

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

    def get_sign_provider(self) -> str:
        """ Get sign provider to sign the firmware

        Returns:
            str: sign provider setting for yaml config
        """
        isk_sign = 1 if self.isk is not None else 0
        key_num = self.isk if isk_sign else self.rotk
        return f'type=nvsp;key_number={key_num};key_label={self.key_label};env={self.env};' + \
               f'key_type=secp384r1;isk_sign={isk_sign};key_dir={self.key_dir}'

    def get_sign_plugin(self) -> str:
        """ Locate the plugin for signing

        Returns:
            str: Location of the plugin file
        """
        return os.path.join(self.repository, 'plugin', 'nvsp.py')

    def get_public_key_name(self) -> str:
        if self.isk is not None:
            return f'{self.key_label}_ISK_{self.isk:02d}'
        else:
            return f'{self.key_label}_ROTK_{self.rotk:02d}'

    def verify_signature(self, pubkey: str, fwdata: str, sigfile: str) -> int:
        if self.env == 'dummy':
            return self.verify_signature_openssl(pubkey, fwdata, sigfile)

        # Verify signature
        cmd = ["shu", "ecdsa", "-k", pubkey, '-o', 'verify', '-f', 'raw', '-d', 'sha384', '--infile', fwdata, '--sigfile', sigfile, '--localkeyfile', pubkey]

        process = subprocess.Popen(
            cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=False)
        stdout, stderr = process.communicate()
        assert process.returncode == 0, stdout + stderr
        response = json.loads(stdout.decode())
        return 0 if response['isValidSignature'] else 1

    def verify_signature_openssl(self, pubkey: str, fwdata: str, sigfile: str) -> int:
        # Read the signature file
        with open(sigfile, "rb") as f:
            raw_sig = f.read()

        # Check if it's raw format (96 bytes for secp384r1)
        if len(raw_sig) == 96:
            # Convert raw r,s format to DER format for OpenSSL verification
            r_bytes = raw_sig[:48]  # First 48 bytes are r
            s_bytes = raw_sig[48:]  # Next 48 bytes are s

            # Remove leading zeros but keep at least one byte
            r_bytes = r_bytes.lstrip(b"\x00") or b"\x00"
            s_bytes = s_bytes.lstrip(b"\x00") or b"\x00"

            # Add leading zero if high bit is set (DER requirement)
            if r_bytes[0] & 0x80:
                r_bytes = b"\x00" + r_bytes
            if s_bytes[0] & 0x80:
                s_bytes = b"\x00" + s_bytes

            # Create DER signature
            der_sigfile = sigfile + ".der"
            r_der = b"\x02" + bytes([len(r_bytes)]) + r_bytes
            s_der = b"\x02" + bytes([len(s_bytes)]) + s_bytes
            full_der = b"\x30" + bytes([len(r_der + s_der)]) + r_der + s_der

            with open(der_sigfile, "wb") as f:
                f.write(full_der)
            sig_to_verify = der_sigfile
        else:
            # Already in DER format
            sig_to_verify = sigfile

        # Verify signature using OpenSSL
        cmd = ["openssl", "dgst", "-sha384", "-verify", pubkey, "-signature", sig_to_verify, fwdata]

        process = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=False)
        _, stderr = process.communicate()

        # Clean up temporary DER file if created
        if len(raw_sig) == 96:
            try:
                os.remove(der_sigfile)
            except FileNotFoundError:
                pass

        # Debug output if verification fails
        if process.returncode != 0:
            print(f"OpenSSL verification failed: {stderr.decode()}")

        # OpenSSL returns 0 for valid signature, 1 for invalid signature
        return process.returncode


class MbiSigner(Signer):
    def _get_allowed_cust_settings(self):
        return ["outputImageExecutionAddress",
                "firmwareVersion",
                "imageVersion"]

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
        pubkey = f'{self.key_dir}/{self.get_public_key_name()}_pub.pem'
        return self.verify_signature(pubkey, fwdata, sig)

    def sign(self) -> int:
        """ Sign the firmware

        Args:
            output (str, optional): File name of the output. Defaults to 'mbi_signed_mcxn236_fw.bin'.
        """
        config = self.load_template(f'{self.family}_xip_signed.yaml')
        config['masterBootOutputFile'] = os.path.realpath(self.outfile)
        config['inputImageFile'] = self.fw_bin
        config['certBlock'] = os.path.realpath(self.cert_block)
        config['signProvider'] = self.get_sign_provider()
        if self.family == 'mcxn547':
            config['enableTrustZone'] = False
        else:
            config['enableTrustZone'] = True
            config['trustZonePresetFile'] = os.path.join(self.repository, 'config', 'tz_preset.yaml')
        self.apply_cust_config(config)

        # Dump cert block yaml config file
        cfgfile = self.tmppath(f'{self.family}_fw_sign.yaml')
        with open(cfgfile, 'w+') as outstream:
            yaml = YAML()
            yaml.dump(config, outstream)

        # Generate MBI signed firmware
        nxpimage.mbi_export(config=cfgfile,
                            plugin=self.get_sign_plugin())
        return 0


class Sb31Signer(Signer):
    def _get_allowed_cust_settings(self):
        return ["commands"]

    def sign(self):
        config = self.load_template(f'{self.family}_sb3.yaml')
        config['certBlock'] = os.path.realpath(self.cert_block)
        config['signProvider'] = self.get_sign_provider()
        config['containerOutputFile'] = os.path.realpath(self.outfile)
        config['containerKeyBlobEncryptionKey'] = os.path.realpath(self.enc_key)
        self.apply_cust_config(config)

        # Dump cert block yaml config file
        cfgfile = self.tmppath(f'{self.family}_sb31_sign.yaml')
        with open(cfgfile, 'w+') as outstream:
            yaml = YAML()
            yaml.dump(config, outstream)

        # Generate SB3.1 file
        nxpimage.sb31_export(config=cfgfile,
                             plugin=self.get_sign_plugin())

def parse_args(argv):
    parser = argparse.ArgumentParser()
    subcmd = parser.add_subparsers(dest='operation', required=True)
    # Common arguments
    common_parser = argparse.ArgumentParser(add_help=False)
    common_parser.add_argument('--env',
                               type=str,
                               required=False,
                               default='hsm',
                               choices=['hsm', 'dummy'],
                               help='Environment, default = hsm')
    common_parser.add_argument('--key_dir',
                               type=str,
                               required=True,
                               help='Key set for the keys (e.g. keystore/s0_dummy)')
    common_parser.add_argument('--key_label',
                               type=str,
                               required=True,
                               help='Key label (e.g. MCXN236_S0_ECDSA_P384)')
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
                               default='mcxn23x',
                               help='Family of the device',
                               choices=['mcxn23x', 'mcxn547'])

    # Arguments for signing
    sign_parser = subcmd.add_parser('sign',
                                    help='Sign the MCU firmware',
                                    parents=[common_parser])
    sign_parser.add_argument('--cert_block',
                             type=str,
                             required=True,
                             help='Certificate block for signing')
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

    # Arguments for generating SB3.1 file
    gensb31_parser = subcmd.add_parser('gensb31',
                                       help='Generate SB 3.1 file',
                                       parents=[common_parser])
    gensb31_parser.add_argument('--cert_block',
                                type=str,
                                required=True,
                                help='Certificate block for signing')
    gensb31_parser.add_argument('--enc_key',
                                type=str,
                                required=True,
                                help='AES encryption key file to encrypt SB blocks')
    gensb31_parser.add_argument('--config',
                                type=str,
                                required=True,
                                help='JSON format config file')
    gensb31_parser.add_argument('--outfile',
                                type=str,
                                required=True,
                                help='Output file (SB3.1) location')

    args = parser.parse_args(argv)

    if args.operation in ['sign', 'verify', 'gensb31']:
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
                           cert_block=args.cert_block,
                           outfile=args.outfile,
                           family=args.family)
        rc = signer.sign()
    elif args.operation == 'verify':
        signer = MbiSigner(args.workspace,
                           args.env,
                           args.key_dir,
                           args.key_label,
                           isk=args.isk,
                           rotk=args.rotk,
                           fw_bin=args.fw_bin,
                           family=args.family)
        rc = signer.verify()
        print('Verification passed.' if rc == 0 else 'Verification failed.')
    elif args.operation == 'gensb31':
        signer = Sb31Signer(args.workspace,
                            args.env,
                            args.key_dir,
                            args.key_label,
                            isk=args.isk,
                            rotk=args.rotk,
                            enc_key=args.enc_key,
                            cert_block=args.cert_block,
                            cust_cfg=args.config,
                            outfile=args.outfile,
                            family=args.family)
        rc = signer.sign()
    return rc


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))