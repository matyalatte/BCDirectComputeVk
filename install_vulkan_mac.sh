#!/usr/bin/env bash
# Downloads the Vulkan SDK into ~/VulkanSDK/1.x.xxx.x
# and installs binaries into /usr/local

set -euo pipefail

SDK_VERSION="1.4.335.1"
SDK_ROOT="$HOME/VulkanSDK"
UNZIP_DIR="$SDK_ROOT/$SDK_VERSION"

ZIP_NAME="vulkansdk-macos-${SDK_VERSION}.zip"
BASE_URL="https://sdk.lunarg.com/sdk/download/${SDK_VERSION}/mac"
DOWNLOAD_URL="${BASE_URL}/${ZIP_NAME}"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "install_vulkan_mac.sh is only available for macOS."
    exit 1
fi

# Download
mkdir -p "$SDK_ROOT"
cd "$SDK_ROOT"

if [ ! -f "$ZIP_NAME" ]; then
    echo "Downloading Vulkan SDK ${SDK_VERSION}..."
    curl -L -o "$ZIP_NAME" "$DOWNLOAD_URL"
else
    echo "ZIP already downloaded: $ZIP_NAME"
fi

# Extract
if [ ! -d "$UNZIP_DIR" ]; then
    echo "Extracting Vulkan SDK into $UNZIP_DIR..."
    unzip -q "$ZIP_NAME"
else
    echo "SDK already extracted: $UNZIP_DIR"
fi

# Install
echo "Installing required files into /usr/local..."
./vulkansdk-macOS-${SDK_VERSION}.app/Contents/MacOS/vulkansdk-macOS-${SDK_VERSION} \
    --root ~/VulkanSDK/${SDK_VERSION} --accept-licenses --default-answer --confirm-command install \
    com.lunarg.vulkan.core com.lunarg.vulkan.usr
