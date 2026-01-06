# BCDirectComputeVk

DirectXTex GPU compressor via the Vulkan API

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![donate](https://img.shields.io/static/v1?label=donate&message=%E2%9D%A4&logo=GitHub&color=%23fe8e86)](https://github.com/sponsors/matyalatte)  

> [!Warning]
> This repository is not a library. It is an experimental codebase and may require modifications for use in other projects.  

## About

BCDirectComputeVk is an experimental attempt to port [the BC6 and BC7 encoders from DirectXTex](https://github.com/microsoft/DirectXTex/tree/main/DirectXTex/Shaders) to Linux and macOS. It enables compiling HLSL shaders using [DirectXShaderCompiler](https://github.com/microsoft/DirectXShaderCompiler) and executing them through a custom C++ class. The goal is to integrate this encoder pipeline into my DDS-related projects.  

## Why not Use Wine?

DirectX shaders can be executed on Linux via [Wine](https://www.winehq.org/) or translation layers such as [dxvk](https://github.com/doitsujin/dxvk). However, these solutions require a large and complex
runtime stack intended for full Direct3D compatibility. In contrast, DirectXTex only requires a small subset of DirectCompute functionality. Supporting GPU codecs can be achieved by integrating several source files.  

## Status

- ✅Windows (NVIDIA GPU): working
- ✅WSL (NVIDIA GPU): working
- ✅Linux VM (NVIDIA GPU): working (on Google Colab instances)
- ✅Linux VM (llvmpipe): working (on Github hosted servers)
- ✅macOS VM (Apple Paravirtual device): working (on Github hosted servers)
- ❓Physical Linux/macOS machines: not tested
- ❓Arm64 Linux: not tested

## Build Instructions

First, download submoudles.
```bash
git submodule update --init
```

Next, install gcc, cmake, and vulkan-sdk.

```bash
# Note: This only works for Ubuntu24.04.
# See here for the other platforms.
# https://vulkan.lunarg.com/doc/sdk/latest/linux/getting_started.html

wget -qO- https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo tee /etc/apt/trusted.gpg.d/lunarg.asc
sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-noble.list http://packages.lunarg.com/vulkan/lunarg-vulkan-noble.list
sudo apt update
sudo apt install build-essential cmake vulkan-sdk

# You can also install vulkan from other packages and "./install_dxc.sh"
#   > sudo apt install vulkan-tools libvulkan-dev vulkan-validationlayers
#   > ./install_dxc.sh
# macOS users can run ./install_vulkan_mac.sh as admin to install the SDK
#   > sudo ./install_vulkan_mac.sh
```

Then, run `dxc_compile.sh` to compile HLSL files.

```bash
./dxc_compile.sh
```

Finally, build and run `example-app` using `build.sh`. It generates `BC6_result.dds` and `BC7_result.dds`.

```bash
./build.sh
./example-app
```

## Example Code

```c++
#include "VulkanDeviceManager.h"
#include "BCDirectComputeVk.h"

void main() {
    // You can easily create VkDevice with VulkanDeviceManager!
    VulkanDeviceManager manager = VulkanDeviceManager();
    VkResult r;

    r = manager.CreateInstance();
    if (r != VK_SUCCESS) {
        // Failed to create VkInstance
        return;
    }
    if (!manager.HasGPU()) {
        // No Vulkan-capable GPUs found
        return;
    }

    r = manager.CreateDevice(-1);  // You can also replace -1 with a GPU id.
    if (r != VK_SUCCESS) {
        // Failed to create VkDevice
        return;
    }

    // Get using device info
    VkDevice         device          = manager.GetDevice(),
    VkPhysicalDevice physical_device = manager.GetUsingGPU(),
    uint32_t         family_id       = manager.GetUsingFamilyId()

    // Initialize compressor
    GPUCompressBCVk compressor = GPUCompressBCVk();

    r = compressor.Initialize(device, physical_device, family_id);
    if (r != VK_SUCCESS) {
        // Failed to create shaders.
        return;
    }

    uint32_t width = 512;
    uint32_t height = 512;
    r = compressor.Prepare(width, height, 0, DXGI_FORMAT_BC7_UNORM, 1.0f);
    if (r != VK_SUCCESS) {
        // Failed to create buffers.
        return;
    }

    std::vector<uint8_t> rgba_pixels(compressor.GetSrcBufSize());
    // Store rgba pixels to rgba_pixels somehow.

    std::vector<uint8_t> bc7_pixels(compressor.GetOutBufSize());

    r = compressor.Compress(&rgba_pixels[0], &bc7_pixels[0]);
    if (r != VK_SUCCESS) {
        // Failed to compress
        return;
    }

    // bc7_pixels now contains compressed data!
}
```
