#!/bin/sh
# Builds and runs the active-ragdoll core tests with g++ (any Linux / WSL / MSYS). The same sources build in the game.
set -e
cd "$(dirname "$0")/../.."
mkdir -p build-tests
${CXX:-g++} -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -I src tests/euphoria/euphoria_tests.cpp src/euphoria/euphoria_body.cpp src/euphoria/euphoria_balance.cpp -o build-tests/euphoria_tests
./build-tests/euphoria_tests
