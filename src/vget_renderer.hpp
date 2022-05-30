#pragma once

#include "vget_window.hpp"
#include "vget_swap_chain.hpp"
#include "vget_device.hpp"
#include "vget_imgui.hpp"

// std
#include <cassert>
#include <memory>
#include <vector>

namespace vget
{
	class VgetRenderer
	{
	public:

		VgetRenderer(VgetWindow& window, VgetDevice& device);
		~VgetRenderer();

		// Избавляемся от copy operator и copy constructor, т.к. VgetRenderer хранит в себе указатели
		// на VkCommandBuffer_T, которые лучше не копировать.
		VgetRenderer(const VgetRenderer&) = delete;
		VgetRenderer& operator=(const VgetRenderer&) = delete;

		VkRenderPass getSwapChainRenderPass() const { return vgetSwapChain->getRenderPass(); }
		float getAspectRatio() const {return vgetSwapChain->extentAspectRatio();}
		bool isFrameInProgress() const { return isFrameStarted; }

		VkCommandBuffer getCurrentCommandBuffer() const
		{
			assert(isFrameStarted && "Cannot get command buffer when frame not in progress");
			return commandBuffers[currentFrameIndex];
		}

		int getFrameIndex() const
		{
			assert(isFrameStarted && "Cannot get frame index when frame not in progress");
			return currentFrameIndex;
		}

		VkCommandBuffer beginFrame();
		void endFrame();
		void beginSwapChainRenderPass(VkCommandBuffer commandBuffer, ImVec4 clearColors);
		void endSwapChainRenderPass(VkCommandBuffer commandBuffer);

	private:
		void createCommandBuffers();
		void freeCommandBuffers();
		void recreateSwapChain();

		VgetWindow& vgetWindow;
		VgetDevice& vgetDevice;
		std::unique_ptr<VgetSwapChain> vgetSwapChain;
		std::vector<VkCommandBuffer> commandBuffers;

		uint32_t currentImageIndex;
		int currentFrameIndex{ 0 };           // [0, Max_Frames_In_Flight]
		bool isFrameStarted{ false };
	};
}