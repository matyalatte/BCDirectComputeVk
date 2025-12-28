cmake_minimum_required(VERSION 3.10)
project(example-app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(
    example-app
    example/main.cpp
    src/BCDirectComputeVk.cpp
    src/VulkanDeviceManager.cpp
)
target_include_directories(example-app PRIVATE include)

# Link Vulkan
find_package(Vulkan REQUIRED)
if (TARGET VulkanVulkan)
    target_link_libraries(example-app PRIVATE VulkanVulkan)
else()
    target_include_directories(example-app PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(example-app PRIVATE ${Vulkan_LIBRARIES})
endif()
