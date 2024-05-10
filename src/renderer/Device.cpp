#include "Device.hpp"

#include <cstring>
#include <iostream>
#include <set>
#include <unordered_set>

// --- DEBUG UTILS/REPORT RELATED FUNCTIONS ---

// Callback function for debug messenger.
// !!! It's useful to set there breakpoint to find out what function call triggered this callback (see callstack).
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT             messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*                                       pUserData
)
{
    std::cerr << "\n";
    switch (messageSeverity) {
        case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            std::cerr << "[VERBOSE] ";
            break;
        case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            std::cerr << "[INFO] ";
            break;
        case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            std::cerr << "[WARNING] ";
            break;
        case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            std::cerr << "[ERROR] ";
            break;
    }

    switch (messageType) {
        case VkDebugUtilsMessageTypeFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
            std::cerr << "[GENERAL] ";
            break;
        case VkDebugUtilsMessageTypeFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
            std::cerr << "[VALIDATION] ";
            break;
        case VkDebugUtilsMessageTypeFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
            std::cerr << "[PERFORMANCE] ";
            break;
    }

    std::cerr << "Validation layer: \"" << pCallbackData->pMessage << "\"\n" << std::endl;
    return VK_FALSE;
    // return VK_TRUE allows to abort the call which triggered this callback. Use VK_FALSE for the casual debugging.
}

// Callback for Debug Report Extension (more elaborate message with information about an object that's causing en error)
static VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallbackFUNC(
    VkDebugReportFlagsEXT      flags,
    VkDebugReportObjectTypeEXT objectType,
    uint64_t                   object,
    size_t                     location,
    int32_t                    messageCode,
    const char*                pLayerPrefix,
    const char*                pMessage,
    void*                      pUserData
)
{
    // https://github.com/zeux/niagara/blob/master/src/device.cpp   [ignoring performance warnings]
    // This silences warnings like "For optimal performance image layout should be VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL instead of GENERAL."
    // We'll assume other performance warnings are also not useful.
    if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
        return VK_FALSE;

    const char* type = (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) ? "ERROR"
        : (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) ? "WARNING"
        : (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) ? "DEBUG"
        : "INFO";

    std::cerr << "[" << type << "] " << "[" << pLayerPrefix << "] Debug report callback: \"" << pMessage << "\"\n" << std::endl;

    return VK_FALSE;
}

// --- CLASS MEMBER FUNCTIONS ---

WrpDevice::WrpDevice(WrpWindow& window) : window{window}
{
    createInstance();      // Vulkan API initialization
    setupDebugMessenger(); // to control output messages from validation layer during debug
    createSurface();       // surface to present output images to (window <-> frame image)
    pickPhysicalDevice();
    createLogicalDevice();
    createCommandPool();
}

WrpDevice::~WrpDevice()
{
    vkDestroyCommandPool(device_, commandPool, nullptr);
    vkDestroyDevice(device_, nullptr);

    if (enableValidationLayers)
    {
        vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        vkDestroyDebugReportCallbackEXT(instance, debugReportCallback, nullptr);
    }

    vkDestroySurfaceKHR(instance, surface_, nullptr);
    vkDestroyInstance(instance, nullptr);
}

void WrpDevice::createInstance()
{
    if (volkInitialize() != VK_SUCCESS)
    {
        throw std::runtime_error("Vulkan loader has not found while trying to initialize volk meta-loader library.");
    }
    if (enableValidationLayers && !checkValidationLayerSupport())
    {
        throw std::runtime_error("Validation layers requested, but not available!");
    }

    VkApplicationInfo appInfo;
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = nullptr;
    appInfo.pApplicationName = "Vulkan";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.flags = 0u;
    createInfo.pApplicationInfo = &appInfo;
    checkRequiredInstanceExtensionsAvailability();
    auto extensions = getRequiredInstanceExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if (enableValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
        // Passing DebugMessangerCreateInfo as pNext allows to enable separate messanger
        // for VkInstance creation and destruction processes. It won't conflict with the main
        // debug messanger and will free with VkInstance destruction.
        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    }
    else
    {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create an instance!");
    }
    volkLoadInstance(instance);
}

void WrpDevice::checkRequiredInstanceExtensionsAvailability()
{
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

    std::cout << "Available Vulkan instance extensions:" << std::endl;
    std::unordered_set<std::string> available;
    for (const auto& extension : extensions)
    {
        std::cout << "\t" << extension.extensionName << ": version " << extension.specVersion << std::endl;
        available.insert(extension.extensionName);
    }

    std::cout << "Required extensions:" << std::endl;
    auto requiredExtensions = getRequiredInstanceExtensions();
    for (const auto& required : requiredExtensions)
    {
        std::cout << "\t" << required << std::endl;
        if (available.find(required) == available.end())
        {
            throw std::runtime_error("Missing required GLFW or Vulkan debug extension.");
        }
    }
}

