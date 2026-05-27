# OpenSMA #
Instructions to build helloworld FW

## Prepare Environment ##
Use Ubuntu 24.04
1. Install package
```
apt-get update && apt-get install -y --no-install-recommends \
        git git-lfs git-delta ca-certificates curl locales \
        gawk sed xxd jq rsync make groff ncurses-bin \
        gnupg python3 python3-pip python3-dev python3-clang \
        openssh-client clang-tidy-18 clang-format-18 \
        lcov gcc-13 g++-13 \
        doxygen graphviz asciidoctor asciidoc gem javacc ruby-rouge \
        file dos2unix less patch xz-utils
```
2. Install python package
```
pip3 install --break-system-packages \
        jsonschema==4.23.0 \
        ruamel.yaml==0.18.14 \
        spsdk==3.4.0
```
3. Link make
```
ln -sf /usr/bin/make corepdk/ubs/bin/make
```

## Prepare Toolchain ##
1. Download [arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz](https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz)
2. Extract *arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz* into `libexec/toolchain/`
3. Download [gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2](https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2?rev=78196d3461ba4c9089a67b5f33edf82a&hash=5631ACEF1F8F237389F14B41566964EC)
4. Extract *gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2* into `libexec/toolchain/`
5. Download [alire](https://github.com/alire-project/alire/releases/download/v2.1.0/alr-2.1.0-bin-x86_64-linux.zip) 
6. Run `alr get gnat_arm_elf=14.2.1`
7. Move *gnat_arm_elf_14.2.1_524d4d41* into `libexec/toolchain/`

## Prepare SDK ##
1. Download Processors MCXN556S SDK version 25.09.00 from https://mcuxpresso.nxp.com/en
    * Host OS: Linux
    * Toolchain/IDE: All toolchains
    * Select all middlewares
2. Extract *25.09.00 SDK* into `libexec/sdk/SDK_25_09_00_MCXN556S`
```
mkdir -p libexec/sdk/SDK_25_09_00_MCXN556S
tar -xvzf SDK_25_09_00_MCXN556S.tar.gz -C libexec/sdk/SDK_25_09_00_MCXN556S
```

## Patch SDK ##
### MCU SDK ###
```
cd libexec/sdk/SDK_25_09_00_MCXN556S
xz -dc ../../patch/mcu.patch.xz | patch -p1 -N
```

## Build and Sign FW ##
Artifact will be generated under `build` folder

### MCU FW ###
```
./mcu_build.sh PROJECT=mcxn547helloworld BOARD=mcxn547helloworld PLATFORM=mcxn556-both MODE=rel SIGN_KEYSET=local RUN_LOCAL=1
```

## Use Docker ##
You can also build and sign FW in Docker.

```
# Build Docker image if you didn't build before
docker build -f ./libexec/ubs.dockerfile -t opensma .
# Run Docker image
docker run --rm -it --user=${UID}:${GID} -v $(pwd):/opensma opensma
# Build and sign FW
./mcu_build.sh PROJECT=mcxn547helloworld BOARD=mcxn547helloworld PLATFORM=mcxn556-both MODE=rel SIGN_KEYSET=local RUN_LOCAL=1
```
