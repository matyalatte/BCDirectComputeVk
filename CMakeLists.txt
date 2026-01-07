cmake_minimum_required(VERSION 3.10)
project(example-app LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USE_VOLK "Use volk (dynamic vulkan loader)" ON)

# Compile shaders
set(SHADER_SOURCES
    src/BC6HEncode.hlsl
    src/BC7Encode.hlsl)
if (WIN32)
    set(COMPILE_COMMAND "dxc_compile.bat")
else()
    set(COMPILE_COMMAND "dxc_compile.sh")
endif()
add_custom_command(
    OUTPUT "${PROJECT_SOURCE_DIR}/src/compiled_shaders/BC6HEncode_EncodeBlockCS.inc"
    MAIN_DEPENDENCY "${PROJECT_SOURCE_DIR}/${COMPILE_COMMAND}"
    DEPENDS ${SHADER_SOURCES}
    COMMENT "Generating SPIR-V shaders..."
    COMMAND "${PROJECT_SOURCE_DIR}/${COMPILE_COMMAND}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    USES_TERMINAL)

# Example app
set(EXAMPLE_SOURCES
    example/main.cpp
    src/BCDirectComputeVk.cpp
    src/VulkanDeviceManager.cpp
    # Need a shader file here to run the custom command
    src/compiled_shaders/BC6HEncode_EncodeBlockCS.inc)
if (USE_VOLK)
    list(APPEND EXAMPLE_SOURCES volk/volk.c)
endif()
add_executable(example-app ${EXAMPLE_SOURCES})
target_include_directories(example-app PRIVATE include)
target_include_directories(example-app PRIVATE src/compiled_shaders)

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
