#include "BCDirectComputeVk.h"

// for std::max
#include <algorithm>

// for memcpy and free
#include <stdlib.h>
#include <string.h>

#include "BC6HEncode_EncodeBlockCS.inc"
#include "BC6HEncode_TryModeG10CS.inc"
#include "BC6HEncode_TryModeLE10CS.inc"

#include "BC7Encode_EncodeBlockCS.inc"
#include "BC7Encode_TryMode02CS.inc"
#include "BC7Encode_TryMode137CS.inc"
#include "BC7Encode_TryMode456CS.inc"

struct BufferBC6HBC7 {
    uint32_t color[4];
};

struct ConstantsBC6HBC7 {
    uint32_t    tex_width;
    uint32_t    num_block_x;
    uint32_t    format;
    uint32_t    mode_id;
    uint32_t    start_block_id;
    uint32_t    num_total_blocks;
    float   alpha_weight;
    uint32_t    reserved;
};

static_assert(sizeof(ConstantsBC6HBC7) == sizeof(uint32_t) * 8, "Constant buffer size mismatch");

GPUCompressBCVk::GPUCompressBCVk() {
    m_device = VK_NULL_HANDLE;
    m_queue = VK_NULL_HANDLE;
    m_cmd_pool = VK_NULL_HANDLE;
    m_memory_props = {};

    m_shader_bc6_enc = VK_NULL_HANDLE;
    m_shader_bc6_modeG10 = VK_NULL_HANDLE;
    m_shader_bc6_modeLE10 = VK_NULL_HANDLE;
    m_shader_bc7_enc = VK_NULL_HANDLE;
    m_shader_bc7_mode02 = VK_NULL_HANDLE;
    m_shader_bc7_mode137 = VK_NULL_HANDLE;
    m_shader_bc7_mode456 = VK_NULL_HANDLE;

    m_desc_set_layout = VK_NULL_HANDLE;
    m_desc_pool = VK_NULL_HANDLE;
    m_desc_set = VK_NULL_HANDLE;
    m_pipeline_layout = VK_NULL_HANDLE;

    m_const_buf = VK_NULL_HANDLE;
    m_const_mem = VK_NULL_HANDLE;
    m_err1_buf = VK_NULL_HANDLE;
    m_err1_mem = VK_NULL_HANDLE;
    m_err2_buf = VK_NULL_HANDLE;
    m_err2_mem = VK_NULL_HANDLE;
    m_out_buf = VK_NULL_HANDLE;
    m_out_mem = VK_NULL_HANDLE;
    m_outcpu_buf = VK_NULL_HANDLE;
    m_outcpu_mem = VK_NULL_HANDLE;

    m_width = 0;
    m_height = 0;
    m_alpha_weight = 1.0f;
    m_bcformat = DXGI_FORMAT_UNKNOWN;
    m_out_buf_size = 0;
}

void GPUCompressBCVk::FreeBuffers() {
    if (m_device == VK_NULL_HANDLE)
        return;
    vkFreeMemory(m_device, m_const_mem, 0);
    vkDestroyBuffer(m_device, m_const_buf, 0);
    vkFreeMemory(m_device, m_err1_mem, 0);
    vkDestroyBuffer(m_device, m_err1_buf, 0);
    vkFreeMemory(m_device, m_err2_mem, 0);
    vkDestroyBuffer(m_device, m_err2_buf, 0);
    vkFreeMemory(m_device, m_out_mem, 0);
    vkDestroyBuffer(m_device, m_out_buf, 0);
    vkFreeMemory(m_device, m_outcpu_mem, 0);
    vkDestroyBuffer(m_device, m_outcpu_buf, 0);
    m_const_mem = VK_NULL_HANDLE;
    m_const_buf = VK_NULL_HANDLE;
    m_err1_mem = VK_NULL_HANDLE;
    m_err1_buf = VK_NULL_HANDLE;
    m_err2_mem = VK_NULL_HANDLE;
    m_err2_buf = VK_NULL_HANDLE;
    m_out_mem = VK_NULL_HANDLE;
    m_out_buf = VK_NULL_HANDLE;
    m_outcpu_mem = VK_NULL_HANDLE;
    m_outcpu_buf = VK_NULL_HANDLE;
}

