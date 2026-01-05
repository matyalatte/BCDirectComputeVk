#!/usr/bin/env bash

set -e

DXC_LOCATION="$(realpath $(dirname "$0"))/dxc"
export PATH="${DXC_LOCATION}/bin:${DXC_LOCATION}/lib:$PATH"

# Check if dxc exists in PATH
if ! command -v dxc >/dev/null 2>&1; then
    echo "Error: dxc not found in PATH. Run install_dxc.sh first."
    exit 1
fi

compile_shader() {
    local base="$1"
    local entry="$2"

    echo Generating ${base}_${entry}.inc...

    dxc \
        -spirv \
        -HV 2018 \
        -fvk-use-dx-layout \
        -T cs_6_0 \
        -fspv-target-env=vulkan1.0 \
        -fvk-u-shift 2 0 \
        -fvk-b-shift 3 0 \
        -E "$entry" \
        -Fh "${base}_${entry}.inc" \
        -Vn "${base}_${entry}" \
        "${base}.hlsl"

    dxc \
        -spirv \
        -HV 2018 \
        -fvk-use-dx-layout \
        -T cs_6_0 \
        -fspv-target-env=vulkan1.0 \
        -fvk-u-shift 2 0 \
        -fvk-b-shift 3 0 \
        -E "$entry" \
        -Fo "${base}_${entry}.spv" \
        "${base}.hlsl"
}

pushd $(dirname "$0")/src

compile_shader BC7Encode TryMode456CS
compile_shader BC7Encode TryMode137CS
compile_shader BC7Encode TryMode02CS
compile_shader BC7Encode EncodeBlockCS

compile_shader BC6HEncode TryModeG10CS
compile_shader BC6HEncode TryModeLE10CS
compile_shader BC6HEncode EncodeBlockCS

popd
