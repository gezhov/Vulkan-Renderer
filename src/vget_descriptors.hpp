#pragma once

#include "vget_device.hpp"

// std
#include <memory>
#include <unordered_map>
#include <vector>

namespace vget
{
    // Класс обёртка над VkDescriptorSetLayout для удобного управления им
    class VgetDescriptorSetLayout
    {
    public:
        // Класс для удобного создания информации о привязках в Descriptor Set'е
        class Builder
        {
        public:
            Builder(VgetDevice& vgetDevice) : vgetDevice{vgetDevice} {}

            // Добавление новой привязки дескрпитора в мапу
            Builder& addBinding(
                uint32_t binding,
                VkDescriptorType descriptorType,
                VkShaderStageFlags stageFlags,
                uint32_t count = 1);
            // Создание экземпляра VgetDescriptorSetLayout на основе текущей мапы привязок
            std::unique_ptr<VgetDescriptorSetLayout> build() const;

        private:
            VgetDevice& vgetDevice;
            // Мапа с информацией по каждой привязке. На основе этой мапы строится VgetDescriptorSetLayout
            std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings{};
        };

        VgetDescriptorSetLayout(VgetDevice& vgetDevice, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings);
        ~VgetDescriptorSetLayout();
        VgetDescriptorSetLayout(const VgetDescriptorSetLayout&) = delete;
        VgetDescriptorSetLayout& operator=(const VgetDescriptorSetLayout&) = delete;

        VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }

    private:
        VgetDevice& vgetDevice;
        VkDescriptorSetLayout descriptorSetLayout;
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;

        friend class VgetDescriptorWriter;
    };

    // Класс обёртка над VkDescriptorPool для удобного управления им
    class VgetDescriptorPool
    {
    public:
        // Удобный класс-строитель для настройки и создания экземпляра VgetDescriptorPool
        class Builder
        {
        public:
            Builder(VgetDevice& vgetDevice) : vgetDevice{vgetDevice} {}

            Builder& addPoolSize(VkDescriptorType descriptorType, uint32_t count); // кол-во дескрипторов заданного типа в данном пуле
            Builder& setPoolFlags(VkDescriptorPoolCreateFlags flags);		// флаги настройки поведения пула
            Builder& setMaxSets(uint32_t count);							// макс. число наборов, которые можно выделить из пула дескрипторов
            // Создание экземпляра VgetDescriptorPool
            std::unique_ptr<VgetDescriptorPool> build() const;

        private:
            VgetDevice& vgetDevice;
            std::vector<VkDescriptorPoolSize> poolSizes{};
            uint32_t maxSets = 1000;
            VkDescriptorPoolCreateFlags poolFlags = 0;
        };

        VgetDescriptorPool(
            VgetDevice& vgetDevice,
            uint32_t maxSets,
            VkDescriptorPoolCreateFlags poolFlags,
            const std::vector<VkDescriptorPoolSize>& poolSizes);
        ~VgetDescriptorPool();
        VgetDescriptorPool(const VgetDescriptorPool&) = delete;
        VgetDescriptorPool& operator=(const VgetDescriptorPool&) = delete;

        // Выделение набора дескрипторов из пула
        bool allocateDescriptorSet(const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor) const;

        // Освобождение дескрипторов из пула
        void freeDescriptors(std::vector<VkDescriptorSet>& descriptors) const;

        // Сброс всего пула и всех дескрипторов, которые были из него выделены
        void resetPool();

    private:
        VgetDevice& vgetDevice;
        VkDescriptorPool descriptorPool;

        friend class VgetDescriptorWriter;
    };

    // Класс для конфигурирования и создания наборов дескрипторов. Он выделяет набор
    // дескрипторов из пула и записывает необходимую информацию для дескрипторов набора.
    class VgetDescriptorWriter
    {
    public:
        VgetDescriptorWriter(VgetDescriptorSetLayout& setLayout, VgetDescriptorPool& pool);

        // Готовит запись для информации о буфере дескриптора
        VgetDescriptorWriter& writeBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo);
        // Готовт запись для информации о ресурсе-изображении дескрипторов
        VgetDescriptorWriter& writeImage(uint32_t binding, VkDescriptorImageInfo* imageInfo, uint32_t count = 1);

        // Выделяет набор из пула в переданный VkDescriptorSet
        // и конфигурирует его VkWriteDescriptorSet записями
        bool build(VkDescriptorSet& set);
        void overwrite(VkDescriptorSet& set);

    private:
        VgetDescriptorSetLayout& setLayout;
        VgetDescriptorPool& pool;
        std::vector<VkWriteDescriptorSet> writes; // структуры-записи для обновления информации о ресурсах дескрипторов
    };
}
