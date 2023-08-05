#pragma once

#include "device.hpp"

ENGINE_BEGIN

class VgetTexture
{
public:
	VgetTexture(const std::string& path, VgetDevice& device);
	~VgetTexture();

	VkDescriptorImageInfo descriptorInfo();

private:
	void createImage(
		uint32_t width,
		uint32_t height,
		VkFormat format,
		VkImageTiling tiling,
		VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties,
		VkImage& image,
		VkDeviceMemory& imageMemory);

	void createTextureImage(const std::string& path);
	void createTextureImageView();
	void createTextureSampler();

	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

	VgetDevice& vgetDevice;

	VkImage textureImage;
	VkDeviceMemory textureImageMemory;
	VkImageView textureImageView;
	VkSampler textureSampler;
};

ENGINE_END
