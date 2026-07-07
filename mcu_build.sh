#!/usr/bin/bash

# print the usage of the script
for arg in "$@"; do
    if [[ "$arg" == "-h" || "$arg" == "--help" || "$arg" == "help" ]]; then
        echo "Usage: $0 [PROJECT=project] [PLATFORM=platform] [BOARD=board] [MODE=mode] [SIGN_KEYSET=keyset] [VERSION=version] [STREAMBOOT=1] [RUN_LOCAL=1] [BUILD_ONLY=1]"
        echo "PROJECT: project name (for example: pg540)"
        echo "PLATFORM: platform name (for example: mcxn236)"
        echo "BOARD: board name (for example: pg540). For px9me/pd4a4/pd4b4 the FRDM-MCXN947 devkit variant is BOARD=devkit."
        echo "MODE: mode name (for example: dev)"
        echo "SIGN_KEYSET: keyset name (for example: S0). If using local signing, use 'local' or 'local-debug' for debug signing, or 'local-prod' for prod sign."
        echo "VERSION: firmware version (for example: 02.0010.0000)"
        echo "STREAMBOOT: set to 1 to generate multi-binary files (for example: STREAMBOOT=1)"
        echo "RUN_LOCAL: set to 1 to execute local without docker (for example: RUN_LOCAL=1)"
        echo "BUILD_ONLY: set to 1 to only build without signing and downloading (for example: BUILD_ONLY=1)"
        exit 0
    fi
done

for ARG in "$@"; do
    case $ARG in
        PROJECT=*) PROJECT="${ARG#*=}" ;;
        PLATFORM=*) PLATFORM="${ARG#*=}" ;;
        BOARD=*) BOARD="${ARG#*=}" ;;
        MODE=*) MODE="${ARG#*=}" ;;
        SIGN_KEYSET=*) SIGN_KEYSET="${ARG#*=}" ;;
        VERSION=*) VERSION="${ARG#*=}" ;;
        CODE_GROUP_INDEX=*) CODE_GROUP_INDEX="${ARG#*=}" ;;
        STREAMBOOT=*) STREAMBOOT="${ARG#*=}" ;;
        RUN_LOCAL=*) RUN_LOCAL="${ARG#*=}" ;;
        BUILD_ONLY=*) BUILD_ONLY="${ARG#*=}" ;;
        *) echo "Unknown argument: $ARG"; exit 1 ;;
    esac
done

PROJECT="${PROJECT:-$PROJECT_ENV}"
PLATFORM="${PLATFORM:-$PLATFORM_ENV}"
BOARD="${BOARD:-$BOARD_ENV}"
MODE="${MODE:-$MODE_ENV}"
SIGN_KEYSET="${SIGN_KEYSET:-$SIGN_KEYSET_ENV}"
VERSION="${VERSION:-$VERSION_ENV}"
CODE_GROUP_INDEX="${CODE_GROUP_INDEX:-$CODE_GROUP_INDEX_ENV}"
BUILD_ONLY="${BUILD_ONLY:-$BUILD_ONLY_ENV}"
export MCU_TOOLCHAIN_ROOT="${MCU_TOOLCHAIN_ROOT:-/toolchain}"
UBS_DOCKER="${UBS_DOCKER:-gitlab-master.nvidia.com:5005/gfw/chips/mcu/mcxn236/ubs-556:latest}"

run_objcopy() {
    local input_file="$1"
    local output_file="$2"
    local objcopy="${MCU_TOOLCHAIN_ROOT}/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi/bin/arm-none-eabi-objcopy"

    if [[ -f /.dockerenv ]]; then
        "$objcopy" -O binary "$input_file" "$output_file"
    else
        echo "Running objcopy in UBS docker image $UBS_DOCKER"
        docker run --rm --user="$(id -u):$(id -g)" -v"$PWD:/tmp/ubs" -w/tmp/ubs "$UBS_DOCKER" \
            "$objcopy" -O binary "$input_file" "$output_file"
    fi
}

