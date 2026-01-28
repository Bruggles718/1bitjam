#!/bin/bash
set -e

SRC=/src          # mounted read-only
WORK=/work        # container-only
OUT=/output

export PLAYDATE_SDK_PATH=$(find /root -maxdepth 1 -type d -name "PlaydateSDK*" | head -n 1)

sed -i 's/^int eventHandler/\/\/ int eventHandler/' $PLAYDATE_SDK_PATH/C_API/pd_api.h

echo "SDK: $PLAYDATE_SDK_PATH"

# Fresh workspace
rm -rf $WORK
mkdir -p $WORK

# Copy project, excluding build/
rsync -a \
  --exclude build \
  --exclude '*.pdx' \
  $SRC/ $WORK/

# Build
mkdir $WORK/build
cd $WORK/build

if [ -z "$1" ]; then
  echo "BUILDING FOR SIM"
  cmake ..
else
  echo "BUILDING FOR DEVICE"
  cmake -DCMAKE_TOOLCHAIN_FILE=$PLAYDATE_SDK_PATH/C_API/buildsupport/arm.cmake ..
fi
make

# Export pdx
PDX=$(find $WORK -name "*.pdx" | head -n 1)

if [ -z "$PDX" ]; then
  echo "❌ No .pdx produced"
  exit 1
fi

cp -r "$PDX" $OUT
echo "✅ Output copied to host"

if [ -z "$1" ]; then
  $PLAYDATE_SDK_PATH/bin/PlaydateSimulator $PDX
fi