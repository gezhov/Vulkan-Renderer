#pragma once

#include "vget_window.hpp"
#include "vget_device.hpp"
#include "vget_game_object.hpp"
#include "vget_renderer.hpp"
#include "vget_descriptors.hpp"

// std
#include <memory>
#include <vector>

namespace vget
{
	class FirstApp
	{
	public:
		static constexpr int WIDTH = 1280;
		static constexpr int HEIGHT = 960;

		FirstApp();
		~FirstApp();

		// Избавляемся от copy operator и copy constrcutor, т.к. FirstApp хранит в себе указатели
		// на VkPipelineLayout_T и VkCommandBuffer_T, которые лучше не копировать.
		FirstApp(const FirstApp&) = delete;
		FirstApp& operator=(const FirstApp&) = delete;

		void run();

	private:
		void loadGameObjects();

		// Порядок объявления перменных-членов имеет значение. Так, они будут инициализироваться
		// сверху вниз, а уничтожаться снизу вверх. Пул дескрипторов, таким образом, должен
		// быть объявлен после девайса.
		VgetWindow vgetWindow{ WIDTH, HEIGHT, "VgetX Engine" };
		VgetDevice vgetDevice{ vgetWindow };
		VgetRenderer vgetRenderer{ vgetWindow, vgetDevice };

		std::unique_ptr<VgetDescriptorPool> globalPool{};
		VgetGameObject::Map gameObjects;
	};
}