run_build_flow() {
    local platform="$1"
    echo "==== Starting build for $platform ===="

    local env_vars=(
        PROJECT="$PROJECT"
        PLATFORM="$platform"
        BOARD="$BOARD"
        MODE="$MODE"
        VERSION="$VERSION"
    )

    if [[ -n "$SIGN_KEYSET" ]]; then
        env_vars+=(SIGN_KEYSET="$SIGN_KEYSET")
    fi

    if [[ -n "$CODE_GROUP_INDEX" ]]; then
        env_vars+=(CODE_GROUP_INDEX="$CODE_GROUP_INDEX")
    fi

    if [[ -n "$RUN_LOCAL" ]]; then
        env_vars+=(RUN_LOCAL="$RUN_LOCAL")
    fi

    if [[ -n "$BUILD_ONLY" ]]; then
        env_vars+=(BUILD_ONLY="$BUILD_ONLY")
    fi

    env "${env_vars[@]}" ./mcu_build.sh || exit 1
    
    return 0
}

# Generate individual core platforms from "both" platforms
if [[ "$PLATFORM" == "mcxn547-both" || "$PLATFORM" == "mcxn556-both" ]]; then
    # Extract base platform name
    if [[ "$PLATFORM" == "mcxn547-both" ]]; then
        BASE_PLATFORM="mcxn547"
    elif [[ "$PLATFORM" == "mcxn556-both" ]]; then
        BASE_PLATFORM="mcxn556"
    else
        echo "Error: Invalid platform $PLATFORM"
        exit 1
    fi

    CORE0_PLATFORM="${BASE_PLATFORM}-core0"
    CORE1_PLATFORM="${BASE_PLATFORM}-core1"

    run_build_flow "$CORE1_PLATFORM"

    REAL_PROJECT="${PROJECT%-coverage}" # this is a workaround to get project name without -coverage suffix
    CORE1_ELF="build/${PROJECT}-${CORE1_PLATFORM}-${MODE}/${REAL_PROJECT}.elf"
    CORE1_BIN="build/${PROJECT}-${CORE1_PLATFORM}-${MODE}/${REAL_PROJECT}.bin"
    CORE1_READELF="build/${PROJECT}-${CORE1_PLATFORM}-${MODE}/${REAL_PROJECT}.readelf"
    CORE1_C="src/projects/${REAL_PROJECT}/core0/core1_image.c"
    CORE1_LD="src/projects/${REAL_PROJECT}/core0/sys/${BASE_PLATFORM}/generated_core1.ld"

    # Generate binary file from ELF
    if [[ -f "$CORE1_ELF" ]]; then
        run_objcopy "$CORE1_ELF" "$CORE1_BIN" 2>/dev/null
        if [[ $? -eq 0 ]]; then
            echo "Binary file generated successfully: $CORE1_BIN"
        else
            echo "Error: Failed to generate binary file $CORE1_BIN"
        fi
    else
        echo "Warning: ELF file not found: $CORE1_ELF"
    fi

    echo "Generating $CORE1_C from $CORE1_BIN..."
    xxd -i "$CORE1_BIN" > "$CORE1_C"
    sed -i \
    -e 's/unsigned char .*_bin\[\]/__attribute__((section(".core1_image"))) const unsigned char core1_bin[]/' \
    -e 's/unsigned int .*_bin_len/unsigned int core1_bin_len/' \
    "$CORE1_C"

    extract_symbol() {
        local symbol=$1
        local addr=$(grep "$symbol" "$CORE1_READELF" | awk '{ print "0x" $2 }')
        if [[ -z "$addr" || "$addr" == "0x" ]]; then
            echo "Error: Failed to extract symbol $symbol from $CORE1_READELF"
            exit 1
        fi
        echo "$addr"
    }

    CORE1_RAM_START=$(extract_symbol "__data_start__")
    CORE1_RAM_END=$(extract_symbol "__edata")
    CORE1_TEXT_START=$(extract_symbol "__image_start")
    CORE1_TEXT_END=$(extract_symbol "__DATA_END_ALIGNED")

    # Function to write SPDX Apache-2.0 license header to a file
    write_license() {
        local target_file="$1"
        cat > "$target_file" << 'EOF'
/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
EOF
    }

    # Generate the .ld file for core0 to include with license + variables
    write_license "$CORE1_LD"
    {
        echo "CORE1_RAM_START = $CORE1_RAM_START;"
        echo "CORE1_RAM_END = $CORE1_RAM_END;"
        echo "CORE1_TEXT_START = $CORE1_TEXT_START;"
        echo "CORE1_TEXT_END = $CORE1_TEXT_END;"
    } >> "$CORE1_LD"

    # Fix any incorrect INCLUDE line in the linker script
    LINKER_SCRIPT="src/projects/${REAL_PROJECT}/core0/sys/${BASE_PLATFORM}/linker_script.ld"
    CORRECT_INCLUDE="INCLUDE \"src/projects/${REAL_PROJECT}/core0/sys/${BASE_PLATFORM}/generated_core1.ld\""
    
    if [[ -f "$LINKER_SCRIPT" ]]; then
        # Replace any INCLUDE line (regardless of what path it contains) with the correct path
        sed -i "/^INCLUDE /c\\$CORRECT_INCLUDE" "$LINKER_SCRIPT"
        echo "Fixed INCLUDE line in $LINKER_SCRIPT to use correct path"
    else
        echo "Warning: Linker script not found: $LINKER_SCRIPT"
    fi

    # Build Core0
    run_build_flow "$CORE0_PLATFORM" false

    # Restore the core1_image.c file to its original format with license
    write_license "$CORE1_C"
    cat >> "$CORE1_C" << 'EOF'
