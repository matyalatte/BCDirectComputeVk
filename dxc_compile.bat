@echo off

set TARGET_VK_VER=vulkan1.0

REM Check if dxc exists in PATH
where dxc >nul 2>&1
if errorlevel 1 (
    echo Error: dxc not found in PATH.
    exit /b 1
)

@pushd %~dp0\src
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
dxc -spirv -fvk-use-dx-layout ^
    -HV 2018 -T cs_6_0 -fspv-target-env=%TARGET_VK_VER% ^
    -E %2 -Fh %1_%2.inc -Vn %1_%2 %1.hlsl
dxc -spirv -fvk-use-dx-layout ^
    -HV 2018 -T cs_6_0 -fspv-target-env=%TARGET_VK_VER% ^
    -E %2 -Fo %1_%2.spv %1.hlsl
exit /b
