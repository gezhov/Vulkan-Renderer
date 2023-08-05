#include "texture.hpp"
#include "buffer.hpp"

// libs
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// std
#include <cassert>
#include <cstring>
#include <stdexcept>

ENGINE_BEGIN

    WrpTexture::WrpTexture(const std::string& path, WrpDevice& device) : vgetDevice{device}
    {
        createTextureImage(path);
        createTextureImageView();
        createTextureSampler();
    }

    WrpTexture::~WrpTexture()
    {
        vkDestroySampler(vgetDevice.device(), textureSampler, nullptr);
        vkDestroyImageView(vgetDevice.device(), textureImageView, nullptr);
        vkDestroyImage(vgetDevice.device(), textureImage, nullptr);
        vkFreeMemory(vgetDevice.device(), textureImageMemory, nullptr);
    }

    void WrpTexture::createTextureImage(const std::string& path)
    {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        uint32_t pixelCount = texWidth * texHeight;
        uint32_t pixelSize = 4;
        VkDeviceSize imageSize = pixelCount * pixelSize;

        if (!pixels)
        {
            throw std::runtime_error("failed to load texture image!");
        }

        // Создание промежуточного буфера
        WrpBuffer stagingBuffer
        {
            vgetDevice,
            pixelSize,
            pixelCount,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // буфер используется как источник для операции переноса памяти
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        };

        stagingBuffer.map();
        stagingBuffer.writeToBuffer((void*)pixels); // запись пикселей в память девайса
        stbi_image_free(pixels); // очистка изначально прочитанного массива пикселей

        // Создание изображения и выделение памяти под него
        createImage(texWidth, texHeight,
            VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            textureImage, textureImageMemory);

        // Копируем буфер с пикселами в изображение текстуры, при этом меняя лэйауты на нужные
        transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        vgetDevice.copyBufferToImage(stagingBuffer.getBuffer(), textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1);
        transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    void WrpTexture::createImage(
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImage& image,
        VkDeviceMemory& imageMemory)
    {
        // Создание объекта VkImage
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;	   // кол-во текселей по X
        imageInfo.extent.height = height;  // кол-во текселей по Y
        imageInfo.extent.depth = 1;		   // кол-во текселей по Z
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;  // исп. оптимальное расположение текселей, заданное реализацией
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // VkImage исп. для получения данных из буфера (TRANSFER_DST), а затем для выборки цвета для меша (SAMPLED)
        imageInfo.usage = usage;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  // изображение исп. только одним семейством очередей
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.flags = 0;	// Optional

        // Выделение памяти под изображение в девайсе
        vgetDevice.createImageWithInfo(imageInfo, properties, image, imageMemory);
    }

    // Смена схемы изображения
    void WrpTexture::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        VkCommandBuffer commandBuffer = vgetDevice.beginSingleTimeCommands();

        // Для смены схемы будет использоваться барьер для изображения
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // для смены семейства очередей у ресурса (не используется)
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        // изображение и его определённая часть, которые затрагивает смена лэйаута
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        // Описание действий барьера при переходе в лэйаут получения передачи
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;								// какие операции с данным ресурсом должны произойти перед барьером
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;   // какие операции с данным ресурсом должны ожидать на барьере

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;		// этап пайплайна для выполнения операций перед барьером
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;		// этап, на котором операции начнут ожидание
        }
        // Описание действий барьера при переходе в лэйаут для чтения шейдером
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else
        {
            throw std::invalid_argument("unsupported layout transition!");
        }

        vkCmdPipelineBarrier(
            commandBuffer,
            sourceStage, destinationStage,
            0,
            0, nullptr,	  // ссылка на массив барьеров памяти
            0, nullptr, // массив барьеров памяти буфера
            1, &barrier  // массив барьеров памяти изображения
        );

        vgetDevice.endSingleTimeCommands(commandBuffer);
    }

    // Создание представления изображения для текстуры
    void WrpTexture::createTextureImageView()
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = textureImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(vgetDevice.device(), &viewInfo, nullptr, &textureImageView) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create texture image view!");
        }
    }

    // Создание выборщика для текстуры (ищет цвет, соответствующий координате)
    void WrpTexture::createTextureSampler()
    {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR; // Фильтрация для увеличенных (magnified) текселей. Это случай, когда их больше, чем фрагментов (oversampling)
        samplerInfo.minFilter = VK_FILTER_LINEAR; // Для уменьшенных (minified) текселей. Это случай, когда их меньше, чем фрагментов (undersampling)
        // Режим адресации цвета по заданной оси. U, V, W используются вместо X, Y и Z по соглашению для координат пространства текстуры
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT; // Повторять текстуру, если координата выходит за пределы изображения
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = vgetDevice.properties.limits.maxSamplerAnisotropy; // макс. кол-во текселей для расчёт финального цвета при анизотропной фильтрации
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;	// координаты будут адресоваться в диапазоне [0;1), т.е. они нормализованы для универсального использования
        samplerInfo.compareEnable = VK_FALSE;			// функция сравнения выбранного текселя с заданным значением отключена (исп. в precentage-closer фильтрации в картах теней)
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        // поля для настройки мипмэппинга
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        if (vkCreateSampler(vgetDevice.device(), &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }

    VkDescriptorImageInfo WrpTexture::descriptorInfo()
    {
        return VkDescriptorImageInfo {
            textureSampler,
            textureImageView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
   }

ENGINE_END