__attribute__((section(".core1_image"))) const unsigned char core1_bin[]   = {};
unsigned int                                                 core1_bin_len = 0;
EOF

    exit 0
fi

# Build
BUILD_CMD="./ubs build"
if [[ -n "$PROJECT" ]]; then BUILD_CMD+=" PROJECT=$PROJECT"; fi
if [[ -n "$PLATFORM" ]]; then BUILD_CMD+=" PLATFORM=$PLATFORM"; fi
if [[ -n "$BOARD" ]]; then BUILD_CMD+=" BOARD=$BOARD"; fi
if [[ -n "$MODE" ]]; then BUILD_CMD+=" MODE=$MODE"; fi
if [[ -n "$VERSION" ]]; then BUILD_CMD+=" VERSION=$VERSION"; fi
if [[ -n "$CODE_GROUP_INDEX" ]]; then BUILD_CMD+=" CODE_GROUP_INDEX=$CODE_GROUP_INDEX"; fi
if [[ -n "$STREAMBOOT" ]]; then BUILD_CMD+=" STREAMBOOT=$STREAMBOOT"; fi
if [[ -n "$RUN_LOCAL" ]]; then BUILD_CMD+=" run_local=$RUN_LOCAL"; fi
if [[ "$BUILD_ONLY" == "1" ]]; then BUILD_CMD+=" BUILD_ONLY=1"; fi

echo "Running: $BUILD_CMD"
eval $BUILD_CMD
BUILD_EXIT_CODE=$?
if [[ $BUILD_EXIT_CODE -ne 0 ]]; then
    echo "Build command failed with exit code $BUILD_EXIT_CODE. Exiting."
    exit $BUILD_EXIT_CODE
fi

