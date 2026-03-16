#!/usr/bin/env bash

set -euo pipefail

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cpack --config build/CPackConfig.cmake -G DEB
