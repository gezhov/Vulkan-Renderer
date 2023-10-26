#include "SwapChain.hpp"

// std
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <algorithm>
#include <set>
#include <stdexcept>

WrpSwapChain::WrpSwapChain(WrpDevice& device, WrpWindow& window)
    : wrpDevice{device}, wrpWindow{window}
{
    init();
}

WrpSwapChain::WrpSwapChain(WrpDevice& device, WrpWindow& window, std::shared_ptr<WrpSwapChain> previous)
    : wrpDevice{device}, wrpWindow{window}, oldSwapChain{previous}
{
    init();

    vkDeviceWaitIdle(wrpDevice.device()); // ожидание, пока старый SwapChain не перестанет использоваться девайсом
    oldSwapChain = nullptr; // избавляемся от старого свапчейна
}

void WrpSwapChain::init()
{
    msaaSampleCount = wrpDevice.getMaxUsableMSAASampleCount(); // needed in multiple structs
    createSwapChain();       // создание SwapChain объекта
    createImageViews();      // создание VkImageView представлений для изображений SwapChain'а
    createColorResources();  // создание изображений цвета для реализации мультисэмплинга
    createDepthResources();  // создание изображений для Depth Buffer вложения
    createRenderPass();      // subpass с его привязками и дальнейшее создание RenderPassa'а
    createFramebuffers();
    createSyncObjects();
}

WrpSwapChain::~WrpSwapChain()
{
    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(wrpDevice.device(), imageView, nullptr);
    }
    swapChainImageViews.clear();

    if (swapChain != nullptr) {
        vkDestroySwapchainKHR(wrpDevice.device(), swapChain, nullptr);
        swapChain = nullptr;
    }

    vkDestroyImageView(wrpDevice.device(), colorImageView, nullptr);
    vkDestroyImage(wrpDevice.device(), colorImage, nullptr);
    vkFreeMemory(wrpDevice.device(), colorImageMemory, nullptr);

    for (int i = 0; i < depthImages.size(); i++) {
        vkDestroyImageView(wrpDevice.device(), depthImageViews[i], nullptr);
        vkDestroyImage(wrpDevice.device(), depthImages[i], nullptr);
        vkFreeMemory(wrpDevice.device(), depthImageMemories[i], nullptr);
    }

    for (auto framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(wrpDevice.device(), framebuffer, nullptr);
    }

    vkDestroyRenderPass(wrpDevice.device(), renderPass, nullptr);

    // cleanup synchronization objects
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(wrpDevice.device(), renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(wrpDevice.device(), imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(wrpDevice.device(), inFlightFences[i], nullptr);
    }
}

VkResult WrpSwapChain::acquireNextImage(uint32_t* imageIndex)
{
    // Wait for fence signal when N'th frames command buffer has executed
    vkWaitForFences(
        wrpDevice.device(),
        1,
        &inFlightFences[currentFrame],
        VK_TRUE,
        std::numeric_limits<uint64_t>::max()); // big timeout number to wait till end

    VkResult result = vkAcquireNextImageKHR(
        wrpDevice.device(),
        swapChain,
        std::numeric_limits<uint64_t>::max(),
        imageAvailableSemaphores[currentFrame],  // must be a not signaled semaphore
        VK_NULL_HANDLE,
        imageIndex);

    return result;
}

