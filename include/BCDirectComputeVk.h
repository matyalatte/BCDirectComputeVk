#pragma once

#ifdef USE_VOLK
#include "volk.h"
#else
#include <vulkan/vulkan.h>
#endif

// GPUCompressBCVk: Class to perform BC compressions via Vulkan APIs

/* Example code
void main() {
    // Recommended to use VulkanDeviceManager.h to create VkDevice.
    VkDevice device;
    VkPhysicalDevice physical_device;
    uint32_t family_id;

    GPUCompressBCVk compressor = GPUCompressBCVk();

    VkResult r;
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
 */

#ifndef DXGI_FORMAT_DEFINED
enum DXGI_FORMAT : uint32_t;
#endif

class GPUCompressBCVk {
 public:
    GPUCompressBCVk();
    ~GPUCompressBCVk();

    // Create shaders.
    //   `physical_device` should be a physical device which `device` uses.
    //   `family_id` should be a queue family id which `device` uses.
    //   Ownership is not transferred. (~GPUCompressBCVk() does not destroy `device`.)
    VkResult Initialize(VkDevice device, VkPhysicalDevice physical_device, uint32_t family_id);

    // Create buffers.
    //   `flags` is TEX_COMPRESS_FLAGS (compression options.)
    //     (e.g. TEX_COMPRESS_BC7_QUICK can simplify BC7 compression.)
    //   `format` is a compressed format. (BC6H or BC7)
    VkResult Prepare(uint32_t width, uint32_t height, uint32_t flags, DXGI_FORMAT format, float alpha_weight);

    // After calling Prepare(), you can check required buffer sizes via these functions.
    uint32_t GetSrcBufSize() { return m_src_buf_size; }
    uint32_t GetOutBufSize() { return m_out_buf_size; }

    // Run shaders.
    //   The pixel format of `src_pixels` should be...
    //     - R32G32B32A32_FLOAT for BC6H compression.
    //     - R8G8B8A8_UNORM     for BC7 compression.
    //   The size of `src_pixels` should be GetSrcBufSize().
    //   The size of `out_pixels` should be GetOutBufSize().
    VkResult Compress(void* src_pixels, void* out_pixels);

 private:
    // activated device
    VkDevice m_device;
    VkQueue m_queue;
    VkCommandPool m_cmd_pool;
    VkPhysicalDeviceMemoryProperties m_memory_props;

    // shaders
    VkShaderModule m_shader_bc6_enc;
    VkShaderModule m_shader_bc6_modeG10;
    VkShaderModule m_shader_bc6_modeLE10;

    VkShaderModule m_shader_bc7_enc;
    VkShaderModule m_shader_bc7_mode02;
    VkShaderModule m_shader_bc7_mode137;
    VkShaderModule m_shader_bc7_mode456;

    // shader info
    VkDescriptorSetLayout m_desc_set_layout;
    VkDescriptorPool m_desc_pool;
    VkDescriptorSet m_desc_set;
    VkPipelineLayout m_pipeline_layout;

    // buffers
    VkBuffer m_const_buf;
    VkDeviceMemory m_const_mem;
    VkBuffer m_err1_buf;
    VkBuffer m_err2_buf;
    VkBuffer m_out_buf;
    VkDeviceMemory m_out_mem;
    VkBuffer m_outcpu_buf;
    VkDeviceMemory m_outcpu_mem;

    // texture info
    uint32_t m_width;
    uint32_t m_height;
    float m_alpha_weight;
    uint32_t m_out_buf_size;
    uint32_t m_src_buf_size;
    DXGI_FORMAT m_bcformat;
    DXGI_FORMAT m_srcformat;
    bool m_isbc7;
    bool m_bc7_mode02;
    bool m_bc7_mode137;

    // Free allocated objects by Prepare()
    void FreeBuffers();

    // Update VkBuffer for constants
    VkResult UpdateConstants(uint32_t xblocks, uint32_t mode_id, uint32_t start_block_id, uint32_t num_total_blocks);
    // Set image view and constant buffer for shaders.
    void SetImageViewAndConstBuf(VkImageView image_view, VkBuffer const_buf);
    // Use buf as an output buffer of shaders.
    void SetOutputBuffer(VkBuffer buf);
    // Use err_buf as input, and use out_buf as output.
    void SetErrorAndOutputBuffer(VkBuffer err_buf, VkBuffer out_buf);

