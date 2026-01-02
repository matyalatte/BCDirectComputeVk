cmake_minimum_required(VERSION 3.10)
project(example-app LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USE_VOLK "Use volk (dynamic vulkan loader)" ON)

# Example app
set(EXAMPLE_SOURCES
    example/main.cpp
    src/BCDirectComputeVk.cpp
    src/VulkanDeviceManager.cpp)
if (USE_VOLK)
    list(APPEND EXAMPLE_SOURCES volk/volk.c)
endif()
add_executable(example-app ${EXAMPLE_SOURCES})
target_include_directories(example-app PRIVATE include)
if (USE_VOLK)
    target_include_directories(example-app PRIVATE volk)
    target_compile_definitions(example-app PRIVATE USE_VOLK)
endif()

# Link Vulkan
find_package(Vulkan REQUIRED)
if (USE_VOLK)
    target_include_directories(example-app PRIVATE ${Vulkan_INCLUDE_DIRS})
else()
    if (TARGET VulkanVulkan)
        target_link_libraries(example-app PRIVATE VulkanVulkan)
    else()
        target_include_directories(example-app PRIVATE ${Vulkan_INCLUDE_DIRS})
        target_link_libraries(example-app PRIVATE ${Vulkan_LIBRARIES})
    endif()
endif()
