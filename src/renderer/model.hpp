#pragma once

#include "device.hpp"
#include "buffer.hpp"
#include "texture.hpp"

// libs
#define GLM_FORCE_RADIANS			  // Функции GLM будут работать с радианами, а не градусами
#define GLM_FORCE_DEPTH_ZERO_TO_ONE   // GLM будет ожидать интервал нашего буфера глубины от 0 до 1 (например, для OpenGL используется интервал от -1 до 1)
#include <glm/glm.hpp>

// std
#include <memory>
#include <vector>

ENGINE_BEGIN

class WrpModel
{
public:
    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 color;
        glm::vec3 normal;
        glm::vec2 uv;

        static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
        static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

        bool operator==(const Vertex& other) const
        {
            return position == other.position && color == other.color &&
                normal == other.normal && uv == other.uv;
        }
    };

    // вспомогательная структура, которая хранит в себе буферы вершин и индексов
    struct Builder // rename ModelBuilder ??
    {
        // структура, описывающая место появления нового подобъекта из .obj модели и индекс его текстуры
        struct SubObjectInfo // todo: rename shapeInfo ?
        {
            uint32_t indexCount;
            uint32_t indexStart;
            int textureIndex;
            glm::vec3 diffuseColor;
        };

        std::vector<Vertex> vertices{};
        std::vector<uint32_t> indices{};
        std::vector<std::string> texturePaths{};
        std::vector<SubObjectInfo> subObjectsInfos{};

        void loadModel(const std::string& filepath);
    };

    WrpModel(WrpDevice& device, const WrpModel::Builder& builder);
    ~WrpModel();

    // Избавляемся от copy operator и copy constrcutor, т.к. WrpModel хранит
    // указатели на буфер вершин и его память.
    WrpModel(const WrpModel&) = delete;
    WrpModel& operator=(const WrpModel&) = delete;

    static std::unique_ptr<WrpModel> createModelFromObjMtl(WrpDevice& device, const std::string& filepath);
    static std::unique_ptr<WrpModel> WrpModel::createModelFromObjTexture
        (WrpDevice& device, const std::string& modelPath, const std::string& texturePath);

    void bind(VkCommandBuffer commandBuffer);
    // todo подумать как можно объединить draw и drawIndexed
    void draw(VkCommandBuffer commandBuffer);
    void drawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t indexStart = 0);

    std::vector<Builder::SubObjectInfo>& getSubObjectsInfos() {return subObjectsInfos;}
    std::vector<std::unique_ptr<WrpTexture>>& getTextures() {return textures;}

    bool hasTextures = false;

private:
    void createVertexBuffers(const std::vector<Vertex>& vertices);
    void createIndexBuffers(const std::vector<uint32_t>& indices);
    void createTextures(const std::vector<std::string>& texturePaths);

    WrpDevice& wrpDevice;

    std::unique_ptr<WrpBuffer> vertexBuffer;
    uint32_t vertexCount;

    bool hasIndexBuffer = false;
    std::unique_ptr<WrpBuffer> indexBuffer;
    uint32_t indexCount;

    std::vector<Builder::SubObjectInfo> subObjectsInfos;
    std::vector<std::unique_ptr<WrpTexture>> textures;
};

ENGINE_END