GPUCompressBCVk::~GPUCompressBCVk() {
    if (m_device != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, m_cmd_pool, nullptr);
        m_cmd_pool = VK_NULL_HANDLE;

        vkDestroyShaderModule(m_device, m_shader_bc6_enc, 0);
        vkDestroyShaderModule(m_device, m_shader_bc6_modeG10, 0);
        vkDestroyShaderModule(m_device, m_shader_bc6_modeLE10, 0);
        vkDestroyShaderModule(m_device, m_shader_bc7_enc, 0);
        vkDestroyShaderModule(m_device, m_shader_bc7_mode02, 0);
        vkDestroyShaderModule(m_device, m_shader_bc7_mode137, 0);
        vkDestroyShaderModule(m_device, m_shader_bc7_mode456, 0);
        m_shader_bc6_enc = VK_NULL_HANDLE;
        m_shader_bc6_modeG10 = VK_NULL_HANDLE;
        m_shader_bc6_modeLE10 = VK_NULL_HANDLE;
        m_shader_bc7_enc = VK_NULL_HANDLE;
        m_shader_bc7_mode02 = VK_NULL_HANDLE;
        m_shader_bc7_mode137 = VK_NULL_HANDLE;
        m_shader_bc7_mode456 = VK_NULL_HANDLE;

        vkDestroyDescriptorSetLayout(m_device, m_desc_set_layout, 0);
        m_desc_set_layout = VK_NULL_HANDLE;

        vkDestroyDescriptorPool(m_device, m_desc_pool, 0);
        m_desc_pool = VK_NULL_HANDLE;

        vkDestroyPipelineLayout(m_device, m_pipeline_layout, 0);
        m_pipeline_layout = VK_NULL_HANDLE;

        FreeBuffers();

        m_device = VK_NULL_HANDLE;
        m_queue = VK_NULL_HANDLE;
    }
}

VkResult CreateVkShaderModule(VkDevice device, VkShaderModule* module,
                                const unsigned char* code, uint32_t code_size) {
    VkShaderModuleCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.pNext = 0;
    ci.flags = 0;
    ci.codeSize = code_size;
    ci.pCode = (const uint32_t*)code;

    *module = VK_NULL_HANDLE;
    return vkCreateShaderModule(device, &ci, 0, module);
}

VkResult CreateVkDescriptorPool(VkDevice device,
                                VkDescriptorPool* descriptor_pool, VkDescriptorSet* descriptor_set,
                                VkDescriptorSetLayout dsl,
                                VkDescriptorPoolSize* pool_sizes, uint32_t pool_size_count) {
    VkDescriptorPoolCreateInfo dpci = {};
    dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.pNext = 0;
    dpci.flags = 0;
    dpci.maxSets = 1;
    dpci.poolSizeCount = pool_size_count;
    dpci.pPoolSizes = pool_sizes;

    VkResult r = vkCreateDescriptorPool(device, &dpci, 0, descriptor_pool);
    if (r != VK_SUCCESS)
        return r;

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo dsai = {};
    dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.pNext = 0;
    dsai.descriptorPool = *descriptor_pool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &dsl;

    return vkAllocateDescriptorSets(device, &dsai, descriptor_set);
}

VkResult CreateVkPipelineLayout(VkDevice device,
                                VkPipelineLayout* pipe_layout,
                                VkDescriptorSetLayout desc_set_layout) {
    VkPipelineLayoutCreateInfo plci = {};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.pNext = 0;
    plci.flags = 0;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &desc_set_layout;
    plci.pushConstantRangeCount = 0;
    plci.pPushConstantRanges = 0;
    return vkCreatePipelineLayout(device, &plci, 0, pipe_layout);
}

