#include "Pipeline.hpp"

#include "Model.hpp"

// std
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <cassert>

WrpPipeline::WrpPipeline(
    WrpDevice& device,
    const std::string& vertFilepath,
    const std::string& fragFilepath,
    PipelineConfigInfo& configInfo,
    ShaderModule* vertShaderModule,
    ShaderModule* fragShaderModule) : wrpDevice(device)
{
    createGraphicsPipeline(vertFilepath, fragFilepath, configInfo, vertShaderModule, fragShaderModule);
}

WrpPipeline::~WrpPipeline()
{
    vkDestroyPipeline(wrpDevice.device(), graphicsPipeline, nullptr);
}

void WrpPipeline::createGraphicsPipeline(
    const std::string& vertFilepath,
    const std::string& fragFilepath,
    PipelineConfigInfo& configInfo,
    ShaderModule* vertShaderModule,
    ShaderModule* fragShaderModule)
{
    // Явные проверки на наличие объектов pipelineLayout и renderPass в структуре конфигурационной информации (configInfo)
    assert(configInfo.pipelineLayout != VK_NULL_HANDLE && "Cannot create graphics pipeline: no pipelineLayout provided in configInfo");
    assert(configInfo.renderPass != VK_NULL_HANDLE && "Cannot create graphics pipeline: no renderPass provided in configInfo");
    std::cout << "Graphics Pipeline is creating..." << std::endl;

    // создание шейдерных модулей, если они не были переданы
    if (!vertShaderModule) {
        vertShaderModule = new ShaderModule(wrpDevice, vertFilepath);
        std::cout << "Vertex Shader Code Size: " << vertShaderModule->getSourceSizeInBytes() << std::endl;
    }
    if (!fragShaderModule) {
        fragShaderModule = new ShaderModule(wrpDevice, fragFilepath);
        std::cout << "Fragment Shader Code Size: " << fragShaderModule->getSourceSizeInBytes() << std::endl;
    }

    VkPipelineShaderStageCreateInfo shaderStages[2];
    // Информация для шейдера вершин
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;  // тип шейдерного этапа
    shaderStages[0].module = vertShaderModule->shaderModule;			 // передаём в структуру созданный ранее шейдерный модуль
    shaderStages[0].pName = "main";						 // название входной функции шейдерной программы
    shaderStages[0].flags = 0;
    shaderStages[0].pNext = nullptr;
    shaderStages[0].pSpecializationInfo = nullptr;		 // VkSpecializationInfo - механизм конфигурирования шейдера, путём изменения его констант

    // Информация для шейдера фрагментов
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragShaderModule->shaderModule;
    shaderStages[1].pName = "main";
    shaderStages[1].flags = 0;
    shaderStages[1].pNext = nullptr;
    shaderStages[1].pSpecializationInfo = nullptr;

    // Получение структур с описанием привязок и атрибутов Vertex Buffer'а(ов). Они используются дальше при описании VertexInput этапа.
    auto& bindingDescriptions = configInfo.bindingDescriptions;
    auto& attributeDescriptions = configInfo.attributeDescriptions;

    // Информация для VertexInput этапа. Этот этап описывает как мы интерпретируем входные данные вершин из Vertex Buffer.
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
    vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();

    // Информация для мультисэмплинга
    // todo: не логично, конечно, изменять здесь конфиг, который назван дефолтным. можно обдумать этот момент - как вносить правки в параметры пайплайна
    configInfo.multisampleInfo.rasterizationSamples = wrpDevice.getMaxUsableMSAASampleCount();

    // Информация для самого Графического Пайплайна
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;							// количество программируемых этапов
    pipelineInfo.pStages = shaderStages;					// указатель на CreateInfo-структуры для программируемых этапов
    // далее идёт прикрепление всех CreateInfo-структур для фиксированных этапов пайплайна
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &configInfo.inputAssemblyInfo;  // некоторые фиксированные этапы описаны в PiplineConfigInfo и берутся оттуда
    pipelineInfo.pViewportState = &configInfo.viewportInfo;
    pipelineInfo.pRasterizationState = &configInfo.rasterizationInfo;
    pipelineInfo.pMultisampleState = &configInfo.multisampleInfo;
    pipelineInfo.pColorBlendState = &configInfo.colorBlendInfo;
    pipelineInfo.pDepthStencilState = &configInfo.depthStencilInfo;
    // Опциональный этап, который позволяет изменять некоторый функционал пайплайна в динамике (без необходимости пересоздания пайплайна),
    // например, область просмотра (Viewport) или толщину линий для отрисовки.
    pipelineInfo.pDynamicState = &configInfo.dynamicStateInfo;
    
    // pipelineLayout создаётся в RenderSystem, а renderPass в SwapChain'е
    pipelineInfo.layout = configInfo.pipelineLayout;
    pipelineInfo.renderPass = configInfo.renderPass;
    pipelineInfo.subpass = configInfo.subpass;			// пайплайн будет применятся в указанном подпроходе рендера

    // Эти два параметра будут применятся для оптимизации производительности,
    // путём создания нового пайплайна на основе другого пайплайна
    pipelineInfo.basePipelineIndex = -1;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.flags = 0; // два поля выше исп., если задать флаг VK_PIPELINE_CREATE_DERIVATIVE_BIT

    if (vkCreateGraphicsPipelines(wrpDevice.device(),
        VK_NULL_HANDLE,
        1, &pipelineInfo,
        nullptr,
        &graphicsPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    // Шейдерные модули можно освободить сразу после создания пайплайна, т.к. шейдеры уже скомпилированы
    delete vertShaderModule;
    delete fragShaderModule;
}

void WrpPipeline::bind(VkCommandBuffer commandBuffer)
{
    // BIND_POINT показывает тип привязанного к буферу команд пайплайна. Можно привязать Graphics, Compute и RayTracing пайплайны
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
}

void WrpPipeline::defaultPipelineConfigInfo(PipelineConfigInfo& configInfo)
{
    // Информация для этапа входной сборки
    configInfo.inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    configInfo.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    configInfo.inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

    // Конфигурация динамического состояния (изменяемого этапа пайплайна).
    // Включение динамического состояния требует передачи конфигурации для него уже в процессе отрисовки через соответствующие команды (сейчас это делается в рендерере при старте рендер пасса).
    configInfo.dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };  // флаги включения динамич. объектов - частей этого динамич. этапа
    configInfo.dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    configInfo.dynamicStateInfo.pDynamicStates = configInfo.dynamicStateEnables.data();
    configInfo.dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(configInfo.dynamicStateEnables.size());
    configInfo.dynamicStateInfo.flags = 0;

    // Объедиение объектов Viewport и Scissor в информацию об области просмотра viewportInfo.
    // С активацией специальных возможностей GPU можно использовать несколько viewport и scissor объектов.
    configInfo.viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    configInfo.viewportInfo.viewportCount = 1;
    configInfo.viewportInfo.pViewports = nullptr; // nullptr, т.к. исп. dynamic state
    configInfo.viewportInfo.scissorCount = 1;
    configInfo.viewportInfo.pScissors = nullptr;

    // Информация для этапа растеризации
    configInfo.rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    configInfo.rasterizationInfo.depthClampEnable = VK_FALSE;		  // зажатие z-компоненты gl_Position между 0 и 1 (VK_FALSE = отключено)
    configInfo.rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;  // сбрасывать примитивы перед этапом растеризации (VK_TRUE отключит любой вывод в буфер кадра)
    configInfo.rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;  // режим генерации фрагментов для полигонов (вершины, края или весь треугольник)
    configInfo.rasterizationInfo.lineWidth = 1.0f;					  // ширина линии
    configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;	  // режим отбраковки треугольников (можно отбрасывать треугольники, которые отображаются задней стороной и т.п.)
    configInfo.rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // определение winding order (порядка намотки) вершин треугольника, который будет использоваться для определения его лицевой стороны
    configInfo.rasterizationInfo.depthBiasEnable = VK_FALSE;		  // смещение значения глубины (VK_FALSE = отключено)
    configInfo.rasterizationInfo.depthBiasConstantFactor = 0.0f;  // Optional. Множитель смещения глубины
    configInfo.rasterizationInfo.depthBiasClamp = 0.0f;           // Optional. Тиски для зажима смещения глубины
    configInfo.rasterizationInfo.depthBiasSlopeFactor = 0.0f;     // Optional. Множитель смещения глубины для фрагмента в наклоне

    // Информация для этапа мультисемплирования (множественная выборка цвета для фрагментов, чтобы устранить зубчатость краёв)
    configInfo.multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    configInfo.multisampleInfo.sampleShadingEnable = VK_TRUE;
    //configInfo.multisampleInfo.rasterizationSamples; этот параметр дописывается при создании пайплайна (из статик контекста не могу достать девайс)
    configInfo.multisampleInfo.minSampleShading = .2f;            // min fraction for sample shading; closer to one is smoother
    configInfo.multisampleInfo.pSampleMask = nullptr;             // Optional
    configInfo.multisampleInfo.alphaToCoverageEnable = VK_FALSE;  // Optional
    configInfo.multisampleInfo.alphaToOneEnable = VK_FALSE;       // Optional

    // Настройка ColorBlend состояния для этапа смешивания цветов Color Attachment'а в подпроходе рендера (subpass)
    configInfo.colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    configInfo.colorBlendAttachment.blendEnable = VK_TRUE;  // Включена
    // finalColor.rgb = newAlpha.a * newColor.rgb + (1 - newAlpha.a) * oldColor.rgb;
    // finalColor.a = newAlpha.a;
    configInfo.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;   // Optional
    configInfo.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;  // Optional
    configInfo.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;              // Optional
    configInfo.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;   // Optional
    configInfo.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;  // Optional
    configInfo.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;              // Optional

    // Информация для этапа смешивания цветов (например, смешивание цвета фрагментов двух треугольников при их наложении друг на друга)
    configInfo.colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    // logicOpEnable = VK_TRUE включит второй способ смешивания цветов (с помощью логич. операции) и отключит смешивание
    // первым способом (уравнение, описанное в переданной привязке ColorBlendAttachmentState).
    configInfo.colorBlendInfo.logicOpEnable = VK_FALSE;
    configInfo.colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;  // Optional
    configInfo.colorBlendInfo.attachmentCount = 1;
    configInfo.colorBlendInfo.pAttachments = &configInfo.colorBlendAttachment;
    configInfo.colorBlendInfo.blendConstants[0] = 1.0f;  // Optional
    configInfo.colorBlendInfo.blendConstants[1] = 1.0f;  // Optional
    configInfo.colorBlendInfo.blendConstants[2] = 1.0f;  // Optional
    configInfo.colorBlendInfo.blendConstants[3] = 1.0f;  // Optional

    // Информация для этапа Depth Stencil (проверка глубины и проверка трафарета).
    // На этапе проверки глубины отбрасываются те фрагменты изображения, которые были перекрыты другим, более близким фрагментом.
    // Глубина самого близкого фрагмента записывается в буфер глубины для дальнейших сравнений.
    // На этапе проверки трафарета фрагменты изображения затеняются в соответствии с полученным для них значением из Depth Buffer.
    // Проверка трафарета отключена в данный момент.
    configInfo.depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    configInfo.depthStencilInfo.depthTestEnable = VK_TRUE;
    configInfo.depthStencilInfo.depthWriteEnable = VK_TRUE;
    configInfo.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;	// Фрагмент ближе, если его значение глубины меньше
    configInfo.depthStencilInfo.depthBoundsTestEnable = VK_FALSE;		// Дополнительная проверка глубины на попадание в диапазон (отключено)
    configInfo.depthStencilInfo.minDepthBounds = 0.0f;  // Optional
    configInfo.depthStencilInfo.maxDepthBounds = 1.0f;  // Optional
    configInfo.depthStencilInfo.stencilTestEnable = VK_FALSE;			// Проверка трафарета отключена
    configInfo.depthStencilInfo.front = {};  // Optional
    configInfo.depthStencilInfo.back = {};   // Optional

    // дефолт значения для массивов привязок и атрибутов буфера вершин
    configInfo.bindingDescriptions = WrpModel::Vertex::getBindingDescriptions();
    configInfo.attributeDescriptions = WrpModel::Vertex::getAttributeDescriptions();
}

// Данная функция включает и настраивает этап смешивания цветов в пайплайне (опциональна)
void WrpPipeline::enableAlphaBlending(PipelineConfigInfo & configInfo)
{
    // Настройка привязки этапа смешивания цветов к фреймбуферу
    configInfo.colorBlendAttachment.blendEnable = VK_TRUE;  // включение смшивания цветов имеет свою цену для производительности
    // ColorWriteMask указывает, что мы хотим писать RGB и A компоненты
    configInfo.colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    // Шесть полей ниже соответствуют переменным в уравнении смешивания цветов и определяют как
    // в действительности нужно комбинировать RGB и A компоненты:
    // Уравнение: color.rgb = (srcColorBlendFactor * src.rgb) <colorBlendOp> (dstColorBlendFactor * dst.rgb) ===>
    // ===> color.rgb = (src.a * src.rgb) + ((1 - src.a) * dst.rgb)
    configInfo.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    configInfo.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    configInfo.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    configInfo.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;   // не исп., т.к. у нас нет альфа смешивания
    configInfo.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;  // не исп.
    configInfo.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;              // не исп.
}