# Generate multi-bin for streaming boot
if [[ "$STREAMBOOT" == "1" ]]; then
    MULTI_BIN_CMD="./ubs multi-bin"
    if [[ -n "$PROJECT" ]]; then MULTI_BIN_CMD+=" PROJECT=$PROJECT"; fi
    if [[ -n "$PLATFORM" ]]; then MULTI_BIN_CMD+=" PLATFORM=$PLATFORM"; fi
    if [[ -n "$BOARD" ]]; then MULTI_BIN_CMD+=" BOARD=$BOARD"; fi
    if [[ -n "$MODE" ]]; then MULTI_BIN_CMD+=" MODE=$MODE"; fi
    if [[ -n "$SIGN_KEYSET" ]]; then MULTI_BIN_CMD+=" SIGN_KEYSET=$SIGN_KEYSET"; fi
    if [[ -n "$VERSION" ]]; then MULTI_BIN_CMD+=" VERSION=$VERSION"; fi
    if [[ -n "$STREAMBOOT" ]]; then MULTI_BIN_CMD+=" STREAMBOOT=$STREAMBOOT"; fi
    if [[ -n "$RUN_LOCAL" ]]; then MULTI_BIN_CMD+=" run_local=$RUN_LOCAL"; fi
    if [[ "$BUILD_ONLY" == "1" ]]; then MULTI_BIN_CMD+=" BUILD_ONLY=1"; fi

    echo "Running: $MULTI_BIN_CMD"
    eval $MULTI_BIN_CMD
    MULTI_BIN_EXIT_CODE=$?
    if [[ $MULTI_BIN_EXIT_CODE -ne 0 ]]; then
        echo "Multi bin command failed with exit code $MULTI_BIN_EXIT_CODE. Exiting."
        exit $MULTI_BIN_EXIT_CODE
    fi
fi

# skip signing and downloading if BUILD_ONLY is set
if [[ "$BUILD_ONLY" == "1" ]]; then
    echo "BUILD_ONLY=1 set, skipping signing and downloading steps."
    exit 0
fi

if [[ "$PLATFORM" == "mcxn547-core1" || "$PLATFORM" == "mcxn556-core1" ]]; then
    echo "Platform $PLATFORM is not supported for signing."
    exit 0
fi

# Local signing
# if SIGN_KEYSET starts with "local", then process local-signing.
if [[ "$SIGN_KEYSET" == "local"* ]]; then
    echo "SIGN_KEYSET set to 'local', process local-signing."
    
    LOCAL_SIGN_CMD="./ubs local-sign"
    # if SIGN_KEYSET is "local-prod", add KEY_TYPE=PROD to the command
    # if SIGN_KEYSET is "local" or "local-debug", add KEY_TYPE=DEBUG to the command
    if [[ "$SIGN_KEYSET" == "local-prod" ]]; then
        LOCAL_SIGN_CMD+=" KEY_TYPE=PROD"
    elif [[ "$SIGN_KEYSET" == "local" || "$SIGN_KEYSET" == "local-debug" ]]; then
        LOCAL_SIGN_CMD+=" KEY_TYPE=DEBUG"
    else
        echo "Unknown SIGN_KEYSET: $SIGN_KEYSET"
        exit 1
    fi
    if [[ -n "$PROJECT" ]]; then LOCAL_SIGN_CMD+=" PROJECT=$PROJECT"; fi
    if [[ -n "$PLATFORM" ]]; then LOCAL_SIGN_CMD+=" PLATFORM=$PLATFORM"; fi
    if [[ -n "$BOARD" ]]; then LOCAL_SIGN_CMD+=" BOARD=$BOARD"; fi
    if [[ -n "$MODE" ]]; then LOCAL_SIGN_CMD+=" MODE=$MODE"; fi
    if [[ -n "$VERSION" ]]; then LOCAL_SIGN_CMD+=" VERSION=$VERSION"; fi
    if [[ -n "$CODE_GROUP_INDEX" ]]; then LOCAL_SIGN_CMD+=" CODE_GROUP_INDEX=$CODE_GROUP_INDEX"; fi
    if [[ -n "$STREAMBOOT" ]]; then LOCAL_SIGN_CMD+=" STREAMBOOT=$STREAMBOOT"; fi
    if [[ -n "$RUN_LOCAL" ]]; then LOCAL_SIGN_CMD+=" run_local=$RUN_LOCAL"; fi
    
    # Add UBS_DOCKER argument for mcxn556 platforms
    if [[ "$PLATFORM" == mcxn556* ]]; then
        UBS_556_DOCKER_IMAGE="${UBS_556_DOCKER_IMAGE:-gitlab-master.nvidia.com:5005/gfw/chips/mcu/mcxn236/ubs-556:0.0.3}"
        LOCAL_SIGN_CMD+=" UBS_DOCKER=${UBS_556_DOCKER_IMAGE}"
        echo "Assign UBS_DOCKER for mcxn556 platform local signing"
    fi

    echo "Running: $LOCAL_SIGN_CMD"
    eval $LOCAL_SIGN_CMD
    LOCAL_SIGN_EXIT_CODE=$?
    if [[ $LOCAL_SIGN_EXIT_CODE -ne 0 ]]; then
        echo "Local signing command failed with exit code $LOCAL_SIGN_EXIT_CODE. Exiting."
        exit $LOCAL_SIGN_EXIT_CODE
    fi
    exit 0
