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
import sign
import traceback
import shutil


def get_enc_key(env, keyLabel, keyFile):
    if env != "dummy":
        raise NotImplementedError("HSM key export is not supported in this local-sign branch")

    source = os.path.join(os.getcwd(), "keystore", "s0_dummy", f"{keyLabel}.bin")
    shutil.copyfile(source, keyFile)
    return True

def sign_mbi(env, family, keyDir, keyPrefix, keyIndex, infile, outfile, config=None, signPqc=False, pqcKeyPrefix=None):
    workspace = os.path.dirname(os.path.realpath(infile))
    signArguments = f"sign --workspace {workspace} --env {env} --family {family} --key_dir {keyDir} --key_label {keyPrefix} --isk {keyIndex} --fw_bin {infile} --outfile {outfile}"
    if signPqc:
        signArguments += f' --pqc --pqc_key_label {pqcKeyPrefix}'
    if config:
        mbiCfg = os.path.join(workspace, "mbi_config.json")
        with open(mbiCfg, "w+") as configFile:
            json.dump(config, configFile, indent=4)
        signArguments += f' --config {mbiCfg}'
        print(f'signArguments: {signArguments}')

    signResult = sign.main( signArguments.split() )
    return signResult

    # verifyArguments = f"verify_n556 --workspace {workspace} --env {env} --family {family} --key_dir {keyDir} --key_label {keyPrefix} --isk {keyIndex} --fw_bin {outfile}"
    # verifyResult = sign.main( verifyArguments.split() )
    # return verifyResult

def sign_sb40(env, family, encKeyLabel, keyDir, keyPrefix, keyIndex, infile, outfile, config=None, signPqc=False, pqcKeyPrefix=None):
    workspace = os.path.dirname(os.path.realpath(infile))
    DEFAULT_SB4_CFG = {
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
    if config:
        cfgjson = dict(config)
    else:
        cfgjson = DEFAULT_SB4_CFG.copy()
    # Create config file for gensb40
    gensb40Cfg = os.path.join(workspace, "gensb40_config.json")
    with open(gensb40Cfg, "w+") as configFile:
        json.dump(cfgjson, configFile, indent=4)
    #       to program signedMbi

    encKey = os.path.join(os.getcwd(), "cust_mk_sk.bin")
    if not get_enc_key(env, encKeyLabel, encKey):
        sys.exit('ERROR: failed to export encryption key')

    gensb40Arguments = f"gensb40 --enc_key {encKey} --config {gensb40Cfg} --workspace {workspace} --env {env} --key_dir {keyDir} --key_label {keyPrefix} --isk {keyIndex} --outfile {outfile}"
    if signPqc:
        gensb40Arguments += f' --pqc --pqc_key_label {pqcKeyPrefix}'
    # Catch exception to make sure the key is removed
    try:
        gensb40Result = sign.main( gensb40Arguments.split() )
    except Exception as err:
        traceback.print_exc()
        gensb40Result = 2
    os.remove(encKey)
    return gensb40Result

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

def process_signing(args, params, policy, config) -> int:
    family = config.get("Family", "mcxn556s")
    env = policy["Environment"][args.environment]["EnvType"]
    keyDir = os.path.join(os.getcwd(), policy["Environment"][args.environment]["KeyDir"])
    keyPrefix = config["KeyPrefix"]
    keyIndex = config["KeyIndex"]

    jobType = config["JobType"]

    if jobType in ["SMA_MCXN556_DEBUG_S0", "SMA_MCXN556_PROD_S0", "SMA_MCXN556_PQC_DEBUG_S0", "SMA_MCXN556_PQC_PROD_S0"]:
        signMbi   = params.get("mbi", True)     # Default to enable MBI sign
        genSb4    = params.get("sb4", False)    # Default to disable SB40 gen

        if jobType in ["SMA_MCXN556_PQC_DEBUG_S0", "SMA_MCXN556_PQC_PROD_S0"]:
            signPqc = True
            pqcKeyPrefix = config["PqcKeyPrefix"]
        else:
            signPqc = False
            pqcKeyPrefix = None

        if signMbi:
            mbiconfig = params.get("mbiconfig", None)
            returncode = sign_mbi(env, family, keyDir, keyPrefix, keyIndex, args.inputFile, args.resultFile, mbiconfig, signPqc, pqcKeyPrefix)
            if returncode:
                return returncode

        if genSb4:
            infile = args.inputFile
            sb4config = params.get("sb4config", None)
            if signMbi:
                # Rename signed firmware to use as input to gensb40
                workspace = os.path.dirname(os.path.realpath(infile))
                signedMbi = os.path.join(workspace, "signed_mbi.bin")
                os.rename(args.resultFile, signedMbi)
                infile = signedMbi
            returncode = sign_sb40(env, family, config["CustMkSk"], keyDir, keyPrefix, keyIndex, infile, args.resultFile, sb4config, signPqc, pqcKeyPrefix)
            if returncode:
                return returncode

    return returncode

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
    config.update(config["KeySets"][0])

    family = config.get("Family", "mcxn556s")
    return process_signing(args, params, policy, config)

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