VkResult GPUCompressBCVk::Initialize(
        VkDevice device,
        VkPhysicalDevice physical_device,
        uint32_t family_id) {

    if (device == VK_NULL_HANDLE || physical_device == VK_NULL_HANDLE)
        return VK_ERROR_UNKNOWN;  // Invalid args

    if (m_device != VK_NULL_HANDLE)
        return VK_ERROR_UNKNOWN;  // Initialized already

    VkResult r = VK_SUCCESS;
    m_device = device;

    vkGetPhysicalDeviceMemoryProperties(physical_device, &m_memory_props);
    vkGetDeviceQueue(m_device, family_id, 0, &m_queue);

    VkCommandPoolCreateInfo command_pool_create_info = {};
    command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_create_info.queueFamilyIndex = family_id;
    command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    r = vkCreateCommandPool(m_device, &command_pool_create_info, nullptr, &m_cmd_pool);
    if (r != VK_SUCCESS)
        return r;

    // Create shader modules
    r = CreateVkShaderModule(m_device, &m_shader_bc6_enc, BC6HEncode_EncodeBlockCS, sizeof(BC6HEncode_EncodeBlockCS));
    if (r != VK_SUCCESS)
        return r;

    r = CreateVkShaderModule(m_device, &m_shader_bc6_modeG10, BC6HEncode_TryModeG10CS, sizeof(BC6HEncode_TryModeG10CS));
    if (r != VK_SUCCESS)
        return r;

    r = CreateVkShaderModule(m_device, &m_shader_bc6_modeLE10, BC6HEncode_TryModeLE10CS, sizeof(BC6HEncode_TryModeLE10CS));
    if (r != VK_SUCCESS)
        return r;

    r = CreateVkShaderModule(m_device, &m_shader_bc7_enc, BC7Encode_EncodeBlockCS, sizeof(BC7Encode_EncodeBlockCS));
    if (r != VK_SUCCESS)
        return r;

    r = CreateVkShaderModule(m_device, &m_shader_bc7_mode02, BC7Encode_TryMode02CS, sizeof(BC7Encode_TryMode02CS));
    if (r != VK_SUCCESS)
        return r;

    r = CreateVkShaderModule(m_device, &m_shader_bc7_mode137, BC7Encode_TryMode137CS, sizeof(BC7Encode_TryMode137CS));
    if (r != VK_SUCCESS)
        return r;

    r = CreateVkShaderModule(m_device, &m_shader_bc7_mode456, BC7Encode_TryMode456CS, sizeof(BC7Encode_TryMode456CS));
    if (r != VK_SUCCESS)
        return r;

    // Create descriptor layout
    VkDescriptorSetLayoutBinding bindings[4] = {
        // t0: g_Input (source texture)
        { 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT },

        // t1: g_InBuff (error)
        { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT },

        // u0: g_OutBuff (output)
        { 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT },

        // b0: cbCS (constants)
        { 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT }
    };

    VkDescriptorSetLayoutCreateInfo dslci = {};
    dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslci.pNext = 0;
    dslci.flags = 0;
    dslci.bindingCount = 4;
    dslci.pBindings = bindings;

    r = vkCreateDescriptorSetLayout(m_device, &dslci, 0, &m_desc_set_layout);
    if (r != VK_SUCCESS)
        return r;

    // Create descriptor pool
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
    };
    r = CreateVkDescriptorPool(m_device, &m_desc_pool, &m_desc_set, m_desc_set_layout, pool_sizes, 4);
    if (r != VK_SUCCESS)
        return r;

    // Create pipeline layout
    r = CreateVkPipelineLayout(m_device, &m_pipeline_layout, m_desc_set_layout);

    return r;
}

DXGI_FORMAT BcFormatToSrcFormat(DXGI_FORMAT format) {
    switch (format)
    {
        // BC6H GPU compressor takes RGBAF32 as input
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;

        // BC7 GPU compressor takes RGBA32 as input
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
        return DXGI_FORMAT_R8G8B8A8_UNORM;

    case DXGI_FORMAT_BC7_UNORM_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    default:
        break;
    }

    return DXGI_FORMAT_UNKNOWN;
}

bool IsBC7(DXGI_FORMAT format) {
    switch (format)
    {
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        return true;
    default:
        break;
    }
    return false;
}

inline uint32_t FindMemoryType(
        VkPhysicalDeviceMemoryProperties* memory_props,
        uint32_t type_bits, VkMemoryPropertyFlags flags) {
    uint32_t type_id = -1;
    for (uint32_t i = 0; i < memory_props->memoryTypeCount; i++ ) {
        if ((type_bits & 1 ) && ((memory_props->memoryTypes[i].propertyFlags & flags) == flags)) {
            type_id = i;
            break;
        }
        type_bits >>= 1;
    }
    return type_id;
}

VkResult CreateVkBuffer(VkDevice device,
                        VkBuffer* buf, VkDeviceSize buf_size, VkBufferUsageFlags buf_usage,
                        VkDeviceMemory* mem,
                        VkPhysicalDeviceMemoryProperties* mem_props,
                        VkMemoryPropertyFlags mem_flags) {
    VkBufferCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size  = buf_size;
    info.usage = buf_usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult r = vkCreateBuffer(device, &info, 0, buf);
    if (r != VK_SUCCESS)
        return r;

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, *buf, &req);

    VkMemoryAllocateInfo alloc = {};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = FindMemoryType(mem_props, req.memoryTypeBits, mem_flags);

    r = vkAllocateMemory(device, &alloc, nullptr, mem);
    if (r != VK_SUCCESS)
        return r;

    return vkBindBufferMemory(device, *buf, *mem, 0);
}

enum TEX_COMPRESS_FLAGS : uint32_t {
    TEX_COMPRESS_DEFAULT = 0,

    TEX_COMPRESS_RGB_DITHER = 0x10000,
    // Enables dithering RGB colors for BC1-3 compression

    TEX_COMPRESS_A_DITHER = 0x20000,
    // Enables dithering alpha for BC1-3 compression

    TEX_COMPRESS_DITHER = 0x30000,
    // Enables both RGB and alpha dithering for BC1-3 compression

    TEX_COMPRESS_UNIFORM = 0x40000,
    // Uniform color weighting for BC1-3 compression; by default uses perceptual weighting

    TEX_COMPRESS_BC7_USE_3SUBSETS = 0x80000,
    // Enables exhaustive search for BC7 compress for mode 0 and 2; by default skips trying these modes

    TEX_COMPRESS_BC7_QUICK = 0x100000,
    // Minimal modes (usually mode 6) for BC7 compression

