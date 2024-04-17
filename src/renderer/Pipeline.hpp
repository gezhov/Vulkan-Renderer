#pragma once

#include "Device.hpp"

// std
#include <string>
#include <vector>

// Структура для хранения данных для конфигурирования пайплайна. Структура доступна
// слою приложения, чтобы в его коде можно было конфигурировать графический пайплайн полностью.
struct PipelineConfigInfo
{
    PipelineConfigInfo() = default;

    // "resource acquisition is initialization"
    PipelineConfigInfo(const PipelineConfigInfo&) = delete;
    PipelineConfigInfo& operator=(const PipelineConfigInfo&) = delete;

    std::vector<VkVertexInputBindingDescription> bindingDescriptions{};
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

    VkPipelineViewportStateCreateInfo viewportInfo;			   // информация об области просмотра
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;  // информация по этапу входной сборки "Input Assembly"
    VkPipelineRasterizationStateCreateInfo rasterizationInfo;  // информация об этапе растеризации
    VkPipelineMultisampleStateCreateInfo multisampleInfo;	   // информация о этапе мультисемплирования
    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    VkPipelineColorBlendStateCreateInfo colorBlendInfo;
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
    std::vector<VkDynamicState> dynamicStateEnables;
    VkPipelineDynamicStateCreateInfo dynamicStateInfo;         // изменяемые св-ва конвейера
    VkPipelineLayout pipelineLayout = nullptr;
    VkRenderPass renderPass = nullptr;				// определяет структуру подпроходов рендера (их вложения (attachments))
    uint32_t subpass = 0;
};

class WrpPipeline
{
public:
    WrpPipeline(
        WrpDevice& device,
        const std::string& vertFilepath,
        const std::string& fragFilepath,
        PipelineConfigInfo& configInfo,
        VkShaderModule vertShaderModule = nullptr,
        VkShaderModule fragShaderModule = nullptr);

    ~WrpPipeline();

    // принцип "resource acquisition is initialization"
    WrpPipeline(const WrpPipeline&) = delete;
    WrpPipeline& operator=(const WrpPipeline&) = delete;

    void bind(VkCommandBuffer commandBuffer);

    static void defaultPipelineConfigInfo(PipelineConfigInfo& configInfo);
    static void enableAlphaBlending(PipelineConfigInfo& configInfo);

    static std::vector<char> readShaderFile(const std::string& filepath);
    void createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule);

private:
    void createGraphicsPipeline(
        const std::string& vertFilepath,
        const std::string& fragFilepath,
        PipelineConfigInfo& configInfo,
        VkShaderModule vertShaderModule = nullptr,
        VkShaderModule fragShaderModule = nullptr
    );

    WrpDevice& wrpDevice;				// девайс
    VkPipeline graphicsPipeline;		// Vulkan Graphics Pipeline (это указатель, сам тип определён через typedef)
};