fi

SIGN_REQ_MBI_CMD="./ubs sign-request-mbi"
if [[ -n "$PROJECT" ]]; then SIGN_REQ_MBI_CMD+=" PROJECT=$PROJECT"; fi
if [[ -n "$PLATFORM" ]]; then SIGN_REQ_MBI_CMD+=" PLATFORM=$PLATFORM"; fi
if [[ -n "$BOARD" ]]; then SIGN_REQ_MBI_CMD+=" BOARD=$BOARD"; fi
if [[ -n "$MODE" ]]; then SIGN_REQ_MBI_CMD+=" MODE=$MODE"; fi
if [[ -n "$SIGN_KEYSET" ]]; then SIGN_REQ_MBI_CMD+=" SIGN_KEYSET=$SIGN_KEYSET"; fi
if [[ -n "$VERSION" ]]; then SIGN_REQ_MBI_CMD+=" VERSION=$VERSION"; fi
if [[ -n "$CODE_GROUP_INDEX" ]]; then SIGN_REQ_MBI_CMD+=" CODE_GROUP_INDEX=$CODE_GROUP_INDEX"; fi
if [[ -n "$STREAMBOOT" ]]; then SIGN_REQ_MBI_CMD+=" STREAMBOOT=$STREAMBOOT"; fi
if [[ -n "$RUN_LOCAL" ]]; then SIGN_REQ_MBI_CMD+=" run_local=$RUN_LOCAL"; fi

echo "Running: $SIGN_REQ_MBI_CMD"
eval $SIGN_REQ_MBI_CMD
SIGN_REQ_EXIT_CODE=$?
if [[ $SIGN_REQ_EXIT_CODE -ne 0 ]]; then
    echo "Sign request command failed with exit code $SIGN_REQ_EXIT_CODE. Exiting."
    exit $SIGN_REQ_EXIT_CODE
fi

SIGN_DL_MBI_CMD="./ubs sign-download-mbi"
if [[ -n "$PROJECT" ]]; then SIGN_DL_MBI_CMD+=" PROJECT=$PROJECT"; fi
if [[ -n "$PLATFORM" ]]; then SIGN_DL_MBI_CMD+=" PLATFORM=$PLATFORM"; fi
if [[ -n "$BOARD" ]]; then SIGN_DL_MBI_CMD+=" BOARD=$BOARD"; fi
if [[ -n "$MODE" ]]; then SIGN_DL_MBI_CMD+=" MODE=$MODE"; fi
if [[ -n "$VERSION" ]]; then SIGN_DL_MBI_CMD+=" VERSION=$VERSION"; fi
if [[ -n "$CODE_GROUP_INDEX" ]]; then SIGN_DL_MBI_CMD+=" CODE_GROUP_INDEX=$CODE_GROUP_INDEX"; fi
if [[ -n "$STREAMBOOT" ]]; then SIGN_DL_MBI_CMD+=" STREAMBOOT=$STREAMBOOT"; fi
if [[ -n "$RUN_LOCAL" ]]; then SIGN_DL_MBI_CMD+=" run_local=$RUN_LOCAL"; fi

echo "Running: $SIGN_DL_MBI_CMD"
eval $SIGN_DL_MBI_CMD
SIGN_DL_MBI_EXIT_CODE=$?
if [[ $SIGN_DL_MBI_EXIT_CODE -ne 0 ]]; then
    echo "Sign download mbi command failed with exit code $SIGN_DL_MBI_EXIT_CODE. Exiting."
    exit $SIGN_DL_MBI_EXIT_CODE
fi

