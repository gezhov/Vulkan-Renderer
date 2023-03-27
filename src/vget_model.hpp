#pragma once

#include "vget_device.hpp"
#include "vget_buffer.hpp"
#include "vget_texture.hpp"

// libs
#define GLM_FORCE_RADIANS			  // Функции GLM будут работать с радианами, а не градусами
#define GLM_FORCE_DEPTH_ZERO_TO_ONE   // GLM будет ожидать интервал нашего буфера глубины от 0 до 1 (например, для OpenGL используется интервал от -1 до 1)
#include <glm/glm.hpp>

// std
#include <memory>
#include <vector>

namespace vget
{
    class VgetModel
    {
    public:
        struct Vertex
        {
            glm::vec3 position;
            glm::vec3 color;
            glm::vec3 normal;
            glm::vec2 uv;		// координаты текстуры

            static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
            static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

            bool operator==(const Vertex& other) const
            {
                return position == other.position && color == other.color &&
                    normal == other.normal && uv == other.uv;
            }
        };

        // вспомогательная структура, которая хранит в себе буферы вершин и индексов
        struct Builder
        {
            // структура, описывающая место появления нового подобъекта из .obj модели и индекс его текстуры
            struct SubObjectInfo
            {
                uint32_t indexCount;
                uint32_t indexStart;
                int textureIndex;
                glm::vec3 diffuseColor;
            };

            std::vector<Vertex> vertices{};
            std::vector<uint32_t> indices{};
            std::vector<std::string> texturePaths{};
            std::vector<SubObjectInfo> subObjectsInfo{};

            void loadModel(const std::string& filepath);
        };

        VgetModel(VgetDevice& device, const VgetModel::Builder& builder);
        ~VgetModel();

        // Избавляемся от copy operator и copy constrcutor, т.к. VgetModel хранит
        // указатели на буфер вершин и его память.
        VgetModel(const VgetModel&) = delete;
        VgetModel& operator=(const VgetModel&) = delete;

        static std::unique_ptr<VgetModel> createModelFromFile(VgetDevice& device, const std::string& filepath);

        void bind(VkCommandBuffer commandBuffer);
        // todo подумать как можно объединить draw и drawIndexed
        void draw(VkCommandBuffer commandBuffer);
        void drawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t indexStart = 0);

        std::vector<Builder::SubObjectInfo>& getSubObjectsInfo() {return subObjectsInfo;}
        std::vector<std::unique_ptr<VgetTexture>>& getTextures() {return textures;}

        bool hasTextures = false;

    private:
        void createVertexBuffers(const std::vector<Vertex>& vertices);
        void createIndexBuffers(const std::vector<uint32_t>& indices);
        void createTextures(const std::vector<std::string>& texturePaths);

        VgetDevice& vgetDevice;

        std::unique_ptr<VgetBuffer> vertexBuffer;
        uint32_t vertexCount;

        bool hasIndexBuffer = false;
        std::unique_ptr<VgetBuffer> indexBuffer;
        uint32_t indexCount;

        std::vector<Builder::SubObjectInfo> subObjectsInfo;
        std::vector<std::unique_ptr<VgetTexture>> textures;
    };
}
