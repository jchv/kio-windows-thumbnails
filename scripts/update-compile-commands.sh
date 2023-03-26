#!/bin/sh
set -e
nix develop --command bash -c "cmake -B .build -S . -D CMAKE_EXPORT_COMPILE_COMMANDS=1"
ln -f -s ./.build/compile_commands.json .