    TEX_COMPRESS_SRGB_IN = 0x1000000,
    TEX_COMPRESS_SRGB_OUT = 0x2000000,
    TEX_COMPRESS_SRGB = (TEX_COMPRESS_SRGB_IN | TEX_COMPRESS_SRGB_OUT),
    // if the input format type is IsSRGB(), then SRGB_IN is on by default
    // if the output format type is IsSRGB(), then SRGB_OUT is on by default

    TEX_COMPRESS_PARALLEL = 0x10000000,
    // Compress is free to use multithreading to improve performance (by default it does not use multithreading)
};

VkResult GPUCompressBCVk::Prepare(uint32_t width, uint32_t height, uint32_t flags, DXGI_FORMAT format, float alpha_weight) {
    VkResult r = VK_SUCCESS;

    if (!width || !height || alpha_weight < 0.f)
        return VK_ERROR_UNKNOWN;  // Invalid args

    if ((width > UINT32_MAX) || (height > UINT32_MAX))
        return VK_ERROR_UNKNOWN;  // Invalid args

    if (m_pipeline_layout == VK_NULL_HANDLE)
        return VK_ERROR_UNKNOWN;  // GPUCompressBCVk::Initialize() is not called yet (or failed.)

    FreeBuffers();

    m_width = width;
    m_height = height;
    m_alpha_weight = alpha_weight;

    if (flags & TEX_COMPRESS_BC7_QUICK) {
        m_bc7_mode02 = false;
        m_bc7_mode137 = false;
    } else {
        m_bc7_mode02 = (flags & TEX_COMPRESS_BC7_USE_3SUBSETS) != 0;
        m_bc7_mode137 = true;
    }

    m_srcformat = BcFormatToSrcFormat(format);
    if (m_srcformat == DXGI_FORMAT_UNKNOWN) {
        m_bcformat = DXGI_FORMAT_UNKNOWN;
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }
    m_bcformat = format;
    m_isbc7 = IsBC7(m_bcformat);
    m_src_buf_size = m_width * m_height * (m_isbc7 ? 4 : 16);

    // Constants
    r = CreateVkBuffer(m_device,
                    &m_const_buf,
                    sizeof(ConstantsBC6HBC7),
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    &m_const_mem,
                    &m_memory_props,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (r != VK_SUCCESS)
        return r;

    // Outputs for GPU
    const size_t xblocks = std::max<size_t>(1, (width + 3) >> 2);
    const size_t yblocks = std::max<size_t>(1, (height + 3) >> 2);
    const size_t num_blocks = xblocks * yblocks;
    VkDeviceSize buf_size = VkDeviceSize(num_blocks) * sizeof(BufferBC6HBC7);
    m_out_buf_size = (uint32_t)buf_size;
    VkBufferUsageFlags buf_usage =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    r = CreateVkBuffer(m_device,
                    &m_err1_buf,
                    buf_size,
                    buf_usage,
                    &m_err1_mem,
                    &m_memory_props,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (r != VK_SUCCESS)
        return r;
    r = CreateVkBuffer(m_device,
                    &m_err2_buf,
                    buf_size,
                    buf_usage,
                    &m_err2_mem,
                    &m_memory_props,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (r != VK_SUCCESS)
        return r;
    r = CreateVkBuffer(m_device,
                    &m_out_buf,
                    buf_size,
                    buf_usage,
                    &m_out_mem,
                    &m_memory_props,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (r != VK_SUCCESS)
        return r;

    // Output for CPU
    r = CreateVkBuffer(m_device,
                    &m_outcpu_buf,
                    buf_size,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    &m_outcpu_mem,
                    &m_memory_props,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    return r;
}

VkResult GPUCompressBCVk::UpdateConstants(uint32_t xblocks, uint32_t mode_id, uint32_t start_block_id, uint32_t num_total_blocks) {
    ConstantsBC6HBC7 param = {};
    param.tex_width = static_cast<uint32_t>(m_width);
    param.num_block_x = xblocks;
    param.format = static_cast<uint32_t>(m_bcformat);
    param.mode_id = mode_id;
    param.start_block_id = start_block_id;
    param.num_total_blocks = num_total_blocks;
    param.alpha_weight = m_alpha_weight;
    void* data;
    VkResult r = vkMapMemory(m_device, m_const_mem, 0, sizeof(ConstantsBC6HBC7), 0, &data);
    if (r == VK_SUCCESS) {
        memcpy(data, &param, sizeof(param));
        vkUnmapMemory(m_device, m_const_mem);
    }
    return r;
}

// Set image view and constant buffer for shaders.
void GPUCompressBCVk::SetImageViewAndConstBuf(VkImageView image_view, VkBuffer const_buf) {
    VkDescriptorImageInfo img_info = {};
    img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    img_info.imageView = image_view;

    VkDescriptorBufferInfo const_buf_info = {};
    const_buf_info.buffer = const_buf;
    const_buf_info.offset = 0;
    const_buf_info.range = sizeof(ConstantsBC6HBC7);

    VkWriteDescriptorSet writes[] = {
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
            m_desc_set, 0, 0, 1,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &img_info
        },
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
            m_desc_set, 3, 0, 1,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &const_buf_info
        }
    };
    vkUpdateDescriptorSets(m_device, 2, writes, 0, 0);
}

// Use buf as an output buffer of shaders.
void GPUCompressBCVk::SetOutputBuffer(VkBuffer buf) {
    VkDescriptorBufferInfo buf_info = {};
    buf_info.buffer = buf;
    buf_info.offset = 0;
    buf_info.range = VK_WHOLE_SIZE;
    VkWriteDescriptorSet writes[] = {
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
            m_desc_set, 2, 0, 1,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &buf_info
        }
    };

    vkUpdateDescriptorSets(m_device, 1, writes, 0, 0);
}

// Use err_buf as input and use out_buf as output.
void GPUCompressBCVk::SetErrorAndOutputBuffer(VkBuffer err_buf, VkBuffer out_buf) {
    VkDescriptorBufferInfo err_buf_info = {};
    err_buf_info.buffer = err_buf;
    err_buf_info.offset = 0;
    err_buf_info.range = VK_WHOLE_SIZE;
    VkDescriptorBufferInfo out_buf_info = {};
    out_buf_info.buffer = out_buf;
    out_buf_info.offset = 0;
    out_buf_info.range = VK_WHOLE_SIZE;
    VkWriteDescriptorSet writes[] = {
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
            m_desc_set, 1, 0, 1,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &err_buf_info
        },
        {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
            m_desc_set, 2, 0, 1,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &out_buf_info
        }
    };

    vkUpdateDescriptorSets(m_device, 2, writes, 0, 0);
}

void ChangeImageLayout(VkCommandBuffer command_buffer, VkImage image,
                        VkImageLayout old_layout, VkImageLayout new_layout,
                        VkAccessFlagBits dst_access_mask,
                        VkPipelineStageFlags dst_stage_mask) {
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {
            VK_IMAGE_ASPECT_COLOR_BIT,
            0,
            1,
            0,
            1,
        };
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = dst_access_mask;

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        dst_stage_mask,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );
}

