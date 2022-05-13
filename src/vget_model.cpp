#include "vget_model.hpp"
#include "vget_utils.hpp"

// libs
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// std
#include <cassert>
#include <cstring>
#include <iostream>
#include <unordered_map>

#ifndef ENGINE_DIR
#define ENGINE_DIR "../"
#endif

namespace std
{
	template<>
	struct hash<vget::VgetModel::Vertex>
	{
		size_t operator()(vget::VgetModel::Vertex const& vertex) const
		{
			size_t seed = 0;
			vget::hashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv);
			return seed;
		}
	};
}

namespace vget
{
	VgetModel::VgetModel(VgetDevice& device, const VgetModel::Builder& builder) : vgetDevice{device}
	{
		createVertexBuffers(builder.vertices);
		createIndexBuffers(builder.indices);
	}

	VgetModel::~VgetModel()
	{
		/* todo уничтожается два раза из-за того, что очищается указатель на model в first_app, а потом ещё раз при удалении игрового объекта,
		   т.е. нужно вынести уничтожение в Texture Объект, а тут должно быть пусто */
		vkDestroySampler(vgetDevice.device(), textureSampler, nullptr);
		vkDestroyImageView(vgetDevice.device(), textureImageView, nullptr);
		vkDestroyImage(vgetDevice.device(), textureImage, nullptr);
		vkFreeMemory(vgetDevice.device(), textureImageMemory, nullptr);
	}

	std::unique_ptr<VgetModel> VgetModel::createModelFromFile(VgetDevice& device, const std::string& filepath)
	{
		Builder builder{};
		builder.loadModel(ENGINE_DIR + filepath);
		std::cout << "Vertex count: " << builder.vertices.size() << "\n";
		return std::make_unique<VgetModel>(device, builder);
	}

	void VgetModel::createVertexBuffers(const std::vector<Vertex>& vertices)
	{
		vertexCount = static_cast<uint32_t>(vertices.size());
		assert(vertexCount >= 3 && "Vertex count must be at least 3");
		VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;
		uint32_t vertexSize = sizeof(vertices[0]);

		// Создание промежуточного буфера
		VgetBuffer stagingBuffer
		{
			vgetDevice,
			vertexSize,
			vertexCount,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // буфер используется как источник для операции переноса памяти
			// HOST_VISIBLE флаг указывает на то, что хост (CPU) будет иметь доступ к размещённой в девайсе (GPU) памяти.
			// Это важно для получения возможности писать данные в память GPU.
			// HOST_COHERENT флаг включает полное соответствие памяти хоста и девайса. Это даёт возможность легко
			// передавать изменения из памяти CPU в память GPU.
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		};

		stagingBuffer.map();
		stagingBuffer.writeToBuffer((void*)vertices.data());

		// Создание буфера для данных о вершинах в локальной памяти девайса
		vertexBuffer = std::make_unique<VgetBuffer>(
			vgetDevice,
			vertexSize,
			vertexCount,
			// Буфер используется для входных данных вершин, а данные для него будут перенесены из другого источника (из промежуточного буфера)
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			// DEVICE_LOCAL флаг указывает на то, что данный буфер будет размещён в оптимальной и быстрой локальной памяти девайса
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);

		// Функция, записывающая команду копирования в командный буфер,
		// который сразу же отправляется в очередь на выполнение.
		vgetDevice.copyBuffer(stagingBuffer.getBuffer(), vertexBuffer->getBuffer(), bufferSize);
	}

	void VgetModel::createIndexBuffers(const std::vector<uint32_t>& indices)
	{
		indexCount = static_cast<uint32_t>(indices.size());
		hasIndexBuffer = indexCount > 0;
		if (!hasIndexBuffer) return;

		VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;
		uint32_t indexSize = sizeof(indices[0]);

		// Создание промежуточного буфера
		VgetBuffer stagingBuffer {
			vgetDevice,
			indexSize,
			indexCount,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		};

		// Маппинг памяти из девайса и передача туда данных по аналогии со staging буфером из createVertexBuffers()
		stagingBuffer.map();
		stagingBuffer.writeToBuffer((void*)indices.data());

		// Создание буфера для индексов в локальной памяти девайса
		indexBuffer = std::make_unique<VgetBuffer>(
			vgetDevice,
			indexSize,
			indexCount,
			// Буфер используется для индексов, а данные для него будут перенесены из другого источника (из промежуточного буфера)
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);

		vgetDevice.copyBuffer(stagingBuffer.getBuffer(), indexBuffer->getBuffer(), bufferSize);
	}

