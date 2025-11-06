#!/usr/bin/bash

# print the usage of the script
for arg in "$@"; do
    if [[ "$arg" == "-h" || "$arg" == "--help" || "$arg" == "help" ]]; then
        echo "Usage: $0 [PROJECT=project] [PLATFORM=platform] [BOARD=board] [MODE=mode] [SIGN_KEYSET=keyset] [VERSION=version] [STREAMBOOT=1] [RUN_LOCAL=1]"
        echo "PROJECT: project name (for example: pg540)"
        echo "PLATFORM: platform name (for example: mcxn236)"
        echo "BOARD: board name (for example: pg540)"
        echo "MODE: mode name (for example: dev)"
        echo "SIGN_KEYSET: keyset name (for example: S0). If using local signing, use 'local' or 'local-debug' for debug signing, or 'local-prod' for prod sign."
        echo "VERSION: firmware version (for example: 02.0010.0000)"
        echo "STREAMBOOT: set to 1 to generate multi-binary files (for example: STREAMBOOT=1)"
        echo "RUN_LOCAL: set to 1 to execute local without docker (for example: RUN_LOCAL=1)"
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
        OBJCOPY=libexec/toolchain/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi/bin/arm-none-eabi-objcopy
        $OBJCOPY -O binary "$CORE1_ELF" "$CORE1_BIN" 2>/dev/null
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
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
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

echo "Running: $BUILD_CMD"
eval $BUILD_CMD
BUILD_EXIT_CODE=$?
if [[ $BUILD_EXIT_CODE -ne 0 ]]; then
    echo "Build command failed with exit code $BUILD_EXIT_CODE. Exiting."
    exit $BUILD_EXIT_CODE
fi