VkResult RunCommand(VkQueue queue, VkCommandBuffer command_buffer) {
    VkSubmitInfo si = {};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.pNext = 0;
    si.waitSemaphoreCount = 0;
    si.pWaitSemaphores = 0;
    si.pWaitDstStageMask = 0;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &command_buffer;
    si.signalSemaphoreCount = 0;
    si.pSignalSemaphores = 0;

    VkResult r = vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
    if (r != VK_SUCCESS)
        return r;
    return vkQueueWaitIdle(queue);
}

VkResult GPUCompressBCVk::CopyToVkImage(
        VkCommandBuffer command_buffer,
        VkBuffer image_cpu_buf,
        VkDeviceMemory image_cpu_mem,
        VkImage image,
        void* buf, uint32_t buf_size) {
    // Copy c buffer to host visible VkBuffer
    void* data;
    VkResult r = vkMapMemory(m_device, image_cpu_mem, 0, buf_size, 0, &data);
    if (r == VK_SUCCESS) {
        memcpy(data, buf, buf_size);
        vkUnmapMemory(m_device, image_cpu_mem);
    } else {
        return r;
    }

    // Copy host visible VkBuffer to local VkImage
    VkCommandBufferBeginInfo cbi = {};
    cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cbi.pNext = 0;
    cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    cbi.pInheritanceInfo = 0;

    r = vkBeginCommandBuffer(command_buffer, &cbi);
    if (r != VK_SUCCESS)
        return r;
    ChangeImageLayout(command_buffer, image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT);

    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = { m_width, m_height, 1 };
    vkCmdCopyBufferToImage(
        command_buffer,
        image_cpu_buf,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    ChangeImageLayout(command_buffer, image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    r = vkEndCommandBuffer(command_buffer);
    if (r != VK_SUCCESS)
        return r;

    return RunCommand(m_queue, command_buffer);
}

// Copy result to cpu memory
VkResult GPUCompressBCVk::CopyFromOutBuffer(VkCommandBuffer command_buffer, void* buf) {
    // Copy local VkBuffer to host visible VkBuffer
    VkCommandBufferBeginInfo cbi = {};
    cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cbi.pNext = 0;
    cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    cbi.pInheritanceInfo = 0;

    VkResult r = vkBeginCommandBuffer(command_buffer, &cbi);
    if (r != VK_SUCCESS)
        return r;

    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = m_out_buf;
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        1, &barrier,
        0, nullptr
    );

    uint32_t buf_size = (uint32_t)GetOutBufSize();
    VkBufferCopy region = { 0, 0, buf_size };
    vkCmdCopyBuffer(
        command_buffer,
        m_out_buf,
        m_outcpu_buf,
        1,
        &region
    );

    r = vkEndCommandBuffer(command_buffer);
    if (r != VK_SUCCESS)
        return r;

    r = RunCommand(m_queue, command_buffer);
    if (r != VK_SUCCESS)
        return r;

    // Copy host visible VkBuffer to c buffer
    void* data;
    r = vkMapMemory(m_device, m_outcpu_mem, 0, VK_WHOLE_SIZE, 0, &data);
    if (r == VK_SUCCESS) {
        memcpy(buf, data, buf_size);
        vkUnmapMemory(m_device, m_outcpu_mem);
    }
    return r;
}

VkFormat SrcFormatToVkFormat(DXGI_FORMAT format) {
    if (format == DXGI_FORMAT_R32G32B32A32_FLOAT)
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    return VK_FORMAT_R8G8B8A8_UNORM;
}

VkResult RunComputeShader(VkCommandBuffer command_buffer, VkQueue queue,
                        VkPipeline pipeline, VkPipelineLayout pipeline_layout,
                        VkDescriptorSet descriptor_set,
                        uint32_t dispatch_x) {
    VkResult r = VK_SUCCESS;
    VkCommandBufferBeginInfo cbi = {};
    cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cbi.pNext = 0;
    cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    cbi.pInheritanceInfo = 0;

    r = vkBeginCommandBuffer(command_buffer, &cbi);
    if (r != VK_SUCCESS)
        return r;

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
                            0, 1, &descriptor_set, 0, 0);
    vkCmdDispatch(command_buffer, dispatch_x, 1, 1);

    r = vkEndCommandBuffer(command_buffer);
    if (r != VK_SUCCESS)
        return r;

    r = RunCommand(queue, command_buffer);
    return r;
}

