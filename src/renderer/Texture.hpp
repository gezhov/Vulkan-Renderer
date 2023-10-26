#pragma once

#include "Device.hpp"

class WrpTexture
{
public:
    WrpTexture(const std::string& path, WrpDevice& device);
    ~WrpTexture();

    VkDescriptorImageInfo descriptorInfo();

private:
    void createTexture(const std::string& path);
    void createTextureImage(
        uint32_t width,
        uint32_t height,
        uint32_t mipLevels,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImage& image,
        VkDeviceMemory& imageMemory);
    void createTextureImageView(uint32_t mipLevels);
    void createTextureSampler(uint32_t mipLevels);

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout,
        VkImageLayout newLayout, uint32_t mipLevels);
    void generateMipmaps(VkImage image, VkFormat imageFormat, VkImageTiling imageTiling,
        int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

    WrpDevice& wrpDevice;

    uint32_t mipLevels;
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;
    VkSampler textureSampler;
};