std::vector<const char*> WrpDevice::getRequiredInstanceExtensions()
{
    // GLFW required extensions
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    // Add extensions for debug messanger (handling validation layers output)
    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);  // extension to set up debug messanger
        extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME); // extension to create debug Report callback
    }

    // Other required extensions from the predefined list
    for (const auto& extension : instanceExtensions)
    {
        extensions.push_back(extension);
    }

    return extensions;
}

void WrpDevice::setupDebugMessenger()
{
    if (!enableValidationLayers) return;

    // Creating Debug Messenger
    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);
    if (vkCreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to set up Debug Messenger!");
    }

    // Creating Debug Report Callback
    VkDebugReportCallbackCreateInfoEXT createInfo2;
    populateDebugReportCallbackInfo(createInfo2);
    if (vkCreateDebugReportCallbackEXT(instance, &createInfo2, nullptr, &debugReportCallback) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to set up Debug Report Callback!");
    }
}

void WrpDevice::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    // catching msg severity are: (INFO and VERBOSE excluded)
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    // catching msg types are:
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback; // callback function pointer
    createInfo.pUserData = nullptr;             // pointer to pass user data to callback function (Optional)
}

void WrpDevice::populateDebugReportCallbackInfo(VkDebugReportCallbackCreateInfoEXT& createInfo)
{
    createInfo =
    {
        .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
        .flags = VK_DEBUG_REPORT_WARNING_BIT_EXT |
            VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
            VK_DEBUG_REPORT_ERROR_BIT_EXT |
            VK_DEBUG_REPORT_DEBUG_BIT_EXT,
        .pfnCallback = debugReportCallbackFUNC,
        .pUserData = nullptr
    };
}

void WrpDevice::pickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }
    std::cout << "Device count: " << deviceCount << std::endl;
    std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());

    for (const auto & physicalDevice : physicalDevices)
    {
        if (isDeviceSuitable(physicalDevice)) {
            physicalDevice_ = physicalDevice;
            break;
        }
    }

    if (physicalDevice_ == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }

    vkGetPhysicalDeviceProperties(physicalDevice_, &properties);
    std::cout << "Picked physical device: " << properties.deviceName << std::endl;
}

// Проверка пригодности переданного физического ус-ва для исп. движком.
bool WrpDevice::isDeviceSuitable(VkPhysicalDevice physicalDevice)
{
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    bool extensionsSupported = checkDeviceExtensionsSupport(physicalDevice);

    bool isSwapChainAdequate = false;
    if (extensionsSupported)
    {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupportDetails(physicalDevice);
        isSwapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    // запрос поддерживаемого функционала данного GPU
    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);

    return indices.isComplete() && extensionsSupported && isSwapChainAdequate
        && supportedFeatures.samplerAnisotropy
        && supportedFeatures.sampleRateShading;
}

// Функция для заполнения структуры, которая хранит индексы нужных нам семейств очередей.
QueueFamilyIndices WrpDevice::findQueueFamilies(VkPhysicalDevice physicalDevice)
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    int i = 0; // индекс рассматриваемого семейства очередей
    for (const auto& queueFamily : queueFamilies)
    {
        // Добавление индекса семейства очередей, которое поддерживает графические команды
        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i;
        }

        // Добавление индекса семейства очередей, которое поддерживает команды отображения
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface_, &presentSupport);
        if (queueFamily.queueCount > 0 && presentSupport)
        {
            indices.presentFamily = i;
        }

        // Выходим из цикла/функции, если структура уже заполнилась
        if (indices.isComplete())
            break;

        i++;
    }

    return indices;
}

void WrpDevice::createLogicalDevice()
{
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<std::optional<uint32_t>> uniqueQueueFamilies = {indices.graphicsFamily, indices.presentFamily};

    // QueueCreateInfo struct for each of the required queue families
    float queuePriority = 1.0f;
    for (std::optional<uint32_t> queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily.value();
        queueCreateInfo.queueCount = 1; // single queue from the family
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.sampleRateShading = VK_TRUE;   // sample shading feature
    deviceFeatures.fillModeNonSolid = VK_TRUE;    // support point and wireframe fill modes

    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    // Device validation layers is deprecated, but they are passed to the info struct to keep consistancy with older Vulkan implementations.
    if (enableValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else
    {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create logical device!");
    }

    // Получение дескрипторов для созданных вместе с девайсом очередей
    vkGetDeviceQueue(device_, indices.graphicsFamily.value(), 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, indices.presentFamily.value(), 0, &presentQueue_);
}

// Создание пула команд, из которого выделяются буферы команд
void WrpDevice::createCommandPool()
{
    QueueFamilyIndices queueFamilyIndices = getQueueFamilies();

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    // Флаги указывают, что буфер команд будет перезаписываться часто и в индивидуальном порядке
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create command pool!");
    }
}

void WrpDevice::createSurface() { window.createWindowSurface(instance, &surface_); }

// Проверка есть ли требуемые слои проверки в списке доступных слоёв экземпляра.
bool WrpDevice::checkValidationLayerSupport()
{
    // Получаем массив свойств для всех доступных слоёв экземпляра
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    // Проверяем наличие требуемых слоёв в полученном массиве всех доступных слоёв экземпляра
    for (const char* layerName : validationLayers)
    {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers)
        {
            if (strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }

        if (!layerFound)
            return false;
    }

    return true;
}

bool WrpDevice::checkDeviceExtensionsSupport(VkPhysicalDevice physicalDevice)
{
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto &extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

SwapChainSupportDetails WrpDevice::querySwapChainSupportDetails(VkPhysicalDevice physicalDevice)
{
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface_, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface_, &formatCount, nullptr);
    if (formatCount != 0)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface_, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface_, &presentModeCount, nullptr);
    if (presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface_, &presentModeCount, details.presentModes.data());
    }
    return details;
}

