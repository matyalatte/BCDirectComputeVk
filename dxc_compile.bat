@echo off

IF defined VULKAN_SDK (
    rem You should use VulkanSDK's dxc
    set "PATH=%VULKAN_SDK%\Bin;%PATH%"
)

set DXC_OPT=-spirv ^
    -fvk-use-dx-layout ^
    -no-warnings ^
    -HV 2018 -T cs_6_0 ^
    -fspv-target-env=vulkan1.0 ^
    -fvk-u-shift 2 0 -fvk-b-shift 3 0

rem Check if dxc exists in PATH
where dxc >nul 2>&1
if errorlevel 1 (
    echo Error: dxc not found in PATH.
    exit /b 1
)

@pushd %~dp0\src
mkdir .\compiled_shaders > NUL 2>&1
call :CompileShader BC7Encode TryMode456CS
call :CompileShader BC7Encode TryMode137CS
call :CompileShader BC7Encode TryMode02CS
call :CompileShader BC7Encode EncodeBlockCS

call :CompileShader BC6HEncode TryModeG10CS
call :CompileShader BC6HEncode TryModeLE10CS
call :CompileShader BC6HEncode EncodeBlockCS
@popd

exit /b 0

:CompileShader
echo Generating %1_%2.inc...
dxc %DXC_OPT% -E %2 -Fh ".\compiled_shaders\%1_%2.inc" -Vn %1_%2 %1.hlsl
rem dxc %DXC_OPT% -E %2 -Fo ".\compiled_shaders\%1_%2.spv" %1.hlsl
exit /b
