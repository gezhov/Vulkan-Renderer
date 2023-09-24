#pragma once

#include "Device.hpp"
#include "Buffer.hpp"
#include "Texture.hpp"

// libs
#define GLM_FORCE_RADIANS			  // Функции GLM будут работать с радианами, а не градусами
#define GLM_FORCE_DEPTH_ZERO_TO_ONE   // GLM будет ожидать интервал нашего буфера глубины от 0 до 1 (например, для OpenGL используется интервал от -1 до 1)
#include <glm/glm.hpp>
#include <tiny_obj_loader.h>

// std
#include <memory>
#include <vector>
#include <unordered_map>

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

    // вспомогательная структура для распределния данных загруженной модели 
    struct Builder
    {
        // структура, описывающая место появления нового подобъекта из .obj модели и индекс его текстуры
        struct SubMesh
        {
            uint32_t indexStart;
            uint32_t indexCount;
            int diffuseTextureIndex;
            glm::vec3 diffuseColor;
        };

        std::vector<Vertex> vertices{};
        std::vector<uint32_t> indices{};
        std::vector<std::string> texturePaths{};
        std::vector<SubMesh> subMeshesInfos{};

        void loadModel(const std::string& filepath);
        SubMesh createSubMesh(uint32_t indexStart, uint32_t indexCount, int materialId,
            std::unordered_map<std::string, int>& difTexPathsMap, std::vector<tinyobj::material_t>& materials);
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
    void draw(VkCommandBuffer commandBuffer);
    void drawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t indexStart = 0);

    std::vector<Builder::SubMesh>& getSubMeshesInfos() {return subMeshesInfos;}
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

    std::vector<Builder::SubMesh> subMeshesInfos;
    std::vector<std::unique_ptr<WrpTexture>> textures;
};

ENGINE_END