VkResult CreateVkPipeline(VkDevice device, VkPipeline* pipeline,
                            VkShaderModule shader_module,
                            const char* entry_point,
                            VkPipelineLayout pipeline_layout) {
    VkPipelineShaderStageCreateInfo ssi = {};
    ssi.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ssi.pNext = 0;
    ssi.flags = 0;
    ssi.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    ssi.module = shader_module;
    ssi.pName = entry_point;
    ssi.pSpecializationInfo = 0;

    VkComputePipelineCreateInfo cpci = {};
    cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cpci.pNext = 0;
    cpci.flags = 0;
    cpci.stage = ssi;
    cpci.layout = pipeline_layout;
    cpci.basePipelineHandle = VK_NULL_HANDLE;
    cpci.basePipelineIndex = -1;

    return vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &cpci, 0, pipeline);
}

VkResult CreateVkImage(VkDevice device, VkImage* image,
                        uint32_t width, uint32_t height, VkFormat format,
                        VkDeviceMemory* mem,
                        VkPhysicalDeviceMemoryProperties* mem_props,
                        VkMemoryPropertyFlags mem_flags) {

    VkImageCreateInfo img_info = {};
    img_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    img_info.imageType = VK_IMAGE_TYPE_2D;
    img_info.format = format;
    img_info.extent = { width, height, 1 };
    img_info.mipLevels = 1;
    img_info.arrayLayers = 1;
    img_info.samples = VK_SAMPLE_COUNT_1_BIT;
    img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    img_info.usage =
        VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VkResult r = vkCreateImage(device, &img_info, nullptr, image);
    if (r != VK_SUCCESS)
        return r;

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(device, *image, &req);

    VkMemoryAllocateInfo alloc = {};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = FindMemoryType(mem_props, req.memoryTypeBits, mem_flags);

    r = vkAllocateMemory(device, &alloc, nullptr, mem);
    if (r != VK_SUCCESS)
        return r;

    return vkBindImageMemory(device, *image, *mem, 0);
}

VkResult CreateVkImageView(VkDevice device, VkImageView* image_view,
                            VkImage image, VkFormat format) {
    VkImageViewCreateInfo ivci = {};
    ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivci.pNext = 0;
    ivci.flags = 0;
    ivci.image = image;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = format;
    ivci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    ivci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    ivci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    ivci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ivci.subresourceRange.baseMipLevel = 0;
    ivci.subresourceRange.levelCount = 1;
    ivci.subresourceRange.baseArrayLayer = 0;
    ivci.subresourceRange.layerCount = 1;

    return vkCreateImageView(device, &ivci, 0, image_view);
}

VkResult AllocateVkCommandBuffer(VkDevice device, VkCommandBuffer* command_buffer,
                                    VkCommandPool command_pool) {
    VkCommandBufferAllocateInfo cbai = {};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.pNext = 0;
    cbai.commandPool = command_pool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;

    return vkAllocateCommandBuffers(device, &cbai, command_buffer);
}

