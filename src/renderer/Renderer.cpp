#include "Renderer.hpp"
#include "Utils.hpp"

// std
#include <stdexcept>
#include <cassert>
#include <array>
#include <iostream>

WrpRenderer::WrpRenderer(WrpWindow& window, WrpDevice& device) : wrpWindow{ window }, wrpDevice{ device }
{
    recreateSwapChain();
    createCommandBuffers();
}

WrpRenderer::~WrpRenderer()
{
    freeCommandBuffers();
}

void WrpRenderer::recreateSwapChain()
{
    // glfwWaitEvents() waits for the event which cause resize of window when it has no size.
    // It can be helpful for the window minimizing case.
    auto extent = wrpWindow.getExtent();
    while (extent.width == 0 || extent.height == 0)
    {
        extent = wrpWindow.getExtent();
        glfwWaitEvents();
    }

    if (wrpSwapChain == nullptr) // first time SwapChain creation
    {
        std::cout << "Creating SwapChain for the first time." << std::endl;
        wrpSwapChain = std::make_unique<WrpSwapChain>(wrpDevice, wrpWindow);
    }
    else // SwapCahin recreation
    {
        std::cout << getTimeStampStr() << "Recreating SwapChain." << std::endl;
        
        // oldSwapChain as shared_ptr used to initialize new wrpSwapCahin
        std::shared_ptr<WrpSwapChain> oldSwapChain = std::move(wrpSwapChain);
        wrpSwapChain = std::make_unique<WrpSwapChain>(wrpDevice, wrpWindow, oldSwapChain);

        if (!oldSwapChain->compareSwapChainFormats(*wrpSwapChain.get()))
        {
            throw std::runtime_error("Swap chain image (or depth) format has changed!");
        }
    }
}

void WrpRenderer::createCommandBuffers()
{
    // CommandBuffers count are equal to FrameBuffers count
    commandBuffers.resize(wrpSwapChain->getImageCount());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = wrpDevice.getCommandPool();
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    if (vkAllocateCommandBuffers(wrpDevice.device(), &allocInfo, commandBuffers.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate command buffers!");
    }
}

void WrpRenderer::freeCommandBuffers()
{
    vkFreeCommandBuffers(
        wrpDevice.device(),
        wrpDevice.getCommandPool(),
        static_cast<uint32_t>(commandBuffers.size()),
        commandBuffers.data()
    );

    commandBuffers.clear();
}

VkCommandBuffer WrpRenderer::beginFrame()
{
    assert(!isFrameStarted && "Can't call beginFrame while already in progress.");

    // currentImageIndex gets index of the next FrameBuffer to render to
    VkResult result = wrpSwapChain->acquireNextImage(&currentImageIndex);

    // If result is OUT_OF_DATE, it means surface, that are being rendered to, got changed properties.
    // It can be window size change, for example. In this case SwapChain is recreating with the new extent.
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreateSwapChain();
        return nullptr;
    }

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    // Start frame creating in current command buffer
    isFrameStarted = true;

    auto commandBuffer = getCurrentCommandBuffer();

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;                  // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to begin recording command buffer!");
    }

    return commandBuffer;
}

void WrpRenderer::endFrame()
{
    assert(isFrameStarted && "Can't call endFrame while frame is not in progress.");
    auto commandBuffer = getCurrentCommandBuffer();

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to record command buffer!");
    }

    // Отправка буфера команд для соответствующего кадра в очередь на выполнение девайсом (с учётом синхронизации работы CPU и GPU).
    // Команды выполняются и SwapChain предоставляет полученное из Color attachment'а изображение дисплею в нужное время (в зависимости от выбранного PRESENT MODE).
    auto result = wrpSwapChain->submitCommandBuffers(&commandBuffer, &currentImageIndex);

    /* Проверка изменения размеров окна, сброс флага, пересоздание цепи обмена.
       Результат SUBOPTIMAL_KHR указывает на случай, когда свойства поверхности изменились, но SwapChain
       по прежнему может продолжать вывод изображения. Здесь мы избавляемся от таких ситуаций тоже. */
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || wrpWindow.wasWindowResized())
    {
        wrpWindow.resetWindowsResizedFlag();
        recreateSwapChain();
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to present swap chain image!");
    }

    isFrameStarted = false;
    currentFrameIndex = (currentFrameIndex + 1) % wrpSwapChain->getImageCount(); // выбираем следующий кадр
}

void WrpRenderer::beginSwapChainRenderPass(VkCommandBuffer commandBuffer, ImVec4 clearColors)
{
    assert(isFrameStarted && "Can't call beginSwapChainRenderPass if frame is not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on command buffer from a different frame");

    // Первой записывается команда старта RenderPass, поэтому заполняем информацию о ней
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = wrpSwapChain->getRenderPass();
    renderPassInfo.framebuffer = wrpSwapChain->getFrameBuffer(currentImageIndex);

    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = wrpSwapChain->getSwapChainExtent();

    // clear values задают начальные значения вложений (attachments) у FrameBuffer
    // буферы вложений заполняются этими значениями во время операции очистки перед новым проходом рендера
    std::array<VkClearValue, 2> clearValues{};
    // 0 - для color attachment'а, 1 - для depth stencil attachment'а
    // порядок clearValues должен быть идентичен порядку вложений FrameBuffer'а 
    clearValues[0].color = { clearColors.x, clearColors.y, clearColors.z, 1.0f};
    clearValues[1].depthStencil = { 1.0f, 0 };
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE); // начинаем проход рендера

    /* Ширина и высота изображения берутся из SwapChain, т.к. они могут отличаться от ширины и высоты из окна WrpWindow.
       Например, такой эффект есть при использовании Retina дисплеев (Apple), у которых высокая плотность пикселей.
       Перезаписываясь каждый кадр, динамические Viewport и Scissor всегда получают корректное значение ширины и высоты окна.*/

    // Заполняем конфигурацию для наших динамических объектов пайплайна.
    // Viewport (Область просмотра) - определяет свойства перехода от выходных данных пайплайна к итоговому изображению (Frame Buffer)
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(wrpSwapChain->getSwapChainExtent().width);
    viewport.height = static_cast<float>(wrpSwapChain->getSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    // Scissor (ножницы) - обрезка выводимых пикселей вне заданного Scissor Rectangle
    VkRect2D scissor{ {0, 0}, wrpSwapChain->getSwapChainExtent() };

    // Запись команд
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);  // установка viewport объекта
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);    // установка scissor объекта
}

void WrpRenderer::endSwapChainRenderPass(VkCommandBuffer commandBuffer)
{
    assert(isFrameStarted && "Can't call endSwapChainRenderPass if frame is not in progress.");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't end render pass on command buffer from a different frame.");

    vkCmdEndRenderPass(commandBuffer);
}
