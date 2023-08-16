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

WrpTexture::WrpTexture(const std::string& path, WrpDevice& device) : wrpDevice{device}
{
    createTextureImage(path);
    createTextureImageView();
    createTextureSampler();
}

WrpTexture::~WrpTexture()
{
    vkDestroySampler(wrpDevice.device(), textureSampler, nullptr);
    vkDestroyImageView(wrpDevice.device(), textureImageView, nullptr);
    vkDestroyImage(wrpDevice.device(), textureImage, nullptr);
    vkFreeMemory(wrpDevice.device(), textureImageMemory, nullptr);
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
        throw std::runtime_error("Failed to load texture image!");
    }

    // host visible staging buffer for image data transfering 
    WrpBuffer stagingBuffer
    {
        wrpDevice,
        pixelSize,
        pixelCount,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };

    stagingBuffer.map();
    stagingBuffer.writeToBuffer((void*)pixels); // writing pixels to devices memory 
    stbi_image_free(pixels);

    // Creating VkImage 
    createImage(texWidth, texHeight,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,  // implementation defined optimal texels tiling
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // use for color sampling in the shader
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        textureImage, textureImageMemory
    );

    // Copying pixels buffer to the texture Image with layout transition to proper ones along the way
    transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );
    wrpDevice.copyBufferToImage(stagingBuffer.getBuffer(), textureImage,
        static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1
    );
    transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
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
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;	   // texels count by X
    imageInfo.extent.height = height;  // texels count by Y
    imageInfo.extent.depth = 1;		   // texels count by Z
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // this image is not for staging so there is no initial layout
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;   // used by only one family queue
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;           // multisampling is not be used
    imageInfo.flags = 0;	// Optional

    // Creating image and allocating memory for it on the device
    wrpDevice.createImageWithInfo(imageInfo, properties, image, imageMemory);
}

void WrpTexture::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBuffer commandBuffer = wrpDevice.beginSingleTimeCommands();

    // ImageMemoryBarrier helps with image layout transition 
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // for queue family ownership transition (not used)
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    // the image and its specific part to change layout for
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    // setting up pipeline barrier options
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;								// operations with the resource to happen before the barrier 
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;   // ops w/ the resource to wait on the barrier

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;		// on which stage to perform initial ops 
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;		// on which stage target ops need to wait
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        throw std::invalid_argument("Unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,	  // MemoryBarriers
        0, nullptr,   // BufferMemoryBarriers
        1, &barrier   // ImageMemoryBarriers
    );

    wrpDevice.endSingleTimeCommands(commandBuffer);
}

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

    if (vkCreateImageView(wrpDevice.device(), &viewInfo, nullptr, &textureImageView) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create texture image view!");
    }
}

void WrpTexture::createTextureSampler()
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR; // filtering for oversampling
    samplerInfo.minFilter = VK_FILTER_LINEAR; // for undersampling
    // Repeat texture, if sampling beyond its extent
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    // max amount of texel samples to calculate the final color
    samplerInfo.maxAnisotropy = wrpDevice.properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;	// coordinates will be addressed in [0;1) range 
    samplerInfo.compareEnable = VK_FALSE;     // texels is not comparing with a value
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    // mipmapping settings
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(wrpDevice.device(), &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create texture sampler!");
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
