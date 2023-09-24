#include "Texture.hpp"
#include "Buffer.hpp"

// libs
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// std
#include <cassert>
#include <cstring>
#include <cmath>
#include <stdexcept>

ENGINE_BEGIN

WrpTexture::WrpTexture(const std::string& path, WrpDevice& device) : wrpDevice{device}
{
    createTexture(path);
    createTextureImageView(mipLevels);
    createTextureSampler(mipLevels);
}

WrpTexture::~WrpTexture()
{
    vkDestroySampler(wrpDevice.device(), textureSampler, nullptr);
    vkDestroyImageView(wrpDevice.device(), textureImageView, nullptr);
    vkDestroyImage(wrpDevice.device(), textureImage, nullptr);
    vkFreeMemory(wrpDevice.device(), textureImageMemory, nullptr);
}

// creates image and imageView for the texture
void WrpTexture::createTexture(const std::string& path)
{
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    uint32_t pixelCount = texWidth * texHeight;
    uint32_t pixelSize = 4;
    VkDeviceSize imageSize = pixelCount * pixelSize;
    mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

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
    VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
    VkImageTiling imageTiling = VK_IMAGE_TILING_OPTIMAL;
    createTextureImage(texWidth, texHeight, mipLevels,
        imageFormat,
        imageTiling,  // implementation defined optimal texels tiling
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | // for mipmaps generaion
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | // for staging buffer copyoing to the image
        VK_IMAGE_USAGE_SAMPLED_BIT,       // for color sampling in the shader
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        textureImage, textureImageMemory
    );

    // Copying pixels buffer to the texture Image with layout transition to proper ones along the way
    transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels
    );
    wrpDevice.copyBufferToImage(stagingBuffer.getBuffer(), textureImage,
        static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1
    );
    // transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps
    generateMipmaps(textureImage, imageFormat, imageTiling, texWidth, texHeight, mipLevels);
}

void WrpTexture::createTextureImage(
    uint32_t width,
    uint32_t height,
    uint32_t mipLevels,
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
    imageInfo.mipLevels = mipLevels;
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

void WrpTexture::transitionImageLayout (VkImage image, VkFormat format,
    VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels)
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
    barrier.subresourceRange.levelCount = mipLevels;
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

void WrpTexture::generateMipmaps(VkImage image, VkFormat imageFormat, VkImageTiling imageTiling,
    int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
    // Checking if used image format supports linear filtering for mipmap generation
    wrpDevice.findSupportedFormat(std::vector<VkFormat>{imageFormat},
        imageTiling, VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);

    VkCommandBuffer commandBuffer = wrpDevice.beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; ++i)
    {
        // i-1 level transition to src_optimal to implement image blit down below
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;  // wait for i-1 lvl to be filled
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;   // current blit op should wait
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier   // ImageMemoryBarriers
        );

        // ImageBlit используется для переноса данных из одного уровня mip уровня в другой
        VkImageBlit blit{};
        // перенос полного размера i-1'ой картинки 
        blit.srcOffsets[0] = {0, 0, 0};                 // offset
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};  // dimensions
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        // i'ый уровень получит вдвое уменьшенную картинку с предыдущего уровня
        blit.dstOffsets[0] = {0,0,0};
        blit.dstOffsets[1] = {
            mipWidth > 1 ? mipWidth / 2 : 1,
            mipHeight > 1 ? mipHeight / 2 : 1,
            1
        };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(
            commandBuffer,
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR
        );

        // i-1 level transition to shader_read_only for upcoming shader sampling
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        // shrunking down dimensions values for the next mip level creation
        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    // transition to shader_read_only for the last mip level
    // that doesn't used for mipmap creation
    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    wrpDevice.endSingleTimeCommands(commandBuffer);
}

void WrpTexture::createTextureImageView(uint32_t mipLevels)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = textureImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(wrpDevice.device(), &viewInfo, nullptr, &textureImageView) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create texture image view!");
    }
}

void WrpTexture::createTextureSampler(uint32_t mipLevels)
{
    // goog explanation for mipmapping sampling: https://vulkan-tutorial.com/Generating_Mipmaps#page_Sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR; // filtering for oversampling (when object is close)
    samplerInfo.minFilter = VK_FILTER_LINEAR; // for undersampling (when it is further from camera)
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
    samplerInfo.maxLod = static_cast<float>(mipLevels);

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
