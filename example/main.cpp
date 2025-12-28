// Test app to compress DDS images

#include "VulkanDeviceManager.h"
#include "BCDirectComputeVk.h"
#include "DDS.h"

#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>

inline bool FourCCEq(const char* str1, const char* str2) {
    return (
        str1[0] == str2[0] &&
        str1[1] == str2[1] &&
        str1[2] == str2[2] &&
        str1[3] == str2[3]);
}

inline bool MaskEq(const uint32_t* mask1, const uint32_t* mask2) {
        return (
        mask1[0] == mask2[0] &&
        mask1[1] == mask2[1] &&
        mask1[2] == mask2[2] &&
        mask1[3] == mask2[3]);
}

int LoadDDS(const char* filename, std::vector<uint8_t>* buf,
            uint32_t* width, uint32_t* height, DXGI_FORMAT* src_format) {
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs) {
        std::cerr << "failed to open " << filename << "\n";
        return 1;
    }
    ifs.seekg(0, std::ios_base::end);
    uint32_t file_size = (uint32_t)ifs.tellg();
    ifs.seekg(0, std::ios_base::beg);
    if (file_size < 148) {
        std::cerr << filename << " is too small\n";
        ifs.close();
        return 1;
    }
    char fourcc[4];
    ifs.read(fourcc, 4);
    if (!FourCCEq(fourcc, "DDS ")) {
        std::cerr << filename << " is not DDS file\n";
        ifs.close();
        return 1;
    }
    ifs.seekg(8, std::ios_base::cur);
    ifs.read(reinterpret_cast<char *>(height), 4);
    ifs.read(reinterpret_cast<char *>(width), 4);
    std::cout << "(width, height) = (" << *width << ", " << *height << ")" << std::endl;
    ifs.seekg(84, std::ios_base::beg);
    ifs.read(fourcc, 4);
    bool isdxt10 = false;
    *src_format = DXGI_FORMAT_UNKNOWN;
    if (FourCCEq(fourcc, "DX10")) {
        isdxt10 = true;
    } else {
        if (fourcc[0] == 116)
            *src_format = DXGI_FORMAT_R32G32B32_FLOAT;
        ifs.seekg(4, std::ios_base::cur);
        uint32_t rgbamask[4];
        for (int i = 0; i < 4; i++) {
            ifs.read(reinterpret_cast<char *>(&rgbamask[i]), 4);
        }

        uint32_t true_rgbamask[4] = {0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000};
        if (MaskEq(rgbamask, true_rgbamask))
            *src_format = DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    ifs.seekg(128, std::ios_base::beg);
    if (isdxt10) {
        uint32_t dxgi_format;
        ifs.read(reinterpret_cast<char *>(&dxgi_format), 4);
        *src_format = (DXGI_FORMAT)dxgi_format;
        ifs.seekg(16, std::ios_base::cur);
    }
    if (*src_format != DXGI_FORMAT_R8G8B8A8_UNORM &&
        *src_format != DXGI_FORMAT_R32G32B32_FLOAT) {
        std::cerr << "DXGI format should be R8G8B8A8_UNORM or R32G32B32_FLOAT\n";
        ifs.close();
        return 1;
    }

    bool isfloat = *src_format == DXGI_FORMAT_R32G32B32_FLOAT;
    uint32_t buf_size = (*width) * (*height) * (isfloat ? 16 : 4);
    uint32_t actual_data_size = file_size - 128 - isdxt10 * 20;
    if (actual_data_size < buf_size) {
        std::cerr << filename << " is too small (expected: " << buf_size << ", actual: " << actual_data_size << ")\n";
        ifs.close();
        return 1;
    }
    *buf = std::vector<uint8_t>(buf_size);
    ifs.read(reinterpret_cast<char*>(&(*buf)[0]), buf_size);
    ifs.close();
    return 0;
}

int SaveDDS(const char* filename, uint32_t width, uint32_t height, DXGI_FORMAT format, void* buf, uint32_t buf_size) {
    DirectX::DDS_HEADER dds_header = {};
    dds_header.size = sizeof(DirectX::DDS_HEADER);
    dds_header.flags = DDS_HEADER_FLAGS_TEXTURE | DDS_HEADER_FLAGS_LINEARSIZE;
    dds_header.height = height;
    dds_header.width = width;
    dds_header.pitchOrLinearSize = buf_size;
    dds_header.ddspf = DirectX::DDSPF_DX10;
    dds_header.mipMapCount = 1;
    dds_header.caps = DDS_SURFACE_FLAGS_TEXTURE;
    DirectX::DDS_HEADER_DXT10 dds_header_dxt10 = {};
    dds_header_dxt10.dxgiFormat = format;
    dds_header_dxt10.resourceDimension = DirectX::DDS_DIMENSION_TEXTURE2D;

    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs) {
        std::cerr << "failed to open " << filename << "\n";
        return 1;
    }

    ofs.write("DDS ", 4);
    ofs.write(
        reinterpret_cast<const char*>(&dds_header),
        sizeof(DirectX::DDS_HEADER));
    ofs.write(
        reinterpret_cast<const char*>(&dds_header_dxt10),
        sizeof(DirectX::DDS_HEADER_DXT10));
    ofs.write(
        reinterpret_cast<const char*>(buf),
        buf_size);
    ofs.close();
    return 0;
}

