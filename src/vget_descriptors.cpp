#include "vget_descriptors.hpp"

// std
#include <cassert>
#include <stdexcept>

namespace vget
{
	// *************** Descriptor Set Layout Builder *********************

	VgetDescriptorSetLayout::Builder& VgetDescriptorSetLayout::Builder::addBinding(
		uint32_t binding,
		VkDescriptorType descriptorType,
		VkShaderStageFlags stageFlags,
		uint32_t count)
	{
		assert(bindings.count(binding) == 0 && "Binding already in use");
		VkDescriptorSetLayoutBinding layoutBinding{};
		layoutBinding.binding = binding;
		layoutBinding.descriptorType = descriptorType;
		layoutBinding.descriptorCount = count;
		layoutBinding.stageFlags = stageFlags;
		bindings[binding] = layoutBinding;
		return *this;
	}

	std::unique_ptr<VgetDescriptorSetLayout> VgetDescriptorSetLayout::Builder::build() const
	{
		return std::make_unique<VgetDescriptorSetLayout>(lveDevice, bindings);
	}

	// *************** Descriptor Set Layout *********************

	VgetDescriptorSetLayout::VgetDescriptorSetLayout(
		VgetDevice& lveDevice, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings)
		: lveDevice{lveDevice}, bindings{bindings}
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
		for (auto kv : bindings)
		{
			setLayoutBindings.push_back(kv.second);
		}

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
		descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
		descriptorSetLayoutInfo.pBindings = setLayoutBindings.data();

		if (vkCreateDescriptorSetLayout(
			lveDevice.device(),
			&descriptorSetLayoutInfo,
			nullptr,
			&descriptorSetLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create descriptor set layout!");
		}
	}

	VgetDescriptorSetLayout::~VgetDescriptorSetLayout()
	{
		vkDestroyDescriptorSetLayout(lveDevice.device(), descriptorSetLayout, nullptr);
	}

	// *************** Descriptor Pool Builder *********************

	VgetDescriptorPool::Builder& VgetDescriptorPool::Builder::addPoolSize(
		VkDescriptorType descriptorType, uint32_t count)
	{
		poolSizes.push_back({descriptorType, count});
		return *this;
	}

	VgetDescriptorPool::Builder& VgetDescriptorPool::Builder::setPoolFlags(
		VkDescriptorPoolCreateFlags flags)
	{
		poolFlags = flags;
		return *this;
	}

	VgetDescriptorPool::Builder& VgetDescriptorPool::Builder::setMaxSets(uint32_t count)
	{
		maxSets = count;
		return *this;
	}

	std::unique_ptr<VgetDescriptorPool> VgetDescriptorPool::Builder::build() const
	{
		return std::make_unique<VgetDescriptorPool>(lveDevice, maxSets, poolFlags, poolSizes);
	}

	// *************** Descriptor Pool *********************

	VgetDescriptorPool::VgetDescriptorPool(
		VgetDevice& lveDevice,
		uint32_t maxSets,
		VkDescriptorPoolCreateFlags poolFlags,
		const std::vector<VkDescriptorPoolSize>& poolSizes)
		: lveDevice{lveDevice}
	{
		VkDescriptorPoolCreateInfo descriptorPoolInfo{};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		descriptorPoolInfo.pPoolSizes = poolSizes.data();
		descriptorPoolInfo.maxSets = maxSets;
		descriptorPoolInfo.flags = poolFlags;

		if (vkCreateDescriptorPool(lveDevice.device(), &descriptorPoolInfo, nullptr, &descriptorPool) !=
			VK_SUCCESS)
		{
			throw std::runtime_error("failed to create descriptor pool!");
		}
	}

	VgetDescriptorPool::~VgetDescriptorPool()
	{
		vkDestroyDescriptorPool(lveDevice.device(), descriptorPool, nullptr);
	}

	// Выделяет именно набор дескрипторов! Выделить отдельный дескриптор из пула нельзя!
	bool VgetDescriptorPool::allocateDescriptor(
		const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor) const
	{
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.pSetLayouts = &descriptorSetLayout;
		allocInfo.descriptorSetCount = 1;

		// Might want to create a "DescriptorPoolManager" class that handles this case, and builds
		// a new pool whenever an old pool fills up. But this is beyond our current scope
		if (vkAllocateDescriptorSets(lveDevice.device(), &allocInfo, &descriptor) != VK_SUCCESS)
		{
			return false;
		}
		return true;
	}

	void VgetDescriptorPool::freeDescriptors(std::vector<VkDescriptorSet>& descriptors) const
	{
		vkFreeDescriptorSets(
			lveDevice.device(),
			descriptorPool,
			static_cast<uint32_t>(descriptors.size()),
			descriptors.data());
	}

	void VgetDescriptorPool::resetPool()
	{
		vkResetDescriptorPool(lveDevice.device(), descriptorPool, 0);
	}

	// *************** Descriptor Writer *********************

	VgetDescriptorWriter::VgetDescriptorWriter(VgetDescriptorSetLayout& setLayout, VgetDescriptorPool& pool)
		: setLayout{setLayout}, pool{pool}
	{
	}

	// Создание объекта записи VkWriteDescriptorSet и добавление его вектор записей.
	// Эти записи нужны для добавления/обновления информации о дескрипторах в заданном наборе.
	VgetDescriptorWriter& VgetDescriptorWriter::writeBuffer(
		uint32_t binding, VkDescriptorBufferInfo* bufferInfo)
	{
		assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");

		auto& bindingDescription = setLayout.bindings[binding];

		assert(
			bindingDescription.descriptorCount == 1 &&
			"Binding single descriptor info, but binding expects multiple");

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorType = bindingDescription.descriptorType;
		write.dstBinding = binding;
		write.pBufferInfo = bufferInfo;
		write.descriptorCount = 1;

		writes.push_back(write);
		return *this;
	}

	// Добавление записи дескриптора ресурса изображения
	VgetDescriptorWriter& VgetDescriptorWriter::writeImage(
		uint32_t binding, VkDescriptorImageInfo* imageInfo, uint32_t count)
	{
		assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");

		auto& bindingDescription = setLayout.bindings[binding];

		assert(
			bindingDescription.descriptorCount == count &&
			"Count of descriptor infos is not equal to layout binding's descriptor count");

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorType = bindingDescription.descriptorType;
		write.dstBinding = binding;
		write.pImageInfo = imageInfo; // Единственное отличие от writeBuffer()
		write.descriptorCount = count;

		writes.push_back(write);
		return *this;
	}

	bool VgetDescriptorWriter::build(VkDescriptorSet& set)
	{
		bool success = pool.allocateDescriptor(setLayout.getDescriptorSetLayout(), set);
		if (!success)
		{
			return false;
		}
		overwrite(set);
		return true;
	}

	// Эта функция записывает данные дескрипторов при выделении набора из пула, либо её можно вызвать
	// отдельно для обновления данных, хранящихся в дескрипторах данного набора.
	void VgetDescriptorWriter::overwrite(VkDescriptorSet& set)
	{
		for (auto& write : writes)
		{
			write.dstSet = set;
		}
		vkUpdateDescriptorSets(pool.lveDevice.device(), writes.size(), writes.data(), 0, nullptr);
	}
}
