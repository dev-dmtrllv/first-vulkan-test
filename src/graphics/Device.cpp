#include "graphics/Device.hpp"
#include "graphics/Context.hpp"
#include "framework.hpp"

namespace NovaEngine::Graphics
{
	DeviceConfig Device::defaultConfig = {
		.queueCount = 1
	};

	bool Device::onInitialize(DeviceConfig* config)
	{
		if (config == nullptr)
			config = &defaultConfig;

		graphicsQueues_.resize(config->queueCount);

		PhysicalDevice& pDev = context()->physicalDevice();
		const QueueFamilies& queueFamilies = pDev.queueFamilies();
		uint32_t graphicsQueueIndex = queueFamilies.graphicsFamily.value();
		uint32_t presentQueueIndex = queueFamilies.presentFamily.value();
		float queuePriority = 1.0f;

		VkPhysicalDeviceFeatures deviceFeatures = {};

		VkDeviceQueueCreateInfo createInfos[] = {
			/*graphicsQueueCreateInfo*/
			{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex = graphicsQueueIndex,
				.queueCount = config->queueCount,
				.pQueuePriorities = &queuePriority,
			},
			/*presentQueueCreateInfo*/
			{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex = presentQueueIndex,
				.queueCount = 1,
				.pQueuePriorities = &queuePriority
			},

		};

		const char* extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

		VkDeviceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pQueueCreateInfos = createInfos;
		createInfo.queueCreateInfoCount = 2;
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = 1;
		createInfo.ppEnabledExtensionNames = &extensions;


		if (vkCreateDevice(*pDev, &createInfo, nullptr, &device_) == VK_SUCCESS)
		{
			for (size_t i = 0; i < config->queueCount; i++)
				vkGetDeviceQueue(device_, graphicsQueueIndex, i, &graphicsQueues_[i]);

			vkGetDeviceQueue(device_, presentQueueIndex, 0, &presentQueue_);
			
			return true;
		}

		return false;
	}

	bool Device::onTerminate()
	{
		vkDestroyDevice(device_, nullptr);
		return true;
	}

	const std::vector<VkQueue>& Device::graphicsQueues()
	{
		return graphicsQueues_;
	}

	VkQueue Device::getGraphicsQueue(uint32_t index)
	{
		assert(index < graphicsQueues_.size());
		return graphicsQueues_[index];
	}
};