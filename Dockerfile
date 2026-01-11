FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

ENV PLAYDATE_SDK_PATH=/root/PlaydateSDK-3.0.2/

# Dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc-arm-none-eabi \
    libnewlib-arm-none-eabi \
    cmake \
    wget \
    tar \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /root

# Download SDK
RUN wget https://download.panic.com/playdate_sdk/Linux/PlaydateSDK-latest.tar.gz \
    && tar -xzf PlaydateSDK-latest.tar.gz \
    && rm PlaydateSDK-latest.tar.gz

# Copy project (filtered by .dockerignore)
WORKDIR /project
COPY . .

# Entrypoint
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]