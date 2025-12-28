#!/usr/bin/env bash

# apt install vulkan-tools libvulkan-dev

mkdir -p $(dirname "$0")/build
pushd $(dirname "$0")/build
    cmake -D CMAKE_CONFIGURATION_TYPES=Debug \
        -D CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded \
        ../

    cmake --build . --config Debug
    cp ./example-app ..
popd
