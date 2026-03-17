# syntax=docker/dockerfile:1
FROM ubuntu:24.04
LABEL maintainer="Eric Wu<eriwu@nvidia.com>"
LABEL version="0.0.24"
LABEL description="UBS Docker Image"
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        git git-lfs git-delta ca-certificates curl locales \
        gawk sed xxd jq rsync make groff ncurses-bin \
        gnupg python3 python3-dev python3-clang python3-pip \
        openssh-client sshpass clang-tidy-18 clang-format-18 \
        lcov gcc-13 g++-13 \
        doxygen graphviz asciidoctor asciidoc gem javacc ruby-rouge \
        file dos2unix less \
    && apt autoclean && apt autoremove \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get upgrade -y
RUN gem install asciidoctor-diagram
RUN locale-gen en_US.UTF-8 && update-locale LANG=en_US.UTF-8
RUN --mount=type=secret,id=pdk_bin_key \
    PDK_BIN_KEY="$(cat /run/secrets/pdk_bin_key)" && \
    git clone --depth=1 https://ubs-docker-pull:${PDK_BIN_KEY}@gitlab-master.nvidia.com/gfw/chips/pdk/bin.git /opt/ubs && \
    rm -rf /opt/ubs/.git

#Install nvsec for mcu signing
RUN mv /usr/lib/python$(python3 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')/EXTERNALLY-MANAGED /usr/lib/python$(python3 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')/EXTERNALLY-MANAGED.old
RUN pip install jsonschema
RUN pip install nvsec -i https://urm.nvidia.com/artifactory/api/pypi/sw-cloudsec-pypi/simple --extra-index-url https://urm.nvidia.com/artifactory/api/pypi/sw-cftt-pypi-local/simple
RUN pip install --upgrade nvsec -i https://urm.nvidia.com/artifactory/api/pypi/sw-cloudsec-pypi/simple --extra-index-url https://urm.nvidia.com/artifactory/api/pypi/sw-cftt-pypi-local/simple
RUN pip install bitarray
RUN pip install python-gitlab
RUN pip install ruamel.yaml==0.18.14
RUN pip install spsdk==2.3.0
RUN pip install gcovr==8.3
RUN pip install rich
WORKDIR /tmp
