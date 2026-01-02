#include "VulkanDeviceManager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

VulkanDeviceManager::VulkanDeviceManager() {
    m_instance = VK_NULL_HANDLE;
    m_dbg = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
    m_family_id = 0;
    m_gpu_id = 0;
    m_gpu_count = 0;
    m_gpus = nullptr;
}

VulkanDeviceManager::~VulkanDeviceManager() {
    vkDestroyDevice(m_device, nullptr);
    m_device = VK_NULL_HANDLE;

    PFN_vkDestroyDebugUtilsMessengerEXT func =
        (PFN_vkDestroyDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func)
        func(m_instance, m_dbg, nullptr);

    vkDestroyInstance(m_instance, nullptr);
    m_instance = VK_NULL_HANDLE;
    m_dbg = VK_NULL_HANDLE;

    free(m_gpus);
    m_gpus = nullptr;
    m_gpu_count = 0;

#ifdef USE_VOLK
    // Unload vulkan library
    volkFinalize();
#endif
}

VkResult CreateVkInstance(VkInstance* instance, bool enable_debug = true) {
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = nullptr;
    app_info.applicationVersion = 0;
    app_info.pEngineName = nullptr;
    app_info.engineVersion = 0;
    app_info.apiVersion = VK_MAKE_VERSION(1, 1, 0); // request Vulkan 1.1 compatibility

    VkValidationFeatureEnableEXT enables[] = {
        VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
        VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
        VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT
    };

    VkValidationFeaturesEXT validation_features = {};
    validation_features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    validation_features.pNext = NULL;
    validation_features.enabledValidationFeatureCount = 3;
    validation_features.pEnabledValidationFeatures = enables;

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    const char* validation_layers[1] = { "VK_LAYER_KHRONOS_validation" };
    const char* extension_names[1] = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };

    if (enable_debug) {
        create_info.pNext = &validation_features;
        create_info.enabledLayerCount = 1;
        create_info.ppEnabledLayerNames = validation_layers;
        create_info.enabledExtensionCount = 1;
        create_info.ppEnabledExtensionNames = extension_names;
    } else {
        create_info.pNext = nullptr;
        create_info.enabledLayerCount = 0;
        create_info.ppEnabledLayerNames = nullptr;
        create_info.enabledExtensionCount = 0;
        create_info.ppEnabledExtensionNames = nullptr;
    }

    *instance = VK_NULL_HANDLE;
    return vkCreateInstance(&create_info, nullptr, instance);
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
        void* user_data) {
    fprintf(stderr, "VULKAN VALIDATION: %s\n", callback_data->pMessage);
    return VK_FALSE; // do not abort Vulkan calls
}

VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance,
        VkDebugUtilsMessengerEXT* messenger) {
    PFN_vkCreateDebugUtilsMessengerEXT func =
        (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func == nullptr)
        return VK_ERROR_EXTENSION_NOT_PRESENT;

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {};
    debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_create_info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_create_info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_create_info.pfnUserCallback = DebugCallback;

    return func(instance, &debug_create_info, nullptr, messenger);
}

bool HasValidationLayerSupport() {
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    VkLayerProperties* layer_props = (VkLayerProperties*)calloc(layer_count, sizeof(VkLayerProperties));
    if (!layer_props)
        return false;
    vkEnumerateInstanceLayerProperties(&layer_count, layer_props);

    bool result = false;
    for (uint32_t i = 0; i < layer_count; i++) {
        if (strcmp(layer_props[i].layerName, "VK_LAYER_KHRONOS_validation") == 0) {
            result = true;
            break;
        }
    }

    free(layer_props);
    return result;
}

VkResult VulkanDeviceManager::CreateInstance(bool enable_debug) {
    m_gpu_count = 0;
    VkResult r = VK_SUCCESS;

#ifdef USE_VOLK
    // Load vulkan library
    r = volkInitialize();
    if (r != VK_SUCCESS)
        return r;
#endif

    m_enable_debug = enable_debug;
    if (m_enable_debug && !HasValidationLayerSupport()) {
        m_enable_debug = false;
        fprintf(stderr, "VULKAN WARNING: Validation layer is disabled since it is not present.\n");
    }

    // Create VkInstance
    r = CreateVkInstance(&m_instance, m_enable_debug);
    if (r != VK_SUCCESS)
        return r;
#if USE_VOLK
    volkLoadInstance(m_instance);
#endif

    if (m_enable_debug) {
        r = CreateDebugUtilsMessengerEXT(m_instance, &m_dbg);
        if (r != VK_SUCCESS)
            return r;
    }

    // Get Physical Devices
    r = vkEnumeratePhysicalDevices(m_instance, &m_gpu_count, nullptr);
    if (r != VK_SUCCESS)
        m_gpu_count = 0;

    if (m_gpu_count == 0)
        return r;

    m_gpus = (VkPhysicalDevice*)calloc(m_gpu_count, sizeof(VkPhysicalDevice));
    if (!m_gpus) {
        m_gpu_count = 0;
        return VK_ERROR_UNKNOWN;  // Failed to allocate m_gpus
    }
    r = vkEnumeratePhysicalDevices(m_instance, &m_gpu_count, m_gpus);
    if (r != VK_SUCCESS) {
        free(m_gpus);
        m_gpus = nullptr;
        m_gpu_count = 0;
    }
    return r;
}

