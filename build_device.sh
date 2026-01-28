docker run --rm \
  -v "$(pwd)":/src:ro \
  -v "$(pwd)":/output \
  -v "$PWD/entrypoint.sh:/src/entrypoint.sh" \
  playdate-cpp-build \
  sh /src/entrypoint.sh 1