VkResult WrpSwapChain::submitCommandBuffers(const VkCommandBuffer* buffers, uint32_t* imageIndex)
{
    if (imagesInFlight[*imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(wrpDevice.device(), 1, &imagesInFlight[*imageIndex], VK_TRUE, UINT64_MAX);
    }
    imagesInFlight[*imageIndex] = inFlightFences[currentFrame];

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    // Wait on writing colors to the image until it become available.
    // Each waiting stage corresponds to the semaphore with the same index.
    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = buffers;
    
    // specifying which semaphores to signal once command buffer(s) has finished execution
    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    // Resetting N'th frame fence for further successful waiting
    vkResetFences(wrpDevice.device(), 1, &inFlightFences[currentFrame]);

    // Command buffer is submitting to graphics queue.
    // The passed fence will be signaled once command buffer(s) has finished execution.
    if (vkQueueSubmit(wrpDevice.graphicsQueue(), 1, &submitInfo,
        inFlightFences[currentFrame]) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    // wait until renderFinishedSemaphore is signaled before presenting image
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapChain}; // swapchain and image to present
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = imageIndex;
    presentInfo.pResults = nullptr; // optional

    // Image is queueing for presentation
    auto result = vkQueuePresentKHR(wrpDevice.presentQueue(), &presentInfo);

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

    return result;
}

void WrpSwapChain::createSwapChain()
{
    SwapChainSupportDetails swapChainSupport = wrpDevice.getSwapChainSupport();

    VkSurfaceFormatKHR surfaceFormat = chooseSwapChainSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapChainPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapChainExtent(swapChainSupport.capabilities);

    // Минимальное кол-во изображений в цепи: min + 1 = triple buffering
    // Если maxImageCount == 0, то это значит, что кол-во изображений в цепи неограничено
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount)
    {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }
    std::cout << "Number of swap chain images: " << imageCount << std::endl;

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = wrpDevice.surface();
    // детали по хранящимся в SwapChain изображениям
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    // изображения цепи обмена исп. как color attachment subpass'а, значит рендер будет вестись прямо в них
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // Указание режима использования изображений цепи обмена очередями
    QueueFamilyIndices indices = wrpDevice.getQueueFamilies();
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};
    if (indices.graphicsFamily != indices.presentFamily) // параллельный режим, если очереди из разных семейств
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else // исключительный режим, если очереди из одного семейства
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0; // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform; // преобразование для изображения по умолчанию
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // использование alpha канала для смешивания пикселей с другими окнами

    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE; // отсечение перекрытых пикселей окна из swapchain изображений

    createInfo.oldSwapchain = oldSwapChain == nullptr ? VK_NULL_HANDLE : oldSwapChain->swapChain;

    if (vkCreateSwapchainKHR(wrpDevice.device(), &createInfo, nullptr, &swapChain) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create swap chain!");
    }

    // В структуре создания цепи обмена мы указали минимальное кол-во изображений в цепи, но
    // реализация Vulkan может создать число изображений больше, чем указано в структуре,
    // поэтому первым вызовом vkGetSwapchainImagesKHR() получаем реальное кол-во изображений.
    vkGetSwapchainImagesKHR(wrpDevice.device(), swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(wrpDevice.device(), swapChain, &imageCount, swapChainImages.data());

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

void WrpSwapChain::createImageViews()
{
    swapChainImageViews.resize(swapChainImages.size()); // ресайз вектора под кол-во изображений
    for (size_t i = 0; i < swapChainImages.size(); i++)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapChainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapChainImageFormat;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY; // маппинг цветовых каналов
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY; // к определённым значениям
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY; // IDENTITY - дефолтный маппинг
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(wrpDevice.device(), &viewInfo, nullptr, &swapChainImageViews[i]) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create image views in swapchain!");
        }
    }
}

void WrpSwapChain::createRenderPass()
{
    // Описание colorBuffer привязки для подпрохода (используется для мультисемплинга фрагментов)
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = getSwapChainImageFormat();     // формат привязки цвета должен совпадать с форматом изображения из цепи обмена
    colorAttachment.samples = msaaSampleCount;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;   // colorBuffer очищается перед отрисовкой нового кадра
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // отрисованное содержимое сохраняется в памяти
    // stencilBuffer не используется, поэтому операции загрузки и хранения не имеют значения
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // Изначальная схема VkImage ресурса для color буфера не имеет значения 
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // схема вложения цвета

    // Ссылка на привязку под индексом 0 (ColorBuffer)
    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0; // по указанному индексу на привязку будет ссылаться шейдер фрагментов: layout(location = 0) out vec4 outColor
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Схема VkImage для исп. в подпроходе рендера

    // Описание depthBuffer привязки для подпрохода
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = swapChainDepthFormat;
    depthAttachment.samples = msaaSampleCount;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Ссылка на привязку под индексом 1 (DepthBuffer)
    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Описание colorBuffer разрешающей привязки.
    // Она получит сэмплированное изображение для дальнейшего показа.
    VkAttachmentDescription colorAttachmentResolve{};
    colorAttachmentResolve.format = swapChainImageFormat;
    colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Ссылка на привязку с индексом 2 (ColorResolveBuffer)
    VkAttachmentReference colorAttachmentResolveRef{};
    colorAttachmentResolveRef.attachment = 2;
    colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Описание подпрохода с передачей ссылок на вложения буфера кадра, которые он будет использовать
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // сабпасс для графич. пайплайна
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    subpass.pResolveAttachments = &colorAttachmentResolveRef;   // this is enough for subpass to define a resolve operation

    // Subpass dependencies are specifying transition properties between subpasses.
    // Even if we have only one subpass we need to describe dependency from implicit external subpass.
    // This dependency will prevent the image transition between subpasses from happening until we actually want to write to it.
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // implicit subpass which denote subpass from a previous/next renderpass
    dependency.dstSubpass = 0;                   // 0 means our subpass
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;   // stages that needs to complete on the srcSubpass before moving to dstSubpass 
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;   // stages to wait on the dstSubpass untill srcSubpass is busy with srcStageMask
    dependency.srcAccessMask = 0;                     // bitmask for memory access types used by srcSubpass 
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; // memory access types to use in dstSubpass 

    // указанные в reference'ах индексы вложений относятся именно к этому массиву
    std::array<VkAttachmentDescription, 3> attachments = {colorAttachment, depthAttachment, colorAttachmentResolve};

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(wrpDevice.device(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create render pass!");
    }
}

void WrpSwapChain::createFramebuffers()
{
    swapChainFramebuffers.resize(imageCount());

    // Создание буфера кадра для каждого из изображений в цепи обмена
    for (size_t i = 0; i < imageCount(); i++)
    {
        // swapChainImageViews для colorAttachment'ов, depthImageViews для depthAttachment'ов
        // данные ImageViews будут связываться с соответствующими VkAttachmentReference'ами сабпасса
        std::array<VkImageView, 3> attachments = {colorImageView, depthImageViews[i], swapChainImageViews[i]};

        VkExtent2D swapChainExtent = getSwapChainExtent();
        // Указывая RenderPass для фреймбуфера, мы говорим, что данный буфер кадра должен быть с ним совместим,
        // т.е. иметь такое же кол-во вложений того же типа. В данном случае это ColorBuffer и DepthBuffer аттачменты,
        // которые являются VkImageView объектами и именно они передаются в буфер кадра как pAttachments.
        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data(); 
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(wrpDevice.device(), &framebufferInfo, nullptr, &swapChainFramebuffers[i])
            != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create framebuffer!");
        }
    }
}

