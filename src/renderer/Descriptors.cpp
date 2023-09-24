#include "Descriptors.hpp"

// std
#include <cassert>
#include <stdexcept>
#include <iostream>

ENGINE_BEGIN

// *************** Descriptor Set Layout Builder *********************

WrpDescriptorSetLayout::Builder& WrpDescriptorSetLayout::Builder::addBinding(
	uint32_t binding,
	VkDescriptorType descriptorType,
	VkShaderStageFlags stageFlags,
	uint32_t count)
{
	assert(bindings.count(binding) == 0 && "Binding already in use.");
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.binding = binding;
	layoutBinding.descriptorType = descriptorType;
	layoutBinding.descriptorCount = count; // массив дескрипторов в наборе, если count > 1
	layoutBinding.stageFlags = stageFlags;
	layoutBinding.pImmutableSamplers = nullptr; // Optional
	bindings[binding] = layoutBinding;
	return *this;
}

std::unique_ptr<WrpDescriptorSetLayout> WrpDescriptorSetLayout::Builder::build() const
{
	return std::make_unique<WrpDescriptorSetLayout>(wrpDevice, bindings);
}

// *************** Descriptor Set Layout *********************

WrpDescriptorSetLayout::WrpDescriptorSetLayout(
	WrpDevice& wrpDevice, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings)
	: wrpDevice{wrpDevice}, bindings{bindings}
{
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
	for (auto& kv : bindings)
	{
		setLayoutBindings.push_back(kv.second);
	}

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
	descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
	descriptorSetLayoutInfo.pBindings = setLayoutBindings.data();

	if (vkCreateDescriptorSetLayout(wrpDevice.device(), &descriptorSetLayoutInfo,
		nullptr, &descriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create descriptor set layout!");
	}
}

WrpDescriptorSetLayout::~WrpDescriptorSetLayout()
{
	vkDestroyDescriptorSetLayout(wrpDevice.device(), descriptorSetLayout, nullptr);
}

// *************** Descriptor Pool Builder *********************

WrpDescriptorPool::Builder& WrpDescriptorPool::Builder::addPoolSize(
	VkDescriptorType descriptorType, uint32_t count)
{
	poolSizes.push_back({descriptorType, count});
	return *this;
}

WrpDescriptorPool::Builder& WrpDescriptorPool::Builder::setPoolFlags(
	VkDescriptorPoolCreateFlags flags)
{
	poolFlags = flags;
	return *this;
}

WrpDescriptorPool::Builder& WrpDescriptorPool::Builder::setMaxSets(uint32_t count)
{
	maxSets = count;
	return *this;
}

std::unique_ptr<WrpDescriptorPool> WrpDescriptorPool::Builder::build() const
{
	return std::make_unique<WrpDescriptorPool>(wrpDevice, maxSets, poolFlags, poolSizes);
}

// *************** Descriptor Pool *********************

WrpDescriptorPool::WrpDescriptorPool(
	WrpDevice& wrpDevice,
	uint32_t maxSets,
	VkDescriptorPoolCreateFlags poolFlags,
	const std::vector<VkDescriptorPoolSize>& poolSizes)
	: wrpDevice{wrpDevice}
{
	VkDescriptorPoolCreateInfo descriptorPoolInfo{};
	descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	descriptorPoolInfo.pPoolSizes = poolSizes.data();
	descriptorPoolInfo.maxSets = maxSets;
	descriptorPoolInfo.flags = poolFlags;

	if (vkCreateDescriptorPool(wrpDevice.device(), &descriptorPoolInfo, nullptr, &descriptorPool) !=
		VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create descriptor pool!");
	}
}

WrpDescriptorPool::~WrpDescriptorPool()
{
	vkDestroyDescriptorPool(wrpDevice.device(), descriptorPool, nullptr);
}

bool WrpDescriptorPool::allocateDescriptorSet(
	const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptorSet) const
{
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.pSetLayouts = &descriptorSetLayout; // в лэйауте обозначен тип и кол-во дескрипторов в наборе
	allocInfo.descriptorSetCount = 1;

	// todo: Might want to create a "DescriptorPoolManager" class that handles this case, and builds
	// a new pool whenever an old pool fills up. But this is beyond our current scope
	if (vkAllocateDescriptorSets(wrpDevice.device(), &allocInfo, &descriptorSet) != VK_SUCCESS)
	{
		std::cerr << "Failed to allocate descriptor sets!" << std::endl;
		return false;
	}
	return true;
}

void WrpDescriptorPool::freeDescriptors(std::vector<VkDescriptorSet>& descriptors) const
{
	vkFreeDescriptorSets(
		wrpDevice.device(),
		descriptorPool,
		static_cast<uint32_t>(descriptors.size()),
		descriptors.data());
}

void WrpDescriptorPool::resetPool()
{
	vkResetDescriptorPool(wrpDevice.device(), descriptorPool, 0);
}

// *************** Descriptor Writer *********************

WrpDescriptorWriter::WrpDescriptorWriter(WrpDescriptorSetLayout& setLayout, WrpDescriptorPool& pool)
	: setLayout{setLayout}, pool{pool}
{
}

// Создание объекта записи VkWriteDescriptorSet и добавление его в вектор записей.
// Эти записи нужны для добавления/обновления информации о дескрипторах в заданном наборе.
// Конкретно эта функция готовит к записи в дескриптор информации о его буфере.
WrpDescriptorWriter& WrpDescriptorWriter::writeBuffer(
	uint32_t binding, VkDescriptorBufferInfo* bufferInfo)
{
	assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding.");

	auto& bindingDescription = setLayout.bindings[binding];

	assert(bindingDescription.descriptorCount == 1 &&
		"DescriptorCount in VkWriteDescriptorSet for buffer resource "
		"should be 1, but DSLayout binding expects multiple.");

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.descriptorType = bindingDescription.descriptorType;
	write.dstBinding = binding;
	write.pBufferInfo = bufferInfo; // содержит descriptorCount VkBufferInfo конфигов, если обновляется массив дескрипторов
	write.dstArrayElement = 0; // начальный элемент, если дескриптор является массивом
	write.descriptorCount = 1; // сколько обновить дескрипторов в массиве

	writes.push_back(write);
	return *this;
}

// Подготовка записи в дескриптор информации о его ресурсе-изображении 
WrpDescriptorWriter& WrpDescriptorWriter::writeImage(
	uint32_t binding, VkDescriptorImageInfo* imageInfo, uint32_t count)
{
	assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding.");

	auto& bindingDescription = setLayout.bindings[binding];

	assert(bindingDescription.descriptorCount == count &&
		"DescriptorCount in VkWriteDescriptorSet for image resource "
		"is not equal to DSLayout binding's descriptor count.");

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.descriptorType = bindingDescription.descriptorType;
	write.dstBinding = binding;
	write.pImageInfo = imageInfo; // Единственное отличие от writeBuffer()
	write.descriptorCount = count;
	write.dstArrayElement = 0;

	writes.push_back(write);
	return *this;
}

bool WrpDescriptorWriter::build(VkDescriptorSet& set)
{
	bool success = pool.allocateDescriptorSet(setLayout.getDescriptorSetLayout(), set);
	if (!success)
	{
		return false;
	}
	overwrite(set);
	return true;
}

// Эта функция обновляет данные дескрипторов при выделении набора из пула,
// либо её можно вызвать отдельно для обновления данных, связанных с дескрипторами данного набора.
void WrpDescriptorWriter::overwrite(VkDescriptorSet& set)
{
	for (auto& write : writes)
	{
		write.dstSet = set; // к структурам записей добавляется обновляемый набор
	}
	vkUpdateDescriptorSets(pool.wrpDevice.device(), writes.size(), writes.data(), 0, nullptr);
}

ENGINE_END
