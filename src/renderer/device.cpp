#include "device.hpp"

// std headers
#include <cstring>
#include <iostream>
#include <set>
#include <unordered_set>

ENGINE_BEGIN

// Отладочная callback-функция, которая исп. отладочным мессенджером.
// Её прототип соответствует требуемому типу PFN_vkDebugUtilsMessengerCallbackEXT.
/* Чтобы увидеть вызов, который стриггерил callback, можно добавить точку останова на
   эту функцию и посмотреть на стек вызовов. */
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
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

    std::cerr << "Validation layer says: \"" << pCallbackData->pMessage << "\"" << std::endl;
    return VK_FALSE;
    /* Если функция будет возвращать VK_TRUE, то вызов Vulkan'а, который инициировал данное сообщение проверки будет прерван.
       Такое прерывание через слой проверки не используется в полноценной отладке, поэтому тут всегда VK_FALSE. */
}

// Функция, которая находит указатель на функцию vkCreateDebugUtilsMessengerEXT(),
// а затем вызывает её создавая дескриптор для отлад. мессенджера VkDebugUtilsMessengerEXT
VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    // ищем указатель на функцию создания отладочного мессенджера в расширении экземпляра
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance,
        "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        // создаём отладочный мессенджер, вызвав найденную функцию
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

// Функция для уничтожения дескриптора отладочного мессенджера при помощи функции
// vkDestroyDebugUtilsMessengerEXT(), указатель на которую сначала ищется через экземпляр.
void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance,
        "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

// CLASS MEMBER FUNCTIONS

WrpDevice::WrpDevice(WrpWindow& window) : window{window}
{
    createInstance();      // инициализация Vulkan API
    setupDebugMessenger(); // настройка отладочного мессенджера для контроля вывода сообщений от слоя проверки в ходе отладки
    createSurface();       // создание surface объекта для связи окна от GLFW и выводимого изображения от Vulkan
    pickPhysicalDevice();  // выбор физического девайса (GPU)
    createLogicalDevice(); // создание логического девайса (выбор технических особенностей GPU для работы с ними)
    createCommandPool();   // создание пула команд
}

WrpDevice::~WrpDevice()
{
    vkDestroyCommandPool(device_, commandPool, nullptr);
    vkDestroyDevice(device_, nullptr);

    if (enableValidationLayers)
    {
        DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }

    vkDestroySurfaceKHR(instance, surface_, nullptr);
    vkDestroyInstance(instance, nullptr);
}

void WrpDevice::createInstance()
{
    if (enableValidationLayers && !checkValidationLayerSupport())
    {
        throw std::runtime_error("Validation layers requested, but not available!");
    }

    VkApplicationInfo appInfo = {}; // Необязательная информация о приложении
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "VgetX-Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo = {}; // Обязательная информация для создания экземпляра Vulkan
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    // Проверяем наличие требуемых для движка расширений и помещаем их названия в createInfo
    hasEngineRequiredInstanceExtensions();
    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if (enableValidationLayers) // включение слоёв проверок
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        // Передача структуры для создания отладочного мессенджера через pNext автоматически
        // создаст этот мессенджер для его работы во время создания и уничтожения экземпляра Vulkan
        // через vkCreateInstance() и vkDestroyInstance() соответственно. Он не мешает работе основного
        // отладочного мессенджера и будет очищен вместе с экземпляром.
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
}

// Проверка доступны ли Vulkan расширения, требуемые для работы движка.
void WrpDevice::hasEngineRequiredInstanceExtensions()
{
    // Ищем и выводим все доступные расширения экземпляра
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

    // Выводим требуемые расширения экземпляра и выбрасываем исключение, если
    // какого-то расширения не хватает для работы движка.
    std::cout << "Required extensions:" << std::endl;
    auto requiredExtensions = getRequiredExtensions();
    for (const auto& required : requiredExtensions)
    {
        std::cout << "\t" << required << std::endl;
        if (available.find(required) == available.end())
        {
            throw std::runtime_error("Missing required GLFW or Vulkan debug extension.");
        }
    }
}

// Формирование и возврат вектора расширений, требуемых для работы движка.
std::vector<const char*> WrpDevice::getRequiredExtensions()
{
    // Встроенная в GLFW функция создаёт массив с расширениями, которые должен 
    // использовать Vulkan для взаимодействия с оконной системой (как минимум "VK_KHR_surface").
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    // Добавление расширения для отладочного мессенджера (он обрабатывает вывод слоёв проверки)
    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // extension to set up debug messanger
    }

    return extensions;
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
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);
        isSwapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    // запрос поддерживаемого функционала данного GPU
    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);

    return indices.isComplete() && extensionsSupported && isSwapChainAdequate && supportedFeatures.samplerAnisotropy;
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

    // Заполнение QueueCreateInfo для каждого из нужных семейств очередей
    float queuePriority = 1.0f;
    for (std::optional<uint32_t> queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily.value();
        queueCreateInfo.queueCount = 1; // создать только одну очередь из данного семейства
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures = {}; // возможности ус-ва для активации
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo createInfo = {}; // структура для создания логического ус-ва
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    // Слои проверок уровня девайса теперь являются устаревшими, но их всё равно стоит указывать для сохранения
    // совместимости со старыми реализациями. Слои берутся такие же, как и для экземпляра.
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

// Настройка и создание отладочного мессенджера
void WrpDevice::setupDebugMessenger()
{
    if (!enableValidationLayers) return;
    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);
    if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to set up debug messenger!");
    }
}

// Заполнение информации для создания дескриптора отладочного мессенджера
void WrpDevice::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    // messageSeverity задаёт виды серьёзности сообщения от valid. layer для отлова мессенджером
    // Функция обратного вызова выполняется только для указанных видов. (не указаны INFO и VERBOSE).
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    // messageType таким же образом фильтрует сообщения по их типу (сейчас указаны все типы)
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback; // указатель на саму функцию обратного вызова
    createInfo.pUserData = nullptr;  // любой указатель для передачи данных в callback-функцию (Optional)
}

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

// Проверка поддерживаются ли требуемые расширения данным физическим девайсом
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

// Заполнение структуры деталей поддержки цепи обмена
SwapChainSupportDetails WrpDevice::querySwapChainSupport(VkPhysicalDevice physicalDevice)
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
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            physicalDevice,
            surface_,
            &presentModeCount,
            details.presentModes.data());
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
    throw std::runtime_error("Failed to find supported format!");
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

ENGINE_END
