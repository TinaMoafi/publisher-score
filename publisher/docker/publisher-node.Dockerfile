FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libboost-all-dev \
    python3 \
    iproute2 \
    iputils-ping \
    tcpdump \
    net-tools \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /tmp

RUN git clone --depth 1 https://github.com/COVESA/vsomeip.git && \
    cd vsomeip && \
    cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local -DENABLE_SIGNAL_HANDLING=1 && \
    cmake --build build -j2 && \
    cmake --install build && \
    ldconfig && \
    rm -rf /tmp/vsomeip

WORKDIR /app/publisher-node

CMD ["sleep", "infinity"]