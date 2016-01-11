#!/bin/bash
fusermount -u asdf
clang-format -i *.cpp *.hpp && \
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Debug && \
ninja-build
