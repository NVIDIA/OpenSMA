#!/usr/bin/env python
# -*- coding: UTF-8 -*-
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
import subprocess

from spsdk.crypto.signature_provider import SignatureProvider
from spsdk.crypto.keys import PublicKey

# temp folders should go under the cwd, which is a dir under /tmp when called by the signing service
# this ensures any temp files/folders created here get cleaned up by the signing service
TEMP_FOLDER = os.getcwd() + '/mcusign/'

class NvidiaSP(SignatureProvider):
    """NVIDIA SignatureProvider to accommodate the signing process for signing server."""

    # identifier of this signature provider; used in yaml configuration file
    identifier = 'nvsp'

    def tmppath(self, f):
        return os.path.join(TEMP_FOLDER, f)

    def __init__(self, key_number: int, key_type: str, key_label: str, isk_sign: str, key_dir: str, env: str = 'hsm') -> None:
        """Initialize the NVIDIA SignatureProvider.

        :param key_number     : index of the key to use (rot_id from yaml config)
        :param key_type       : key type (e.g. secp256r1, secp384r1, secp521r1)
        :param key_label      : key label (e.g. MCXN236_S0_ECDSA_P384)
        :param isk_sign       : Using image signing key
        :param key_dir        : key directory
        :param env            : hsm or dummy environment, default = hsm
        """
        self.key_number = int(key_number)
        self.key_type = key_type
        self.key_label = key_label
        self.isk_sign = int(isk_sign)
        self.key_dir = key_dir
        self.env = env
        if not os.path.isdir(self.tmppath('')):
            os.mkdir(self.tmppath(''))

    def get_dummy_private_key(self) -> str:
        keyname = self.get_private_key_name()
        return os.path.join(self.key_dir, f'{keyname}_pvt.pem')

    def get_private_key_name(self) -> str:
        assert self.key_type == 'secp384r1'
        if self.isk_sign:
            return f'{self.key_label}_ISK_{self.key_number:02d}'
        else:
            return f'{self.key_label}_ROTK_{self.key_number:02d}'

    def sign(self, data: bytes) -> bytes:
        """Perform the signing.

        :param data: Data to sign
        :return: Signature
        """

        # Write a raw binary for openssl to sign
        rawdata = self.tmppath('rawdata.bin')
        with open(rawdata, 'wb+') as f:
            f.write(data)
            f.close()

        # Sign the data
        sigout = self.tmppath('sigdata.bin')
    
        # Using local key to sign
        if self.env == 'dummy':
            cmd = ["openssl", "dgst", "-sha384", "-sign", self.get_dummy_private_key(), "-out", sigout, rawdata]
        else:
            pvt = self.get_private_key_name()
            raise NotImplementedError("Implementing formal signing method")

        process = subprocess.Popen(
            cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=False)
        stdout, stderr = process.communicate()
        assert process.returncode == 0, stdout + stderr
        with open(sigout, 'rb') as f:
            signed_data = f.read()

        # Remove raw binary file store in tmp folder
        os.system(f'rm -rf {rawdata}')
        os.system(f'rm -rf {sigout}')
        return signed_data

    @property
    def signature_length(self) -> int:
        """Return length of the signature."""
        return {"rsa2048": 256, "secp256r1": 64, "secp384r1": 96, "secp521r1": 132}[self.key_type]

