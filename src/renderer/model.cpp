#include "model.hpp"
#include "utils.hpp"

// libs
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

// std
#include <cassert>
#include <cstring>
#include <iostream>
#include <unordered_map>

namespace std
{
    // хэш-функция для Vertex, чтобы хранить его в мапе
    template<> struct hash<engine::WrpModel::Vertex>
    {
        size_t operator()(engine::WrpModel::Vertex const& vertex) const
        {
            size_t seed = 0;
            engine::hashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv);
            return seed;
        }
    };
}

ENGINE_BEGIN

WrpModel::WrpModel(WrpDevice& device, const WrpModel::Builder& builder)
    : wrpDevice{device}, subObjectsInfos{builder.subObjectsInfos}
{
    createVertexBuffers(builder.vertices);
    createIndexBuffers(builder.indices);
    createTextures(builder.texturePaths);
}

WrpModel::~WrpModel(){}

std::unique_ptr<WrpModel> WrpModel::createModelFromObjMtl(WrpDevice& device, const std::string& filepath)
{
    Builder builder{};
    builder.loadModel(filepath);
    std::cout << "Vertex count: " << builder.vertices.size() << "\n";
    return std::make_unique<WrpModel>(device, builder);
}

std::unique_ptr<WrpModel>
WrpModel::createModelFromObjTexture(WrpDevice& device, const std::string& modelPath, const std::string& texturePath)
{
    Builder builder{};
    builder.loadModel(modelPath);
    std::cout << "Vertex count: " << builder.vertices.size() << "\n";
    builder.texturePaths.emplace(texturePath, 0);
    
    return std::make_unique<WrpModel>(device, builder);
}

void WrpModel::createVertexBuffers(const std::vector<Vertex>& vertices)
{
    vertexCount = static_cast<uint32_t>(vertices.size());
    assert(vertexCount >= 3 && "Vertex count must be at least 3");

    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;
    uint32_t vertexSize = sizeof(vertices[0]);

    // Создание промежуточного буфера с данными вершин, который виден на хосте.
    // Буфер представлен локальной переменной, поэтому он очистится, после окончания функции.
    WrpBuffer stagingBuffer
    {
        wrpDevice,
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
    vertexBuffer = std::make_unique<WrpBuffer>(
        wrpDevice,
        vertexSize,
        vertexCount,
        // Буфер используется для входных данных вершин, а данные для него будут перенесены из другого источника (из промежуточного буфера)
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        // DEVICE_LOCAL флаг указывает на то, что данный буфер будет размещён в оптимальной и быстрой локальной памяти девайса
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    // copying buffer memory at the device itself through command submitting
    wrpDevice.copyBuffer(stagingBuffer.getBuffer(), vertexBuffer->getBuffer(), bufferSize);
}

void WrpModel::createIndexBuffers(const std::vector<uint32_t>& indices)
{
    indexCount = static_cast<uint32_t>(indices.size());
    hasIndexBuffer = indexCount > 0;
    if (!hasIndexBuffer) return;

    VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;
    uint32_t indexSize = sizeof(indices[0]);

    // Создание промежуточного буфера
    WrpBuffer stagingBuffer {
        wrpDevice,
        indexSize,
        indexCount,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };

    // Маппинг памяти из девайса и передача туда данных по аналогии со staging буфером из createVertexBuffers()
    stagingBuffer.map();
    stagingBuffer.writeToBuffer((void*)indices.data());

    // Создание буфера для индексов в локальной памяти девайса
    indexBuffer = std::make_unique<WrpBuffer>(
        wrpDevice,
        indexSize,
        indexCount,
        // Буфер используется для индексов, а данные для него будут перенесены из другого источника (из промежуточного буфера)
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    wrpDevice.copyBuffer(stagingBuffer.getBuffer(), indexBuffer->getBuffer(), bufferSize);
}

void WrpModel::createTextures(const std::unordered_map<std::string, int>& texturePaths)
{
    if (!texturePaths.empty()) hasTextures = true;
    else hasTextures = false;

    for (auto& path : texturePaths)
    {
        textures.push_back(std::make_unique<WrpTexture>(path.first, wrpDevice));
    }
}

void WrpModel::Builder::loadModel(const std::string& filepath)
{
    // obj файл состоит из атрибут и граней. грани состоят из вершин, включающих индексы своих атрибутов
    // tinyObjLoader парсит в следующую вложенность: shapes -> shape.mesh -> indices -> index_t.attribute 
    // все фигуры -> меш отдельной фигуры -> все вершины (индексы) этого меша -> index_t хранит индексы всех атрибутов отдельной вершины 
    tinyobj::attrib_t attrib;						// данные позиций, цветов, нормалей и координат текстур
    std::vector<tinyobj::shape_t> shapes;			// все отдельные фигуры составной модели
    std::vector<tinyobj::material_t> materials;		// данные о материалах
    std::string warn, err;                          // предупреждения и ошибки

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str(), MODELS_DIR))
    {
        throw std::runtime_error(warn + err);
    }

    // очистка текущей структуры Builder перед загрузкой новой модели
    vertices.clear();
    indices.clear();
    texturePaths.clear();

    // Данный способ считывания .obj объекта со множеством текстур в материале основан на данном топике:
    // https://www.reddit.com/r/vulkan/comments/826w5d/what_needs_to_be_done_in_order_to_load_obj_model/
    uint32_t indexCount = 0; // the number of indices to be drawn in one bundle
    auto indexStart = static_cast<uint32_t>(indices.size()); // index offset for drawing
    int textureId = 0;
    bool isThereMaterials = false;

    if (materials.size() != 0) isThereMaterials = !isThereMaterials;

    for (const auto& mat : materials)
    {
        if (mat.diffuse_texname != "")
        {
            std::string path = MODELS_DIR + mat.diffuse_texname;
            if (texturePaths.find(path) == texturePaths.end()) {
                texturePaths.emplace(path, textureId);
                ++textureId;
            }
        }
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{}; // helps with index buffer creation

    // Итерирование по каждой фигуре из obj файла
    for (const auto &shape : shapes)
    {
        // эти две переменные нужны, чтобы обозначить границы текущей фигуры
        indexCount = 0;
        indexStart = static_cast<uint32_t>(indices.size());

        // Итерирование по всем индексам вершин текущей фигуры
        for (const auto &index : shape.mesh.indices)
        {
            Vertex vertex{};

            if (index.vertex_index >= 0) // отрицательный индекс означает, что атрибут не был представлен
            {
                // с помощью текущего индекса позиции извлекаем из атрибутов позицию вершины
                vertex.position = {
                    attrib.vertices[3 * index.vertex_index + 0], // x
                    attrib.vertices[3 * index.vertex_index + 1], // y
                    attrib.vertices[3 * index.vertex_index + 2], // z
                };

                // по таким же индексам из атрибутов извлекается цвет вершины, если он был представлен в файле
                vertex.color = {
                    attrib.colors[3 * index.vertex_index + 0], // r
                    attrib.colors[3 * index.vertex_index + 1], // g
                    attrib.colors[3 * index.vertex_index + 2], // b
                };
            }

            if (index.normal_index >= 0)
            {
                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0], // x
                    attrib.normals[3 * index.normal_index + 1], // y
                    attrib.normals[3 * index.normal_index + 2], // z
                };
            }

            if (index.texcoord_index >= 0)
            {
                vertex.uv = {
                    attrib.texcoords[2 * index.texcoord_index + 0],		   // u
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1], // v (координата по Y переворачивается для коорд. системы вулкана)
                };
            }

            // Ведётся сохранение только уникальных вершин при помощи мапы
            if (uniqueVertices.count(vertex) == 0)
            {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }
            indices.push_back(uniqueVertices[vertex]); // добавляется индекс уникальной вершины

            ++indexCount;
        }

        SubObjectInfo info{indexCount, indexStart, -1, glm::vec3{}};
        if (isThereMaterials) {
            int materialId = shape.mesh.material_ids.at(0); // пока не встречал подобъекты с разными материалами на сетке, поэтому беру первый id
            int textureId = 0;
            std::string current_shape_texname = materials.at(materialId).diffuse_texname;
            if (texturePaths.find(MODELS_DIR + current_shape_texname) == texturePaths.end()) {
                textureId = -1;
            }
            else {
                textureId = texturePaths[MODELS_DIR + current_shape_texname];
            }

            // Данной фигуре .obj модели присваивается её начало, кол-во индексов, индекс текстуры из мапы текстур и диффузный цвет
            info = {
                indexCount,
                indexStart,
                textureId,
                glm::vec3(materials.at(materialId).diffuse[0], materials.at(materialId).diffuse[1], materials.at(materialId).diffuse[2])
            };
        }
        subObjectsInfos.push_back(info);
    }
}

