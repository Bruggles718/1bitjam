#!/bin/bash
set -e

BUILD=/project/build
OUT=/output

# Auto-detect SDK
SDK_DIR=$(find /root -maxdepth 1 -type d -name "PlaydateSDK*" | head -n 1)

echo "Using SDK: $SDK_DIR"

mkdir -p $BUILD
cd $BUILD

cmake -DCMAKE_TOOLCHAIN_FILE=$SDK_DIR/C_API/buildsupport/arm.cmake ..
make

# Find .pdx
PDX=$(find .. -maxdepth 1 -name "*.pdx" | head -n 1)

if [ -z "$PDX" ]; then
  echo "❌ No .pdx produced"
  exit 1
fi

cp -r "$PDX" $OUT

echo "✅ Build complete! Output copied to host."