int TryCompression(
        GPUCompressBCVk* compressor,
        const char* src_file, const char* out_file) {
    std::cout << "\"" << src_file << "\" -> \"" << out_file << "\"\n";

    std::vector<uint8_t> src_pixels;
    uint32_t width, height;
    DXGI_FORMAT src_format;
    int res;
    res = LoadDDS(src_file, &src_pixels, &width, &height, &src_format);
    if (res != 0) return res;

    DXGI_FORMAT bc_format;
    if (src_format == DXGI_FORMAT_R32G32B32_FLOAT)
        bc_format = DXGI_FORMAT_BC6H_UF16;
    else
        bc_format = DXGI_FORMAT_BC7_UNORM;

    VkResult r;
    r = compressor->Prepare(width, height, 0, bc_format, 1.0f);
    if (r != VK_SUCCESS) {
        std::cout << "Failed to create VkBuffer (error " << r << ")\n";
        return 1;
    }

    uint32_t src_buf_size = compressor->GetSrcBufSize();

    uint32_t out_buf_size = compressor->GetOutBufSize();
    std::vector<uint8_t> out_pixels(out_buf_size);
    r = compressor->Compress(&src_pixels[0], &out_pixels[0]);
    if (r != VK_SUCCESS) {
        std::cout << "failed (error " << r << ")\n";
        return 1;
    }

    res = SaveDDS(out_file,
            width, height,
            bc_format,
            out_pixels.data(), out_buf_size);
    return res;
}

void PrintUsage() {
    static const char* const usage =
        "Usage: example-app [<options>]\n"
        "\n"
        "  options:\n"
        "    --enable-debug: enable the validation layer for Vulkan.\n"
        "    --help: show this message.\n";
    std::cout << usage;
}

int main(int argc, char** argv) {
    bool enable_debug = false;

    // Parse args
    for (int i = 1; i < argc; i++) {
        const char* opt = argv[i];
        if (strcmp(opt, "--enable-debug") == 0) {
            enable_debug = true;
        } else if (strcmp(opt, "--help") == 0) {
            PrintUsage();
            return 0;
        } else {
            std::cerr << "ERROR: unknown option (" << opt << ")\n";
            PrintUsage();
            return 1;
        }
    }

    VulkanDeviceManager manager = VulkanDeviceManager();
    VkResult r = manager.CreateInstance(enable_debug);
    if (r != VK_SUCCESS) {
        const char* msg;
        if (!manager.HasInstance()) {
            msg = "Failed to create Vulkan instance";
        } else {
            msg = "vkEnumeratePhysicalDevices failed";
        }
        std::cerr << msg << " (error " << r << ")\n";
        return 1;
    }

    if (!manager.HasGPU()) {
        std::cout << "No Vulkan-capable GPUs found.\n";
        return 0;
    }

    uint32_t gpu_count = manager.GetGPUCount();
    std::cout << "Found " << gpu_count << " physical device(s):\n";

    for (uint32_t i = 0; i < gpu_count; ++i) {
        VkPhysicalDeviceProperties props;
        manager.GetGPUProperties(i, &props);

        std::cout << "  GPU #" << i << ": " << props.deviceName << "\n";
        std::cout << "    Type: ";
        switch (props.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: std::cout << "Integrated GPU\n"; break;
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   std::cout << "Discrete GPU\n"; break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    std::cout << "Virtual GPU\n"; break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:            std::cout << "CPU\n"; break;
            default: std::cout << "Other\n"; break;
        }
        std::cout << "    VID: " << props.vendorID << "\n";
        std::cout << "    PID: " << props.deviceID << "\n";
        std::cout << "    API version: "
                    << VK_VERSION_MAJOR(props.apiVersion) << "."
                    << VK_VERSION_MINOR(props.apiVersion) << "."
                    << VK_VERSION_PATCH(props.apiVersion) << "\n";
    }

    std::cout << "Creating logical devices...\n";
    r = manager.CreateDevice();
    if (r != VK_SUCCESS) {
        std::cout << "Failed to create VkDevice (error " << r << ")\n";
        return 1;
    }

    GPUCompressBCVk compressor = GPUCompressBCVk();

    std::cout << "Creating shaders...\n";
    r = compressor.Initialize(
            manager.GetDevice(),
            manager.GetUsingGPU(),
            manager.GetUsingFamilyId());
    if (r != VK_SUCCESS) {
        std::cout << "Failed to create VkShaderModule (error " << r << ")\n";
        return 1;
    }

    int res;
    res = TryCompression(
        &compressor,
        "example/R8G8B8A8_UNORM_512x512.dds",
        "BC7_result.dds");
    if (res != 0) return res;
    res = TryCompression(
        &compressor,
        "example/R32G32B32A32_FLOAT_512x512.dds",
        "BC6_result.dds");
    if (res != 0) return res;

    std::cout << "success\n";
    return 0;
}