VkResult GPUCompressBCVk::Compress(void* src_pixels, void* out_pixels) {
    VkResult r = VK_SUCCESS;

    if (!src_pixels || !out_pixels)
        return VK_ERROR_UNKNOWN;

    VkFormat src_format = SrcFormatToVkFormat(m_srcformat);

    constexpr uint32_t MAX_BLOCK_BATCH = 64u;
    const size_t xblocks = std::max<size_t>(1, (m_width + 3) >> 2);
    const size_t yblocks = std::max<size_t>(1, (m_height + 3) >> 2);

    const auto num_total_blocks = static_cast<uint32_t>(xblocks * yblocks);
    uint32_t num_blocks = num_total_blocks;
    uint32_t start_block_id = 0;

    // Pipelines
    VkPipeline pipeline_mode456_G10 = VK_NULL_HANDLE;
    VkPipeline pipeline_mode137_LE10 = VK_NULL_HANDLE;
    VkPipeline pipeline_mode02 = VK_NULL_HANDLE;
    VkPipeline pipeline_enc = VK_NULL_HANDLE;

    // Host visible image buffer
    VkBuffer src_image_cpu = VK_NULL_HANDLE;
    VkDeviceMemory src_image_cpu_memory = VK_NULL_HANDLE;

    // Image buffer for GPU
    VkImage src_image = VK_NULL_HANDLE;
    VkDeviceMemory src_image_memory = VK_NULL_HANDLE;
    VkImageView src_image_view = VK_NULL_HANDLE;

    VkCommandBuffer command_buffer = VK_NULL_HANDLE;

    // Create objects
    if (m_isbc7) {
        r = CreateVkPipeline(m_device, &pipeline_mode456_G10, m_shader_bc7_mode456, "TryMode456CS", m_pipeline_layout);
        if (r != VK_SUCCESS)
            goto COMPUTE_END;

        r = CreateVkPipeline(m_device, &pipeline_mode137_LE10, m_shader_bc7_mode137, "TryMode137CS", m_pipeline_layout);
        if (r != VK_SUCCESS)
            goto COMPUTE_END;

        r = CreateVkPipeline(m_device, &pipeline_mode02, m_shader_bc7_mode02, "TryMode02CS", m_pipeline_layout);
        if (r != VK_SUCCESS)
            goto COMPUTE_END;

        r = CreateVkPipeline(m_device, &pipeline_enc, m_shader_bc7_enc, "EncodeBlockCS", m_pipeline_layout);
        if (r != VK_SUCCESS)
            goto COMPUTE_END;
    } else {
        r = CreateVkPipeline(m_device, &pipeline_mode456_G10, m_shader_bc6_modeG10, "TryModeG10CS", m_pipeline_layout);
        if (r != VK_SUCCESS)
            goto COMPUTE_END;

        r = CreateVkPipeline(m_device, &pipeline_mode137_LE10, m_shader_bc6_modeLE10, "TryModeLE10CS", m_pipeline_layout);
        if (r != VK_SUCCESS)
            goto COMPUTE_END;

        r = CreateVkPipeline(m_device, &pipeline_enc, m_shader_bc6_enc, "EncodeBlockCS", m_pipeline_layout);
        if (r != VK_SUCCESS)
            goto COMPUTE_END;
    }

    r = CreateVkBuffer(m_device,
                    &src_image_cpu,
                    m_src_buf_size,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    &src_image_cpu_memory,
                    &m_memory_props,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (r != VK_SUCCESS)
        goto COMPUTE_END;

    r = CreateVkImage(m_device, &src_image,
                m_width, m_height, src_format,
                &src_image_memory, &m_memory_props,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (r != VK_SUCCESS)
        goto COMPUTE_END;

    r = CreateVkImageView(m_device, &src_image_view, src_image, src_format);
    if (r != VK_SUCCESS)
        goto COMPUTE_END;

    r = AllocateVkCommandBuffer(m_device, &command_buffer, m_cmd_pool);
    if (r != VK_SUCCESS)
        goto COMPUTE_END;

    // Copy src_pixels to GPU
    r = CopyToVkImage(
        command_buffer,
        src_image_cpu, src_image_cpu_memory, src_image,
        src_pixels, (uint32_t)m_src_buf_size);

    SetImageViewAndConstBuf(src_image_view, m_const_buf);

    while (num_blocks > 0) {
        const uint32_t n = std::min<uint32_t>(num_blocks, MAX_BLOCK_BATCH);
        const uint32_t uThreadGroupCount = n;
        r = UpdateConstants((uint32_t)xblocks, 0, start_block_id, num_total_blocks);
        if (r != VK_SUCCESS)
            goto COMPUTE_END;

        if (m_isbc7) {
            // BC7
            // Try mode456
            SetOutputBuffer(m_err1_buf);
            r = RunComputeShader(command_buffer, m_queue,
                                pipeline_mode456_G10, m_pipeline_layout, m_desc_set,
                                std::max<uint32_t>((uThreadGroupCount + 3) / 4, 1));
            if (r != VK_SUCCESS)
                goto COMPUTE_END;

            if (m_bc7_mode137) {
                // Try mode137
                for (uint32_t i = 0; i < 3; ++i) {
                    static const uint32_t modes[] = { 1, 3, 7 };
                    r = UpdateConstants((uint32_t)xblocks, modes[i], start_block_id, num_total_blocks);
                    if (r != VK_SUCCESS)
                        goto COMPUTE_END;
                    SetErrorAndOutputBuffer(
                        (i & 1) ? m_err2_buf : m_err1_buf,
                        (i & 1) ? m_err1_buf : m_err2_buf);
                    r = RunComputeShader(command_buffer, m_queue,
                                    pipeline_mode137_LE10, m_pipeline_layout, m_desc_set,
                                    uThreadGroupCount);
                    if (r != VK_SUCCESS)
                        goto COMPUTE_END;
                }
            }

            if (m_bc7_mode02) {
                // Try mode02
                for (uint32_t i = 0; i < 2; ++i) {
                    static const uint32_t modes[] = { 0, 2 };
                    r = UpdateConstants((uint32_t)xblocks, modes[i], start_block_id, num_total_blocks);
                    if (r != VK_SUCCESS)
                        goto COMPUTE_END;
                    SetErrorAndOutputBuffer(
                        (i & 1) ? m_err1_buf : m_err2_buf,
                        (i & 1) ? m_err2_buf : m_err1_buf);
                    r = RunComputeShader(command_buffer, m_queue,
                                    pipeline_mode02, m_pipeline_layout, m_desc_set,
                                    uThreadGroupCount);
                    if (r != VK_SUCCESS)
                        goto COMPUTE_END;
                }
            }

            // Encode
            SetErrorAndOutputBuffer(m_err1_buf, m_out_buf);
            r = RunComputeShader(command_buffer, m_queue,
                                pipeline_enc, m_pipeline_layout, m_desc_set,
                                std::max<uint32_t>((uThreadGroupCount + 3) / 4, 1));
            if (r != VK_SUCCESS)
                goto COMPUTE_END;
        } else {
            // BC6H
            // Try modeG10

            SetOutputBuffer(m_err1_buf);
            r = RunComputeShader(command_buffer, m_queue,
                                pipeline_mode456_G10, m_pipeline_layout, m_desc_set,
                                std::max<uint32_t>((uThreadGroupCount + 3) / 4, 1));
            if (r != VK_SUCCESS)
                goto COMPUTE_END;

            // Try modeLE10
            for (uint32_t i = 0; i < 10; ++i) {
                r = UpdateConstants((uint32_t)xblocks, i, start_block_id, num_total_blocks);
                if (r != VK_SUCCESS)
                    goto COMPUTE_END;
                SetErrorAndOutputBuffer(
                    (i & 1) ? m_err2_buf : m_err1_buf,
                    (i & 1) ? m_err1_buf : m_err2_buf);
                r = RunComputeShader(command_buffer, m_queue,
                                pipeline_mode137_LE10, m_pipeline_layout, m_desc_set,
                                std::max<uint32_t>((uThreadGroupCount + 1) / 2, 1));
                if (r != VK_SUCCESS)
                    goto COMPUTE_END;
            }

            // Encode
            SetErrorAndOutputBuffer(m_err1_buf, m_out_buf);
            r = RunComputeShader(command_buffer, m_queue,
                                pipeline_enc, m_pipeline_layout, m_desc_set,
                                std::max<uint32_t>((uThreadGroupCount + 1) / 2, 1));
            if (r != VK_SUCCESS)
                goto COMPUTE_END;
        }

        start_block_id += n;
        num_blocks -= n;
    }

    // Copy result from GPU
    r = CopyFromOutBuffer(command_buffer, out_pixels);

    COMPUTE_END:
    vkFreeMemory(m_device, src_image_cpu_memory, 0);
    vkDestroyBuffer(m_device, src_image_cpu, 0);
    vkDestroyImageView(m_device, src_image_view, 0);
    vkFreeMemory(m_device, src_image_memory, 0);
    vkDestroyImage(m_device, src_image, 0);
    vkDestroyPipeline(m_device, pipeline_mode456_G10, 0);
    vkDestroyPipeline(m_device, pipeline_mode137_LE10, 0);
    vkDestroyPipeline(m_device, pipeline_mode02, 0);
    vkDestroyPipeline(m_device, pipeline_enc, 0);
    vkFreeCommandBuffers(m_device, m_cmd_pool, 1, &command_buffer);

    return r;
}
