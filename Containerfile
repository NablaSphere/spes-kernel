FROM ubuntu:24.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    bc \
    bison \
    clang \
    cpio \
    flex \
    git \
    libelf-dev \
    libncurses-dev \
    libssl-dev \
    lld \
    llvm \
    make \
    openssl \
    pkg-config \
    python-is-python3 \
    xz-utils \
  && git config --system --add safe.directory '*' \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /work