void GetComputeQueueFamily(VkPhysicalDevice device, uint32_t *family_id, uint32_t *queue_count) {
    *family_id = -1;
    *queue_count = 0;

    uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, NULL);
    if (family_count == 0)
        return;

    VkQueueFamilyProperties* family_props = (VkQueueFamilyProperties*)calloc(family_count, sizeof(VkQueueFamilyProperties));
    if (!family_props)
        return;

    vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, family_props);

    VkQueueFamilyProperties* props = family_props;
    for (uint32_t i = 0; i < family_count; ++i) {
        if (props->queueFlags & VK_QUEUE_COMPUTE_BIT) {
            *family_id = i;
            *queue_count = props->queueCount;
            break;
        }
        props++;
    }
    free(family_props);
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

int HasSupportedGpuMemroy(VkPhysicalDevice device) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(device, &mem_props);
    uint32_t visible_id =
        FindMemoryType(&mem_props, -1, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    uint32_t invisible_id =
        FindMemoryType(&mem_props, -1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    return (visible_id != -1) && (invisible_id != -1);
}

VkResult CreateVkDevice(VkPhysicalDevice physical_device, uint32_t family_id, uint32_t queue_count,
                        VkDevice* device) {
    VkDeviceQueueCreateInfo device_queue_create_info = {};
    device_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    device_queue_create_info.queueFamilyIndex = family_id;
    device_queue_create_info.queueCount = 1;
    // device_queue_create_info.queueCount = queue_count;
    float queue_priorities[]{ 1.0f };
    device_queue_create_info.pQueuePriorities = queue_priorities;

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = &device_queue_create_info;

    VkPhysicalDeviceFeatures features = {};
    features.fragmentStoresAndAtomics = VK_TRUE;
    features.vertexPipelineStoresAndAtomics = VK_TRUE;
    device_create_info.pEnabledFeatures = &features;

    *device = VK_NULL_HANDLE;
    return vkCreateDevice(physical_device, &device_create_info, nullptr, device);
}

static bool IsLLVMpipe(VkPhysicalDevice gpu) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(gpu, &props);
    return strstr(props.deviceName, "llvmpipe") != nullptr;
}

bool VulkanDeviceManager::GPUIsLLVMpipe(uint32_t id) {
    return IsLLVMpipe(m_gpus[id]);
}

VkResult VulkanDeviceManager::CreateDevice(uint32_t gpu_id) {
    if (m_device != VK_NULL_HANDLE)
        return VK_ERROR_UNKNOWN;  // VkDevice exists already.
    if (gpu_id != -1 && m_gpu_count <= gpu_id)
        return VK_ERROR_UNKNOWN;  // Out of bounds for m_gpus

    m_device = VK_NULL_HANDLE;
    VkResult r = VK_SUCCESS;

    // Find suitable device
    m_family_id = 0;
    m_gpu_id = gpu_id;
    uint32_t queue_count = 0;

    VkPhysicalDevice gpu = VK_NULL_HANDLE;
    if (m_gpu_id != -1) {
        gpu = m_gpus[m_gpu_id];
        GetComputeQueueFamily(gpu, &m_family_id, &queue_count);
        if (!HasSupportedGpuMemroy(gpu))
            m_family_id = -1;
    } else {
        for (uint32_t i = 0; i < m_gpu_count; i++) {
            gpu = m_gpus[i];
            uint32_t family_id = -1;
            GetComputeQueueFamily(gpu, &family_id, &queue_count);
            if (!HasSupportedGpuMemroy(gpu))
                family_id = -1;
            if (family_id != -1) {
                m_gpu_id = i;
                m_family_id = family_id;
                if (!IsLLVMpipe(gpu))
                    break;
                // Find another GPU if it's LLVMpipe
            }
        }
    }
    if (m_family_id == -1)
        return VK_ERROR_UNKNOWN;  // Supported device not found

    r = CreateVkDevice(gpu, m_family_id, queue_count, &m_device);
    #if USE_VOLK
        if (r == VK_SUCCESS)
            volkLoadDevice(m_device);
    #endif
    return r;
}
