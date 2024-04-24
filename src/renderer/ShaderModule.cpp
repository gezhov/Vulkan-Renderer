#include "ShaderModule.hpp"
#include "HeaderCore.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <fstream>
#include <iostream>

ShaderModule::ShaderModule(WrpDevice& device, std::string shaderFilename) : wrpDevice(device)
{
    std::string path = SHADERS_DIR + shaderFilename;
    std::string shaderSource = readShaderFile(path);
    if (shaderSource.empty()) {
        throw std::runtime_error("[ShaderModule] Shader source string is empty.");
    }

    if (compileShaderIntoSPIRV(glslangShaderStageFromFileName(path.c_str()), shaderSource, path) < 1) {
        throw std::runtime_error("[ShaderModule] SPIR-V source has 0 size.");
    }
    createShaderModule();
}

ShaderModule::~ShaderModule()
{
    vkDestroyShaderModule(wrpDevice.device(), shaderModule, nullptr);
}

std::string ShaderModule::readShaderFile(std::string& shaderPath)
{
    // ate is setting pointer to the end of file to read file size right from to-go
    std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file: " + shaderPath);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize + 1); // + 1 for terminating null

    // read file to buffer
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    buffer.at(fileSize) = '\0'; // adding null so data was properly read after conversion to std::string

    // remove BOM from the start of the file to support legacy GLSL compilers (especially on Android)
    static constexpr unsigned char BOM[] = {0xEF, 0xBB, 0xBF};

    if (fileSize > 3)
    {
        if (!memcmp(buffer.data(), BOM, 3))
            memset(buffer.data(), ' ', 3);
    }

    // handle #include directives in shader code
    std::string code(buffer.data());
    while (code.find("#include ") != code.npos)
    {
        const auto pos = code.find("#include ");
        const auto p1 = code.find('<', pos);
        const auto p2 = code.find('>', pos);
        if (p1 == code.npos || p2 == code.npos || p2 <= p1)
        {
            throw std::runtime_error("Failed to handle #include directive in " + shaderPath);
        }
        std::string name = code.substr(p1 + 1, p2 - p1 - 1);
        std::string include = readShaderFile(name);
        code.replace(pos, p2 - pos + 1, include.c_str());
    }

    sourceSizeInBytes = code.size();
    return code;
}

shaderc_shader_kind ShaderModule::glslangShaderStageFromFileName(const char* fileName)
{
    if (endsWith(fileName, ".vert"))
        return shaderc_glsl_vertex_shader;

    if (endsWith(fileName, ".frag"))
        return shaderc_glsl_fragment_shader;

    if (endsWith(fileName, ".geom"))
        return shaderc_glsl_geometry_shader;

    if (endsWith(fileName, ".comp"))
        return shaderc_glsl_compute_shader;

    if (endsWith(fileName, ".tesc"))
        return shaderc_glsl_tess_control_shader;

    if (endsWith(fileName, ".tese"))
        return shaderc_glsl_tess_evaluation_shader;

    return shaderc_glsl_vertex_shader;
}

bool ShaderModule::endsWith(const char* s, const char* part)
{
    const size_t sLength = strlen(s);
    const size_t partLength = strlen(part);
    if (sLength < partLength)
        return false;
    return strcmp(s + sLength - partLength, part) == 0;
}

size_t ShaderModule::compileShaderIntoSPIRV(shaderc_shader_kind shaderKind, std::string& shaderSource, std::string& shaderPath)
{
    shaderc_compiler_t compiler = shaderc_compiler_initialize();
    shaderc_compilation_result_t result = shaderc_compile_into_spv(compiler, shaderSource.data(), shaderSource.size(),
        shaderKind, shaderPath.c_str(), "main", nullptr);

    const char* compiler_error_msgs = shaderc_result_get_error_message(result);
    if (compiler_error_msgs)
    {
        std::cerr << compiler_error_msgs << std::endl;
    }

    size_t sizeInBytes = shaderc_result_get_length(result);
    const uint32_t* code = reinterpret_cast<const uint32_t*>(shaderc_result_get_bytes(result));

    spirv.resize(sizeInBytes / sizeof(unsigned int));
    memcpy(spirv.data(), code, sizeInBytes);

    if (result) {
        shaderc_result_release(result);
    }
    shaderc_compiler_release(compiler);
    return sizeInBytes;
}

VkResult ShaderModule::createShaderModule()
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirv.size() * sizeof(uint32_t);
    createInfo.pCode = spirv.data();
    if (vkCreateShaderModule(wrpDevice.device(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create shader module.");
    }
}
