docker run --rm \
  -v "$(pwd)":/src:ro \
  -v "$(pwd)":/output \
  playdate-cpp-build