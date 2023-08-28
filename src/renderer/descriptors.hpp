#pragma once

#include "device.hpp"

// std
#include <memory>
#include <unordered_map>
#include <vector>

ENGINE_BEGIN

// Класс обёртка над VkDescriptorSetLayout для удобного управления им
class WrpDescriptorSetLayout
{
public:
    // Класс для удобного создания информации о привязках в Descriptor Set'е
    class Builder
    {
    public:
        Builder(WrpDevice& wrpDevice) : wrpDevice{wrpDevice} {}

        // Добавление новой привязки дескриптора в мапу
        Builder& addBinding(
            uint32_t binding,
            VkDescriptorType descriptorType,
            VkShaderStageFlags stageFlags,
            uint32_t count = 1);
        // Создание экземпляра WrpDescriptorSetLayout на основе текущей мапы привязок
        std::unique_ptr<WrpDescriptorSetLayout> build() const;

    private:
        WrpDevice& wrpDevice;
        // Мапа с информацией по каждой привязке. На основе этой мапы строится WrpDescriptorSetLayout
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings{};
    };

    WrpDescriptorSetLayout(WrpDevice& wrpDevice, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings);
    ~WrpDescriptorSetLayout();
    WrpDescriptorSetLayout(const WrpDescriptorSetLayout&) = delete;
    WrpDescriptorSetLayout& operator=(const WrpDescriptorSetLayout&) = delete;

    VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }

private:
    WrpDevice& wrpDevice;
    VkDescriptorSetLayout descriptorSetLayout;
    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;

    friend class WrpDescriptorWriter;
};

// Класс обёртка над VkDescriptorPool для удобного управления им
class WrpDescriptorPool
{
public:
    // Удобный класс-строитель для настройки и создания экземпляра WrpDescriptorPool
    class Builder
    {
    public:
        Builder(WrpDevice& wrpDevice) : wrpDevice{wrpDevice} {}

        Builder& addPoolSize(VkDescriptorType descriptorType, uint32_t count); // кол-во дескрипторов заданного типа в данном пуле
        Builder& setPoolFlags(VkDescriptorPoolCreateFlags flags);		// флаги настройки поведения пула
        Builder& setMaxSets(uint32_t count);							// макс. число наборов, которые можно выделить из пула дескрипторов
        // Создание экземпляра WrpDescriptorPool
        std::unique_ptr<WrpDescriptorPool> build() const;

    private:
        WrpDevice& wrpDevice;
        std::vector<VkDescriptorPoolSize> poolSizes{};
        uint32_t maxSets = 1000;
        VkDescriptorPoolCreateFlags poolFlags = 0;
    };

    WrpDescriptorPool(
        WrpDevice& wrpDevice,
        uint32_t maxSets,
        VkDescriptorPoolCreateFlags poolFlags,
        const std::vector<VkDescriptorPoolSize>& poolSizes);
    ~WrpDescriptorPool();
    WrpDescriptorPool(const WrpDescriptorPool&) = delete;
    WrpDescriptorPool& operator=(const WrpDescriptorPool&) = delete;

    // Выделение набора дескрипторов из пула
    bool allocateDescriptorSet(const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor) const;

    // Освобождение дескрипторов из пула
    void freeDescriptors(std::vector<VkDescriptorSet>& descriptors) const;

    // Сброс всего пула и всех дескрипторов, которые были из него выделены
    void resetPool();

private:
    WrpDevice& wrpDevice;
    VkDescriptorPool descriptorPool;

    friend class WrpDescriptorWriter;
};

// Класс для конфигурирования и создания наборов дескрипторов. Он выделяет набор
// дескрипторов из пула и записывает необходимую информацию для дескрипторов набора.
class WrpDescriptorWriter
{
public:
    WrpDescriptorWriter(WrpDescriptorSetLayout& setLayout, WrpDescriptorPool& pool);

    // Готовит запись для информации о буфере дескриптора
    WrpDescriptorWriter& writeBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo);
    // Готовт запись для информации о ресурсе-изображении дескрипторов
    WrpDescriptorWriter& writeImage(uint32_t binding, VkDescriptorImageInfo* imageInfo, uint32_t count = 1);

    // Выделяет набор из пула в переданный VkDescriptorSet
    // и конфигурирует его VkWriteDescriptorSet записями
    bool build(VkDescriptorSet& set);
    void overwrite(VkDescriptorSet& set);

private:
    WrpDescriptorSetLayout& setLayout;
    WrpDescriptorPool& pool;
    std::vector<VkWriteDescriptorSet> writes; // структуры-записи для обновления информации о ресурсах дескрипторов
};

ENGINE_END
