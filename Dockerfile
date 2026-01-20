FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc-arm-none-eabi \
    libnewlib-arm-none-eabi \
    cmake \
    wget \
    rsync \
    tar \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /root

# Download SDK
RUN wget https://download.panic.com/playdate_sdk/Linux/PlaydateSDK-latest.tar.gz \
    && tar -xzf PlaydateSDK-latest.tar.gz \
    && rm PlaydateSDK-latest.tar.gz

# Entrypoint
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]