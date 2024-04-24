#pragma once

#include "Device.hpp"

#include "shaderc/shaderc.h"

#include <vector>
#include <string>

class ShaderModule
{
public:
    ShaderModule(WrpDevice& device, std::string shaderFilename);
    ~ShaderModule();

    size_t getSourceSizeInBytes() { return sourceSizeInBytes; };

    VkShaderModule shaderModule = nullptr;

private:
    std::string readShaderFile(std::string& shaderPath);
    shaderc_shader_kind glslangShaderStageFromFileName(const char* fileName);
    bool endsWith(const char* s, const char* part);
    size_t compileShaderIntoSPIRV(shaderc_shader_kind shaderKind, std::string& shaderSource, std::string& shaderPath);
    VkResult createShaderModule();

    WrpDevice& wrpDevice;
    size_t sourceSizeInBytes = 0;
    std::vector<uint32_t> spirv;
};
