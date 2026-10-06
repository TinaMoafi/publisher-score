FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    curl \
    unzip \
    ca-certificates \
    pkg-config \
    libboost-all-dev \
    python3 \
    iproute2 \
    iputils-ping \
    tcpdump \
    net-tools \
    libc++1 \
    libc++abi1 \
    libc++-dev \
    libc++abi-dev \
    && rm -rf /var/lib/apt/lists/*

RUN curl -L -o /usr/local/bin/bazel \
    https://github.com/bazelbuild/bazelisk/releases/download/v1.20.0/bazelisk-linux-amd64 \
    && chmod +x /usr/local/bin/bazel

WORKDIR /tmp

RUN git clone --depth 1 https://github.com/COVESA/vsomeip.git && \
    cd vsomeip && \
    cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local -DENABLE_SIGNAL_HANDLING=1 && \
    cmake --build build -j2 && \
    cmake --install build && \
    ldconfig && \
    rm -rf /tmp/vsomeip

WORKDIR /app/inc_someip_gateway

CMD ["sleep", "infinity"]