    // Copy buf to GPU
    VkResult CopyToVkImage(VkCommandBuffer command_buffer,
                        VkBuffer image_cpu_buf,
                        VkDeviceMemory image_cpu_mem,
                        VkImage image,
                        void* buf, uint32_t buf_size);

    // Copy result to cpu memory
    VkResult CopyFromOutBuffer(VkCommandBuffer command_buffer, void* buf);
};

#ifndef DXGI_FORMAT_DEFINED
#define DXGI_FORMAT_DEFINED 1
enum DXGI_FORMAT : uint32_t {
    DXGI_FORMAT_UNKNOWN	                    = 0,
    DXGI_FORMAT_R32G32B32A32_TYPELESS       = 1,
    DXGI_FORMAT_R32G32B32A32_FLOAT          = 2,
    DXGI_FORMAT_R32G32B32A32_UINT           = 3,
    DXGI_FORMAT_R32G32B32A32_SINT           = 4,
    DXGI_FORMAT_R32G32B32_TYPELESS          = 5,
    DXGI_FORMAT_R32G32B32_FLOAT             = 6,
    DXGI_FORMAT_R32G32B32_UINT              = 7,
    DXGI_FORMAT_R32G32B32_SINT              = 8,
    DXGI_FORMAT_R16G16B16A16_TYPELESS       = 9,
    DXGI_FORMAT_R16G16B16A16_FLOAT          = 10,
    DXGI_FORMAT_R16G16B16A16_UNORM          = 11,
    DXGI_FORMAT_R16G16B16A16_UINT           = 12,
    DXGI_FORMAT_R16G16B16A16_SNORM          = 13,
    DXGI_FORMAT_R16G16B16A16_SINT           = 14,
    DXGI_FORMAT_R32G32_TYPELESS             = 15,
    DXGI_FORMAT_R32G32_FLOAT                = 16,
    DXGI_FORMAT_R32G32_UINT                 = 17,
    DXGI_FORMAT_R32G32_SINT                 = 18,
    DXGI_FORMAT_R32G8X24_TYPELESS           = 19,
    DXGI_FORMAT_D32_FLOAT_S8X24_UINT        = 20,
    DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS    = 21,
    DXGI_FORMAT_X32_TYPELESS_G8X24_UINT     = 22,
    DXGI_FORMAT_R10G10B10A2_TYPELESS        = 23,
    DXGI_FORMAT_R10G10B10A2_UNORM           = 24,
    DXGI_FORMAT_R10G10B10A2_UINT            = 25,
    DXGI_FORMAT_R11G11B10_FLOAT             = 26,
    DXGI_FORMAT_R8G8B8A8_TYPELESS           = 27,
    DXGI_FORMAT_R8G8B8A8_UNORM              = 28,
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB         = 29,
    DXGI_FORMAT_R8G8B8A8_UINT               = 30,
    DXGI_FORMAT_R8G8B8A8_SNORM              = 31,
    DXGI_FORMAT_R8G8B8A8_SINT               = 32,
    DXGI_FORMAT_R16G16_TYPELESS             = 33,
    DXGI_FORMAT_R16G16_FLOAT                = 34,
    DXGI_FORMAT_R16G16_UNORM                = 35,
    DXGI_FORMAT_R16G16_UINT                 = 36,
    DXGI_FORMAT_R16G16_SNORM                = 37,
    DXGI_FORMAT_R16G16_SINT                 = 38,
    DXGI_FORMAT_R32_TYPELESS                = 39,
    DXGI_FORMAT_D32_FLOAT                   = 40,
    DXGI_FORMAT_R32_FLOAT                   = 41,
    DXGI_FORMAT_R32_UINT                    = 42,
    DXGI_FORMAT_R32_SINT                    = 43,
    DXGI_FORMAT_R24G8_TYPELESS              = 44,
    DXGI_FORMAT_D24_UNORM_S8_UINT           = 45,
    DXGI_FORMAT_R24_UNORM_X8_TYPELESS       = 46,
    DXGI_FORMAT_X24_TYPELESS_G8_UINT        = 47,
    DXGI_FORMAT_R8G8_TYPELESS               = 48,
    DXGI_FORMAT_R8G8_UNORM                  = 49,
    DXGI_FORMAT_R8G8_UINT                   = 50,
    DXGI_FORMAT_R8G8_SNORM                  = 51,
    DXGI_FORMAT_R8G8_SINT                   = 52,
    DXGI_FORMAT_R16_TYPELESS                = 53,
    DXGI_FORMAT_R16_FLOAT                   = 54,
    DXGI_FORMAT_D16_UNORM                   = 55,
    DXGI_FORMAT_R16_UNORM                   = 56,
    DXGI_FORMAT_R16_UINT                    = 57,
    DXGI_FORMAT_R16_SNORM                   = 58,
    DXGI_FORMAT_R16_SINT                    = 59,
    DXGI_FORMAT_R8_TYPELESS                 = 60,
    DXGI_FORMAT_R8_UNORM                    = 61,
    DXGI_FORMAT_R8_UINT                     = 62,
    DXGI_FORMAT_R8_SNORM                    = 63,
    DXGI_FORMAT_R8_SINT                     = 64,
    DXGI_FORMAT_A8_UNORM                    = 65,
    DXGI_FORMAT_R1_UNORM                    = 66,
    DXGI_FORMAT_R9G9B9E5_SHAREDEXP          = 67,
    DXGI_FORMAT_R8G8_B8G8_UNORM             = 68,
    DXGI_FORMAT_G8R8_G8B8_UNORM             = 69,
    DXGI_FORMAT_BC1_TYPELESS                = 70,
    DXGI_FORMAT_BC1_UNORM                   = 71,
    DXGI_FORMAT_BC1_UNORM_SRGB              = 72,
    DXGI_FORMAT_BC2_TYPELESS                = 73,
    DXGI_FORMAT_BC2_UNORM                   = 74,
    DXGI_FORMAT_BC2_UNORM_SRGB              = 75,
    DXGI_FORMAT_BC3_TYPELESS                = 76,
    DXGI_FORMAT_BC3_UNORM                   = 77,
    DXGI_FORMAT_BC3_UNORM_SRGB              = 78,
    DXGI_FORMAT_BC4_TYPELESS                = 79,
    DXGI_FORMAT_BC4_UNORM                   = 80,
    DXGI_FORMAT_BC4_SNORM                   = 81,
    DXGI_FORMAT_BC5_TYPELESS                = 82,
    DXGI_FORMAT_BC5_UNORM                   = 83,
    DXGI_FORMAT_BC5_SNORM                   = 84,
    DXGI_FORMAT_B5G6R5_UNORM                = 85,
    DXGI_FORMAT_B5G5R5A1_UNORM              = 86,
    DXGI_FORMAT_B8G8R8A8_UNORM              = 87,
    DXGI_FORMAT_B8G8R8X8_UNORM              = 88,
    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM  = 89,
    DXGI_FORMAT_B8G8R8A8_TYPELESS           = 90,
    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB         = 91,
    DXGI_FORMAT_B8G8R8X8_TYPELESS           = 92,
    DXGI_FORMAT_B8G8R8X8_UNORM_SRGB         = 93,
    DXGI_FORMAT_BC6H_TYPELESS               = 94,
    DXGI_FORMAT_BC6H_UF16                   = 95,
    DXGI_FORMAT_BC6H_SF16                   = 96,
    DXGI_FORMAT_BC7_TYPELESS                = 97,
    DXGI_FORMAT_BC7_UNORM                   = 98,
    DXGI_FORMAT_BC7_UNORM_SRGB              = 99,
    DXGI_FORMAT_AYUV                        = 100,
    DXGI_FORMAT_Y410                        = 101,
    DXGI_FORMAT_Y416                        = 102,
    DXGI_FORMAT_NV12                        = 103,
    DXGI_FORMAT_P010                        = 104,
    DXGI_FORMAT_P016                        = 105,
    DXGI_FORMAT_420_OPAQUE                  = 106,
    DXGI_FORMAT_YUY2                        = 107,
    DXGI_FORMAT_Y210                        = 108,
    DXGI_FORMAT_Y216                        = 109,
    DXGI_FORMAT_NV11                        = 110,
    DXGI_FORMAT_AI44                        = 111,
    DXGI_FORMAT_IA44                        = 112,
    DXGI_FORMAT_P8                          = 113,
    DXGI_FORMAT_A8P8                        = 114,
    DXGI_FORMAT_B4G4R4A4_UNORM              = 115,

    DXGI_FORMAT_P208                        = 130,
    DXGI_FORMAT_V208                        = 131,
    DXGI_FORMAT_V408                        = 132,


    DXGI_FORMAT_FORCE_UINT                  = 0xffffffff
};
#endif
