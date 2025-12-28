#!/usr/bin/env bash
# Installs DirectXShaderCompiler to ./dxc
# You need to add ./dxc/bin and ./dxc/lib to the PATH variable when using dxc.

set -euo pipefail

DXC_VERSION="v1.8.2505.1"
DXC_RELEASE_DATE="2025_07_14"
DXC_RELEASE_URL="https://github.com/microsoft/DirectXShaderCompiler/releases/download/${DXC_VERSION}"
DXC_ARCHIVE="linux_dxc_${DXC_RELEASE_DATE}.x86_64.tar.gz"
INSTALL_PREFIX="$(realpath $(dirname "$0"))/dxc"

if [[ "$(uname -m)" != "x86_64" ]]; then
    echo "There is no dxc releases for your CPU architecture. Build dxc yourself."
    exit 1
fi

mkdir -p $INSTALL_PREFIX
echo "Installing DirectXShaderCompiler ${DXC_VERSION}"

# Create temp directory
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

cd "$TMPDIR"

echo "Downloading DXC..."
curl -L -o "$DXC_ARCHIVE" "${DXC_RELEASE_URL}/${DXC_ARCHIVE}"

echo "Extracting..."
tar -xzf "$DXC_ARCHIVE"

echo "Installing to ${INSTALL_PREFIX}..."
cp -r bin "${INSTALL_PREFIX}/"
cp -r lib "${INSTALL_PREFIX}/"

echo "Verifying installation..."
export PATH="${INSTALL_PREFIX}/bin:${INSTALL_PREFIX}/lib:$PATH"
if command -v dxc >/dev/null 2>&1; then
    dxc --version
    echo "DXC installation successful."
else
    echo "DXC installation failed." >&2
    exit 1
fi