	// todo принимать строку std::string и соединять путь с ENGINE_DIR
	void VgetModel::createTextureImage()
	{
		int texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load("../textures/khr_vulkan_logo.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		uint32_t pixelCount = texWidth * texHeight;
		uint32_t pixelSize = 4;
		VkDeviceSize imageSize = pixelCount * pixelSize;

		if (!pixels)
		{
			throw std::runtime_error("failed to load texture image!");
		}

		// Создание промежуточного буфера
		VgetBuffer stagingBuffer
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
		vgetDevice.copyBufferToImage(stagingBuffer.getBuffer(), textureImage,
			static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1);
		transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	void VgetModel::createImage(
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
	void VgetModel::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		VkCommandBuffer commandBuffer = vgetDevice.beginSingleTimeCommands();

		// Для смены схемы будет использоваться барьер для памяти изображения
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
	void VgetModel::createTextureImageView()
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
	void VgetModel::createTextureSampler()
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

	// todo структура с данными по дескриптору изображения. нужно вынести в отдельную функцию текстуры наподобие texture->descriptorInfo()
	VkDescriptorImageInfo VgetModel::descriptorInfo()
	{
		return VkDescriptorImageInfo {
			textureSampler,
			textureImageView,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};
	}

	void VgetModel::draw(VkCommandBuffer commandBuffer)
	{
		if (hasIndexBuffer)
		{
			// Запись команды на отрисовку с применением буфера индексов
			vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
		}
		else
		{
			// Запись команды на отрисовку. (vertexCount вершин, 1 экземпляр, без смещений)
			vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
		}
	}

	void VgetModel::bind(VkCommandBuffer commandBuffer)
	{
		VkBuffer buffers[] = { vertexBuffer->getBuffer() };
		VkDeviceSize offsets[] = { 0 };

		// Запись команды в буфер команд о создании привязки буфера вершин к пайплайну.
		// После выполнения данная команда создаст Binding[0] для одного буфера вершин из buffers с отступом offsets внутри этого буфера.
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

		if (hasIndexBuffer)
		{
			// Команда создания привязки буфера индексов (если он есть) к пайплайну.
			// Тип индекса должен совпадать с типом данных в самом буфере и может выбираться
			// поменьше для экономии памяти при использовании простых моделей объектов.
			vkCmdBindIndexBuffer(commandBuffer, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
		}
	}

	std::vector<VkVertexInputBindingDescription> VgetModel::Vertex::getBindingDescriptions()
	{
		// Создание описания привязки для буфера вершин.
		std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
		bindingDescriptions[0].binding = 0;										// индекс занимаемой буфером привязки
		bindingDescriptions[0].stride = sizeof(Vertex);							// вершины расположены с шагом в sizeof(Vertex) байт
		bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescriptions;
	}

	std::vector<VkVertexInputAttributeDescription> VgetModel::Vertex::getAttributeDescriptions()
	{
		// Создание описания атрибута внутри буфера вершин
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
		
		// position attribute
		VkVertexInputAttributeDescription attribDescription{};
		attribDescription.binding = 0;								// атрибут расположен в привязке с индексом 0
		attribDescription.location = 0;								// Номер location'а в шейдере вершин для заданного атрибута
		attribDescription.format = VK_FORMAT_R32G32B32_SFLOAT;		// Тип данных данного атрибута
		attribDescription.offset = offsetof(Vertex, position);		// Отступ от начала буфера до данного атрибута
		attributeDescriptions.push_back(attribDescription);

		// Иницизализация оставшихся описаний атрибутов представлена в сжатой форме
		attributeDescriptions.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)});
		attributeDescriptions.push_back({2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)});
		attributeDescriptions.push_back({3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)});

		return attributeDescriptions;
	}

	void VgetModel::Builder::loadModel(const std::string& filepath)
	{
		tinyobj::attrib_t attrib;						// содержит данные позиций, цветов, нормалей и координат текстур
		std::vector<tinyobj::shape_t> shapes;			// shapes хранит значения индексов для каждого из face элементов
		std::vector<tinyobj::material_t> materials;		// materials хранит данные материалах
		std::string warn, err;

		// После успешного выполнения функции LoadObj() переданные локальные переменный заполнятся
		// данными из предоставленного .obj файла
		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str()))
		{
			throw std::runtime_error(warn + err);
		}

		// очистка текущей структуры перед загрузкой новой модели
		vertices.clear();
		indices.clear();

		// Мапа хранит уникальные вершины с их индексами. С её помощью составляется буфер индексов.
		std::unordered_map<Vertex, uint32_t> uniqueVertices{};

		// Проходимся по каждому face элементу модели
		for (const auto &shape : shapes)
		{
			// Проходимся по индексам каждой из вершин примитива
			for (const auto &index : shape.mesh.indices)
			{
				Vertex vertex{};

				if (index.vertex_index >= 0) // отрицательный индекс означает, что позиция не была предоставлена
				{
					// с помощью текущего индекса позиции извлекаем из атрибутов позицию вершины
					vertex.position = {
						attrib.vertices[3 * index.vertex_index + 0], // x
						attrib.vertices[3 * index.vertex_index + 1], // y
						attrib.vertices[3 * index.vertex_index + 2], // z
					};

					// по таким же индексам из атрибутов извелкается цвет вершины, если он был представлен в файле
					vertex.color = {
						attrib.colors[3 * index.vertex_index + 0], // r
						attrib.colors[3 * index.vertex_index + 1], // g
						attrib.colors[3 * index.vertex_index + 2], // b
					};
				}

				// извлекаем из атрибутов позицию нормали
				if (index.normal_index >= 0)
				{
					vertex.normal = {
						attrib.normals[3 * index.normal_index + 0], // x
						attrib.normals[3 * index.normal_index + 1], // y
						attrib.normals[3 * index.normal_index + 2], // z
					};
				}

				// извлекаем из атрибутов координаты текстуры
				if (index.texcoord_index >= 0)
				{
					vertex.uv = {
						attrib.texcoords[2 * index.texcoord_index + 0], // u
						attrib.texcoords[2 * index.texcoord_index + 1], // v
					};
				}

				// Если считанная вершина не найдена в мапе, то она добавляется в неё и получает
				// свой индекс, а затем добавляется в вектор builder'а
				if (uniqueVertices.count(vertex) == 0)
				{
					uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
					vertices.push_back(vertex);
				}
				indices.push_back(uniqueVertices[vertex]); // Буфер индексов добавляет индекс считанной вершины
			}
		}
	}

}