void WrpModel::draw(VkCommandBuffer commandBuffer)
{
    if (hasIndexBuffer)
    {
        vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
    }
    else
    {
        vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
    }
}

void WrpModel::drawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t indexStart)
{
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, indexStart, 0, 0);
}

// Binding vertexBuffers and indexBuffer to graphics pipeline
void WrpModel::bind(VkCommandBuffer commandBuffer)
{
    VkBuffer buffers[] = { vertexBuffer->getBuffer() };
    VkDeviceSize offsets[] = { 0 };

    // This command create association between given vertex buffers and their bindings
    // in graphics pipeline which was set up earlier in getBindingDescriptions().
    // So in this particular case vertexBuffer will be binded to the 0s VertexInputBinding.
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

    if (hasIndexBuffer)
    {
        // Команда создания привязки буфера индексов (если он есть) к пайплайну.
        // Тип индекса должен совпадать с типом данных в самом буфере и может выбираться
        // меньше для экономии памяти при использовании простых моделей объектов.
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
    }
}

// Returning binding descriptions for the vertex buffer
std::vector<VkVertexInputBindingDescription> WrpModel::Vertex::getBindingDescriptions()
{
    // there are only one binding in the vector, cause for now all of the vertex data is packed into one array
    std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
    bindingDescriptions[0].binding = 0;										// this bindings' index
    bindingDescriptions[0].stride = sizeof(Vertex);
    bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;         // load data per vertex
    return bindingDescriptions;
}

// Returning attribute descriptions for the vertex buffer
std::vector<VkVertexInputAttributeDescription> WrpModel::Vertex::getAttributeDescriptions()
{
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
    
    // position attribute
    VkVertexInputAttributeDescription attribDescription{};
    attribDescription.location = 0;								// input data location index for this attribute in a vertex shader
    attribDescription.binding = 0;								// from which binding this attribute will be taken
    attribDescription.format = VK_FORMAT_R32G32B32_SFLOAT;		// attribute data type 
    attribDescription.offset = offsetof(Vertex, position);		// offset from the start of per-vertex data to this attribute 
    attributeDescriptions.push_back(attribDescription);

    // initialization of the rest attributes
    attributeDescriptions.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)});
    attributeDescriptions.push_back({2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)});
    attributeDescriptions.push_back({3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)});

    return attributeDescriptions;
}

ENGINE_END
