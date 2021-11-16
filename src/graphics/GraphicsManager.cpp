#include "graphics/GraphicsManager.hpp"

#include "Engine.hpp"

namespace NovaEngine::Graphics
{
	const std::vector<Vertex> vertices = {
		{ { 0.0f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
		{ { 0.5f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
		{ { -0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f } }
	};

	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;

	void GraphicsManager::onFrameBufferResizedHandler(GLFWwindow* window, int width, int height)
	{
		GraphicsManager* m = static_cast<GraphicsManager*>(glfwGetWindowUserPointer(window));

		if (m != nullptr)
			m->didResize = true;
	}

	bool GraphicsManager::onInitialize(GLFWwindow* window)
	{
		try
		{
			window_ = window;
			instance_ = VkFactory::createInstance();
			surface_ = VkFactory::createSurface(instance_, window_);
			physicalDevice_ = VkFactory::pickPhysicalDevice(instance_, surface_);
			device_ = VkFactory::createDevice(physicalDevice_, surface_);

			initSwapChain();

			syncObjects_ = VkFactory::createSyncObjects(device_, swapChain_, 2);

			glfwSetWindowUserPointer(window_, this);
			glfwSetFramebufferSizeCallback(window_, onFrameBufferResizedHandler);
		}
		catch (const std::runtime_error& err)
		{
			Logger::get()->error(err.what());
		}

		vertexBuffer = VkFactory::createVertexBuffer(device_, sizeof(vertices[0]) * vertices.size());


		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(*device_, vertexBuffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = VkUtils::findMemoryType(*physicalDevice_, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		if (vkAllocateMemory(*device_, &allocInfo, nullptr, &vertexBufferMemory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate vertex buffer memory!");
		}

		vkBindBufferMemory(*device_, vertexBuffer, vertexBufferMemory, 0);

		void* data;
		vkMapMemory(*device_, vertexBufferMemory, 0, sizeof(vertices[0]) * vertices.size(), 0, &data);
		memcpy(data, vertices.data(), (size_t)sizeof(vertices[0]) * vertices.size());
		vkUnmapMemory(*device_, vertexBufferMemory);

		vkDeviceWaitIdle(*device_);

		return true;
	}

	bool GraphicsManager::onTerminate()
	{
		vkDeviceWaitIdle(*device_);
		syncObjects_.destroy();
		vkFreeMemory(*device_, vertexBufferMemory, nullptr);
		vkDestroyBuffer(*device_, vertexBuffer, nullptr);
		destroySwapChain();
		device_.destroy();
		physicalDevice_.destroy();
		surface_.destroy();
		instance_.destroy();

		return true;
	}

	void GraphicsManager::initSwapChain(bool recreate)
	{
		if (isInitialized() && recreate)
		{
			vkDeviceWaitIdle(*device_);
			Vk::SwapChain newSwapChain = VkFactory::createSwapChain(physicalDevice_, device_, surface_, window_, &swapChain_);

			commandPool_.destroy();
			swapChain_.destroyFrameBuffers();
			renderPass_.destroy();
			swapChain_.destroy();

			swapChain_ = newSwapChain;

			renderPass_ = VkFactory::createRenderPass(device_, swapChain_);
			swapChain_.createFrameBuffers(renderPass_);
			commandPool_ = VkFactory::createCommandPool(physicalDevice_, device_, surface_);
			commandBuffers_ = VkFactory::createCommandBuffers(device_, swapChain_, commandPool_);
		}
		else
		{
			swapChain_ = VkFactory::createSwapChain(physicalDevice_, device_, surface_, window_, nullptr);
			renderPass_ = VkFactory::createRenderPass(device_, swapChain_);
			swapChain_.createFrameBuffers(renderPass_);
			pipeline_ = VkFactory::createPipline(device_, swapChain_, renderPass_);
			commandPool_ = VkFactory::createCommandPool(physicalDevice_, device_, surface_);
			commandBuffers_ = VkFactory::createCommandBuffers(device_, swapChain_, commandPool_);
		}
	}

	void GraphicsManager::destroySwapChain()
	{
		commandPool_.destroy();
		swapChain_.destroyFrameBuffers();
		pipeline_.destroy();
		renderPass_.destroy();
		swapChain_.destroy();
	}


	void GraphicsManager::recordCommands(size_t index)
	{
		VkCommandBuffer buf = commandBuffers_.buffers[index];

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		if (vkBeginCommandBuffer(buf, &beginInfo) != VK_SUCCESS)
			throw std::runtime_error("failed to begin recording command buffer!");

		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = *renderPass_;
		renderPassInfo.framebuffer = swapChain_.frameBuffers[index];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = swapChain_.extent;

		VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline_);

		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)swapChain_.extent.width;
		viewport.height = (float)swapChain_.extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		vkCmdSetViewport(buf, 0, 1, &viewport);

		VkRect2D siccors = {};
		siccors.offset = { 0, 0 };
		siccors.extent = swapChain_.extent;
		vkCmdSetScissor(buf, 0, 1, &siccors);

		VkBuffer vertexBuffers[] = { vertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(buf, 0, 1, vertexBuffers, offsets);

		vkCmdDraw(buf, 3, 1, 0, 0);

		vkCmdEndRenderPass(buf);

		if (vkEndCommandBuffer(buf) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to record command buffer!");
		}
	}

	void GraphicsManager::draw()
	{
		static uint32_t currentFrame = 0;

		vkWaitForFences(*device_, 1, &syncObjects_.inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(*device_, *swapChain_, UINT64_MAX, syncObjects_.imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			initSwapChain(true);
			draw();
			return;
		}
		else if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to aquire next image from swapchain!");
		}

		if (syncObjects_.imagesInFlight[imageIndex] != VK_NULL_HANDLE)
		{
			vkWaitForFences(*device_, 1, &syncObjects_.imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
		}

		syncObjects_.imagesInFlight[imageIndex] = syncObjects_.inFlightFences[currentFrame];

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { syncObjects_.imageAvailableSemaphores[currentFrame] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers_.buffers[imageIndex];

		VkSemaphore signalSemaphores[] = { syncObjects_.renderFinishedSemaphores[currentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		vkResetFences(*device_, 1, &syncObjects_.inFlightFences[currentFrame]);

		recordCommands(imageIndex);

		if (vkQueueSubmit(device_.graphicsQueue, 1, &submitInfo, syncObjects_.inFlightFences[currentFrame]) != VK_SUCCESS)
			throw std::runtime_error("failed to submit draw command buffer!");

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { *swapChain_ };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;

		presentInfo.pImageIndices = &imageIndex;

		result = vkQueuePresentKHR(device_.presentationQueue, &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || didResize)
		{
			// Logger::get()->info("suboptimal acquire!");
			initSwapChain(true);
			didResize = false;
			draw();
		}
		else if (result != VK_SUCCESS)
			throw std::runtime_error("failed to present swap chain image!");

		currentFrame = (currentFrame + 1) % syncObjects_.maxFramesInFlight;
	}
};
