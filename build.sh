#!/bin/bash
docker run --rm \
  -v "$(pwd)/":/output \
  playdate-build