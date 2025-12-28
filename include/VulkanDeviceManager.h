#pragma once

#include <vulkan/vulkan.h>

// VulkanDeviceManager: Class to create VkInstance and VkDevice

/* Example code
void main() {
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
}
 */

constexpr bool VK_MANAGER_ENABLE_DEBUG = true;
constexpr bool VK_MANAGER_DISABLE_DEBUG = false;

class VulkanDeviceManager {
 private:
    // instance
    VkInstance m_instance;
    VkDebugUtilsMessengerEXT m_dbg;
    bool m_enable_debug;

    // activated device info
    VkDevice m_device;
    uint32_t m_family_id;
    uint32_t m_gpu_id;

    // physical devices
    uint32_t m_gpu_count;
    VkPhysicalDevice* m_gpus;

 public:
    VulkanDeviceManager();
    ~VulkanDeviceManager();

    // Create VkInstance and get physical devices
    VkResult CreateInstance(bool enable_debug = false);

    bool HasInstance() { return m_instance != VK_NULL_HANDLE; }
    VkInstance GetInstance() { return m_instance; }

    // Note: The following functions require CreateInstance() to get expected results

    // Functions to get info about physical devices
    bool HasGPU() { return m_gpu_count != 0; }
    uint32_t GetGPUCount() { return m_gpu_count; }
    VkPhysicalDevice GetGPU(uint32_t id) { return m_gpus[id]; }
    VkPhysicalDevice* GetGPUs() { return m_gpus; }
    void GetGPUProperties(uint32_t id, VkPhysicalDeviceProperties* props) {
        vkGetPhysicalDeviceProperties(m_gpus[id], props);
    }

    // Create VkDevice from a GPU.
    // When `gpu_id` is -1, it uses one of GPUs which can run compute shaders.
    VkResult CreateDevice(uint32_t gpu_id = -1);

    bool HasDevice() { return m_device != VK_NULL_HANDLE; }
    VkDevice GetDevice() { return m_device; }

    // Note: The following functions require CreateDevice() to get expected results

    // Functions to get info about created VkDevice
    uint32_t GetUsingFamilyId() { return m_family_id; }
    uint32_t GetUsingGPUId() { return m_gpu_id; }
    VkPhysicalDevice GetUsingGPU() { return m_gpus[m_gpu_id]; }

};