SIGN_REQ_SB_CMD="./ubs sign-request-sb"
if [[ -n "$PROJECT" ]]; then SIGN_REQ_SB_CMD+=" PROJECT=$PROJECT"; fi
if [[ -n "$PLATFORM" ]]; then SIGN_REQ_SB_CMD+=" PLATFORM=$PLATFORM"; fi
if [[ -n "$BOARD" ]]; then SIGN_REQ_SB_CMD+=" BOARD=$BOARD"; fi
if [[ -n "$MODE" ]]; then SIGN_REQ_SB_CMD+=" MODE=$MODE"; fi
if [[ -n "$SIGN_KEYSET" ]]; then SIGN_REQ_SB_CMD+=" SIGN_KEYSET=$SIGN_KEYSET"; fi
if [[ -n "$VERSION" ]]; then SIGN_REQ_SB_CMD+=" VERSION=$VERSION"; fi
if [[ -n "$CODE_GROUP_INDEX" ]]; then SIGN_REQ_SB_CMD+=" CODE_GROUP_INDEX=$CODE_GROUP_INDEX"; fi
if [[ -n "$STREAMBOOT" ]]; then SIGN_REQ_SB_CMD+=" STREAMBOOT=$STREAMBOOT"; fi
if [[ -n "$RUN_LOCAL" ]]; then SIGN_REQ_SB_CMD+=" run_local=$RUN_LOCAL"; fi

echo "Running: $SIGN_REQ_SB_CMD"
eval $SIGN_REQ_SB_CMD
SIGN_REQ_SB_EXIT_CODE=$?
if [[ $SIGN_REQ_SB_EXIT_CODE -ne 0 ]]; then
    echo "Sign request sb command failed with exit code $SIGN_REQ_SB_EXIT_CODE. Exiting."
    exit $SIGN_REQ_SB_EXIT_CODE
fi

SIGN_DL_SB_CMD="./ubs sign-download-sb"
if [[ -n "$PROJECT" ]]; then SIGN_DL_SB_CMD+=" PROJECT=$PROJECT"; fi
if [[ -n "$PLATFORM" ]]; then SIGN_DL_SB_CMD+=" PLATFORM=$PLATFORM"; fi
if [[ -n "$BOARD" ]]; then SIGN_DL_SB_CMD+=" BOARD=$BOARD"; fi
if [[ -n "$MODE" ]]; then SIGN_DL_SB_CMD+=" MODE=$MODE"; fi
if [[ -n "$VERSION" ]]; then SIGN_DL_SB_CMD+=" VERSION=$VERSION"; fi
if [[ -n "$CODE_GROUP_INDEX" ]]; then SIGN_DL_SB_CMD+=" CODE_GROUP_INDEX=$CODE_GROUP_INDEX"; fi
if [[ -n "$STREAMBOOT" ]]; then SIGN_DL_SB_CMD+=" STREAMBOOT=$STREAMBOOT"; fi
if [[ -n "$RUN_LOCAL" ]]; then SIGN_DL_SB_CMD+=" run_local=$RUN_LOCAL"; fi

echo "Running: $SIGN_DL_SB_CMD"
eval $SIGN_DL_SB_CMD
SIGN_DL_SB_EXIT_CODE=$?
if [[ $SIGN_DL_SB_EXIT_CODE -ne 0 ]]; then
    echo "Sign download sb command failed with exit code $SIGN_DL_SB_EXIT_CODE. Exiting."
    exit $SIGN_DL_SB_EXIT_CODE
fi

# Generate configuration file JSON
if [[ -n "$PROJECT" ]]; then
    echo "Generating project configuration file JSON..."

    # Determine build directory based on PROJECT, PLATFORM, and MODE
    BUILD_DIR="build/${PROJECT}-${PLATFORM}-${MODE}"

    # Create build directory if it doesn't exist
    mkdir -p "$BUILD_DIR"

    # Generate JSON file in the build directory
    python3 parse_config.py "$PROJECT" -o "${BUILD_DIR}/${PROJECT}_config.json"
    if [[ $? -eq 0 ]]; then
        echo "Configuration file generated successfully: ${BUILD_DIR}/${PROJECT}_config.json"
    else
        echo "Warning: Failed to generate configuration file"
    fi
fi
