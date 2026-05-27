#!/usr/bin/env python
# -*- coding: UTF-8 -*-
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


import os

from spsdk.crypto.signature_provider import SignatureProvider
from spsdk.crypto.hash import EnumHashAlgorithm
from spsdk.crypto.keys import PrivateKey

class NvidiaSP(SignatureProvider):
    """NVIDIA SignatureProvider for local signing."""

    # identifier of this signature provider; used in yaml configuration file
    identifier = 'nvsp'

    def __init__(self, key_number: int, key_type: str, key_label: str, isk_sign: str, key_dir: str, env: str = 'hsm', **kwargs) -> None:
        """Initialize the NVIDIA SignatureProvider.

        :param key_number     : index of the key to use (rot_id from yaml config)
        :param key_type       : key type (e.g. secp256r1, secp384r1, secp521r1, mldsa87)
        :param key_label      : key label (e.g. MCXN556_S00_ECDSA_P384)
        :param isk_sign       : Using image signing key
        :param key_dir        : key directory
        :param env            : dummy local signing environment
        """
        self.key_number = int(key_number)
        self.key_type = key_type
        self.key_label = key_label
        self.isk_sign = int(isk_sign)
        self.key_dir = key_dir
        self.env = env

    def get_dummy_private_key(self) -> str:
        keyname = self.get_private_key_name()
        return os.path.abspath(os.path.join(self.key_dir, f'{keyname}_pvt.pem'))

    def get_private_key_name(self) -> str:
        assert self.key_type in ['secp384r1', 'mldsa87']
        if self.isk_sign:
            return f'{self.key_label}_ISK_{self.key_number:02d}'
        else:
            return f'{self.key_label}_ROTK_{self.key_number:02d}'

    def sign(self, data: bytes) -> bytes:
        """Perform the signing.

        :param data: Data to sign
        :return: Signature
        """

        assert self.key_type in ['secp384r1', 'mldsa87']
        if self.env != 'dummy':
            raise NotImplementedError("HSM signing is not supported in this local-sign branch")

        private_key = PrivateKey.load(self.get_dummy_private_key())
        algorithm = EnumHashAlgorithm.SHA512 if self.key_type == 'mldsa87' else EnumHashAlgorithm.SHA384
        signature = private_key.sign(data, algorithm=algorithm)
        if len(signature) != self.signature_length:
            raise RuntimeError(f'Invalid signature length: {len(signature)} != {self.signature_length}')
        return signature

    @property
    def signature_length(self) -> int:
        """Return length of the signature."""
        return {"rsa2048": 256, "secp256r1": 64, "secp384r1": 96, "secp521r1": 132, "mldsa87": 4627}[self.key_type]
