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
import sign
import traceback


def get_enc_key(keyLabel, keyFile, env):
    if env == 'dummy':
        import shutil
        shutil.copyfile(f'keystore/s0_dummy/{keyLabel}.bin', keyFile)
        return True
    else:
        raise NotImplementedError("Implementing formal signing process")


def sign_mbi(env, family, keyDir, keyPrefix, keyIndex, certBlock, infile, outfile, config=None):
    workspace = os.path.dirname(os.path.realpath(infile))
    signArguments = f"sign --workspace {workspace} --env {env} --family {family} --key_dir {keyDir} --key_label {keyPrefix} --isk {keyIndex} --cert_block {certBlock} --fw_bin {infile} --outfile {outfile}"

    # verify with public key doesn't require HSM, hence env hardcoded to dummy here
    verifyArguments = f"verify --workspace {workspace} --env dummy --family {family} --key_dir {keyDir} --key_label {keyPrefix} --isk {keyIndex} --fw_bin {outfile}"

    if config:
        mbiCfg = os.path.join(workspace, "mbi_config.json")
        with open(mbiCfg, "w+") as configFile:
            json.dump(config, configFile, indent=4)
        signArguments += f' --config {mbiCfg}'

    signResult = sign.main( signArguments.split() )
    if signResult:
        return signResult

    verifyResult = sign.main( verifyArguments.split() )
    return verifyResult


def sign_sb31(env, family, encKeyLabel, keyDir, keyPrefix, keyIndex, certBlock, infile, outfile, config=None):
    workspace = os.path.dirname(os.path.realpath(infile))
    DEFAULT_SB3_CFG = {
        "commands": [
            {
                "checkFwVersion": {
                    "value": 0,
                    "counterId": "secure"
                }
            },
            {
                "erase": {
                    "address": 0,
                    "size": "0x00060000"
                }
            },
            {
                "load": {
                    "address": 0,
                    "file": infile
                }
            }
        ]
    }
    cfgjson = config if config else DEFAULT_SB3_CFG
    # Create config file for gensb31
    gensb31Cfg = os.path.join(workspace, "gensb31_config.json")
    with open(gensb31Cfg, "w+") as configFile:
        json.dump(cfgjson, configFile, indent=4)
    #       to program signedMbi

    encKey = os.path.join(os.getcwd(), "cust_mk_sk.bin")
    if not get_enc_key(encKeyLabel, encKey, env):
        sys.exit('ERROR: failed to export encryption key')

    gensb3Arguments = f"gensb31 --enc_key {encKey} --config {gensb31Cfg} --workspace {workspace} --env {env} --family {family} --key_dir {keyDir} --key_label {keyPrefix} --isk {keyIndex} --cert_block {certBlock} --outfile {outfile}"

    # Catch exception to make sure the key is removed
    try:
        gensb3Result = sign.main( gensb3Arguments.split() )
    except Exception as err:
        traceback.print_exc()
        gensb3Result = 2
    os.remove(encKey)
    return gensb3Result


def parse_args(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument('--inputFile',
                        type=str,
                        required=True,
                        help='Input file from request')
    parser.add_argument('--resultFile',
                        type=str,
                        required=True,
                        help='Signed result file to create')
    parser.add_argument('--parameters',
                        type=str,
                        required=False,
                        help='JSON file with request and job type parameters')
    parser.add_argument('--environment',
                        type=str,
                        required=False,
                        default='Dummy',
                        choices=['Development', 'Production', 'Dummy'],
                        help='Signing environment')
    parser.add_argument('--jobType',
                        type=str,
                        required=True,
                        help='Signing job type')

    args = parser.parse_args(argv)
    return args


def main(argv) -> int:
    args = parse_args(argv)

    params = {}
    if args.parameters:
        paramFile = args.parameters
        if os.path.getsize(paramFile) < 2:
            print('INFO: parameters file size {0}'.format(os.path.getsize(paramFile)))
        else:
            with open(paramFile) as f:
                try:
                    params = json.load(f)
                except BaseException:
                    sys.exit('ERROR: invalid json format for {0}'.format(paramFile))

    policyFile = os.path.join(os.getcwd(), "policy.json")
    with open(policyFile) as f:
        try:
            policy = json.load(f)
        except BaseException:
            sys.exit('ERROR: invalid json format for {0}'.format(policyFile))

    config = next((x for x in policy["Configs"] if x["JobType"] == args.jobType), None)

    if config is None:
        sys.exit('ERROR: {0} not found in policy configuration'.format(args.jobType))

    family = config.get("Family", "mcxn23x")

    if "KeySets" in config:
        keySet = params.get("keySet", None)
        if keySet is None:
            sys.exit('ERROR: \'keySet\' not found in request parameters')
        keySetCfg = next((x for x in config["KeySets"] if x["KeySet"] == keySet.lower()), None)
        if keySetCfg is None:
            sys.exit('ERROR: {0} not found in policy keysets configuration'.format(keySet.lower()))
        config.update(keySetCfg)

    env = policy["Environment"][args.environment]["EnvType"]
    keyDir = os.path.join(os.getcwd(), policy["Environment"][args.environment]["KeyDir"])

    certBlock = os.path.join(os.getcwd(), keyDir, config["CertBlock"])

    if not os.path.isfile(certBlock):
        sys.exit('ERROR: No cert block found at {0}'.format(certBlock))

    keyPrefix = config["KeyPrefix"]
    keyIndex = config["KeyIndex"]

    returncode = 0
    signMbi   = params.get("mbi", True)     # Default to enable MBI sign
    genSb3    = params.get("sb3", False)    # Default to disable SB31 gen

    if signMbi:
        mbiconfig = params.get("mbiconfig", None)
        returncode = sign_mbi(env, family, keyDir, keyPrefix, keyIndex, certBlock, args.inputFile, args.resultFile, mbiconfig)
        if returncode:
            return returncode

    if genSb3:
        infile = args.inputFile
        sb3config = params.get("sb3config", None)
        if signMbi:
            # Rename signed firmware to use as input to gensb31
            workspace = os.path.dirname(os.path.realpath(infile))
            signedMbi = os.path.join(workspace, "signed_mbi.bin")
            os.rename(args.resultFile, signedMbi)
            infile = signedMbi
        returncode = sign_sb31(env, family, config["CustMkSk"], keyDir, keyPrefix, keyIndex, certBlock, infile, args.resultFile, sb3config)
        if returncode:
            return returncode

    return returncode

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
