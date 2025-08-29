FROM ubuntu:24.04
LABEL maintainer="Alan Cheng<alacheng@nvidia.com>"
LABEL version="1"
LABEL description="UBS image"
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        git git-lfs git-delta ca-certificates curl locales \
        gawk sed xxd jq rsync make groff ncurses-bin \
        gnupg python3 python3-pip python3-dev python3-clang \
        openssh-client clang-tidy-18 clang-format-18 \
        lcov gcc-13 g++-13 \
        doxygen graphviz asciidoctor asciidoc gem javacc ruby-rouge \
        file dos2unix less patch \
    && apt autoclean && apt autoremove \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get upgrade -y
RUN locale-gen en_US.UTF-8 && update-locale LANG=en_US.UTF-8

RUN pip3 install --break-system-packages \
        jsonschema==4.23.0 \
        ruamel.yaml==0.18.14 \
        spsdk==2.3.0
WORKDIR /tmp