// Создание изображений цвета для их использования в ходе мультисемплирования
void WrpSwapChain::createColorResources()
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = swapChainExtent.width;
    imageInfo.extent.height = swapChainExtent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = swapChainImageFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageInfo.samples = msaaSampleCount;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    wrpDevice.createImageWithInfo(
        imageInfo,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        colorImage,
        colorImageMemory
    );

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = colorImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = swapChainImageFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(wrpDevice.device(), &viewInfo, nullptr, &colorImageView) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create color image view!");
    }
}

// Создание изображений глубины для их использования в качестве вложения глубины (Depth Attachment)
void WrpSwapChain::createDepthResources()
{
    swapChainDepthFormat = findDepthFormat();

    depthImages.resize(imageCount());
    depthImageMemories.resize(imageCount());
    depthImageViews.resize(imageCount());

    for (int i = 0; i < depthImages.size(); i++)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = swapChainExtent.width;
        imageInfo.extent.height = swapChainExtent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = swapChainDepthFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = msaaSampleCount;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.flags = 0;

        wrpDevice.createImageWithInfo(
            imageInfo,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            depthImages[i],
            depthImageMemories[i]
        );

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = depthImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapChainDepthFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(wrpDevice.device(), &viewInfo, nullptr, &depthImageViews[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create depth image view!");
        }
    }
}

void WrpSwapChain::createSyncObjects()
{
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    imagesInFlight.resize(imageCount(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (vkCreateSemaphore(wrpDevice.device(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(wrpDevice.device(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(wrpDevice.device(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create synchronization objects for a frame!");
        }
    }
}

// Функция выбора формата поверхности для цепи обмена
VkSurfaceFormatKHR WrpSwapChain::chooseSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    for (const auto& availableFormat : availableFormats)
    {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return availableFormat;
    }

    return availableFormats[0]; // выбираем первый формат, если не был найден нужный
}

// Функция выбора режима показа кадров
VkPresentModeKHR WrpSwapChain::chooseSwapChainPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
    // Если поддерживается Mailbox, то возвращается именно он.
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            std::cout << "Present mode: Mailbox" << std::endl;
            return availablePresentMode;
        }
    }

    // Резервный вариант в виде режима показа Immediate. В этом режиме
    // вертикальная синхронизация отсутствует.
    /*for (const auto &availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            std::cout << "Present mode: Immediate" << std::endl;
            return availablePresentMode;
        }
    }*/

    // В случае отсутствия поддержки выбранного выше режима показа, включается
    // режим FIFO (стандартный V-Sync).
    std::cout << "Present mode: FIFO" << std::endl;
    return VK_PRESENT_MODE_FIFO_KHR;
}

// Выбор размера (разрешения) для кадров в цепи обмена
VkExtent2D WrpSwapChain::chooseSwapChainExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    // Если текущая ширина поверхности отлична от uint32_t max, то это значит, что в качестве
    // размерностей для кадров используются значения совпадающие с координатами окна.
    // uint32_t max - это спец. значение для индикации несоответствия размеров окна и размеров кадра в SwapChain.
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    // В противном случае, мы "зажимаем" в допустимых для поверхности пределах текущий реальный размер
    // окна (его буфера кадра) в пикселях и возвращаем его в качестве размера кадра для SwapChain.
    else
    {
        int width, height;
        glfwGetFramebufferSize(wrpWindow.getGLFWwindow(), &width, &height);

        VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

// Поиск поддерживаемого девайсом формата глубины из переданного списка
VkFormat WrpSwapChain::findDepthFormat()
{
    // ищутся форматы с depth компонентом и с поддержкой depth stencil attachment использования
    return wrpDevice.findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}
