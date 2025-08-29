#!/bin/bash
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

set -e 

keyset=$1

usage() {
    echo "Usage:"
    echo "  $(basename $0) <keyset>"
    echo ""
    echo "Args:"
    echo "  keyset: int number of the keyset"
}

if [ -z $keyset ] || [[ $keyset != +([[:digit:]]) ]]; then
    usage
    exit 1
fi

echo "Generate ROT keys"
for idx in $(seq 0 3)
do
    keyname="MCXN236_S${keyset}_ECDSA_P384_ROTK_0${idx}"
    openssl ecparam -genkey -name secp384r1 -noout -out ${keyname}_pvt.pem
    openssl ec -in ${keyname}_pvt.pem -pubout -out ${keyname}_pub.pem
done

echo "Generate Image Signing Key"
for idx in 00 01 99
do
    keyname="MCXN236_S${keyset}_ECDSA_P384_ISK_${idx}"
    openssl ecparam -genkey -name secp384r1 -noout -out ${keyname}_pvt.pem
    openssl ec -in ${keyname}_pvt.pem -pubout -out ${keyname}_pub.pem
done