// Candidate formats are checking in order from most to least desirable
VkFormat WrpDevice::findSupportedFormat(const std::vector<VkFormat>& candidates,
    VkImageTiling tiling, VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties prprts;
        vkGetPhysicalDeviceFormatProperties(physicalDevice_, format, &prprts);

        // Checking if format contains needed features, depends on the type of desired image tiling 
        if (tiling == VK_IMAGE_TILING_LINEAR && (prprts.linearTilingFeatures & features) == features)
        {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (prprts.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }
    throw std::runtime_error("Failed to find supported image format!");
}

VkSampleCountFlagBits WrpDevice::getMaxUsableMSAASampleCount()
{
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice_, &physicalDeviceProperties);

    // looking for the same sample count for both color and depth buffers
    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts &
        physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

    return VK_SAMPLE_COUNT_1_BIT;
}

uint32_t WrpDevice::findMemoryType(uint32_t memoryTypeFilter, VkMemoryPropertyFlags properties)
{
    // getting available memory types and heaps on this device
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        // checking appropriate memory type existence among the device ones
        if ((memoryTypeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}

void WrpDevice::createBuffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer &buffer,
    VkDeviceMemory &deviceMemoryForBuffer)
{
    // buffer creation
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferInfo.flags = 0; // can be used to configure sparse buffer memory

    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }

    // getting memory requirements for this buffer to allocate memory for it further
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device_, buffer, &memRequirements);

    // Filling in memory allocation structure.
    // Using size from requirements and choosing the appropriate type of memory.
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device_, &allocInfo, nullptr, &deviceMemoryForBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate vertex buffer memory!");
    }

    // associating buffer with allocated memory
    vkBindBufferMemory(device_, buffer, deviceMemoryForBuffer, 0);
}

VkCommandBuffer WrpDevice::beginSingleTimeCommands()
{
    /* todo: [possible improvement] use distinct commandPool for
       transient command buffers like this one */
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void WrpDevice::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    // Вместо vkQueueWaitIdle(), тут можно использовать VkFence и несколько vkQueueSubmit'ов,
    // только я не понял чем это будет отличаться от записи нескольких команд на копирование
    // с дальнейшей их отправкой через один vkQueueSubmit().
    vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue_);

    vkFreeCommandBuffers(device_, commandPool, 1, &commandBuffer);
}

/*!
 * @brief Copying srcbuffer to dstBuffer on device through vulkan command.
          This required to HOST_VISIBLE memory to optimal DEVICE_LOCAL memory,
          like its done in stagingBuffer to vertexBuffer transfer.
*/
void WrpDevice::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    // allocating and start commandBuffer to perform this single copying operation
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;  // Optional
    copyRegion.dstOffset = 0;  // Optional
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}

void WrpDevice::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount)
{
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;

    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = layerCount;

    copyRegion.imageOffset = {0, 0, 0};
    copyRegion.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // current image layout
        1,	// there can be multiple copyRegions to create specific pixels layout in the target image 
        &copyRegion
    );

    endSingleTimeCommands(commandBuffer);
}

void WrpDevice::createImageWithInfo(
    const VkImageCreateInfo& imageInfo,
    VkMemoryPropertyFlags properties,
    VkImage& image,
    VkDeviceMemory& imageMemory)
{
    if (vkCreateImage(device_, &imageInfo, nullptr, &image) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device_, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device_, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate image memory!");
    }

    // Binding image and allocated memory
    if (vkBindImageMemory(device_, image, imageMemory, 0) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to bind image memory!");
    }
}

bool WrpDevice::setVkObjectName(void* object, VkObjectType objType, const char* name)
{
    VkDebugUtilsObjectNameInfoEXT nameInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .pNext = nullptr,
        .objectType = objType,
        .objectHandle = (uint64_t)object,
        .pObjectName = name
    };

    return (vkSetDebugUtilsObjectNameEXT(device_, &nameInfo) == VK_SUCCESS);
}
