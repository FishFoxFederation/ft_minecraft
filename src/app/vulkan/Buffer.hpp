#pragma once

#include "vk_define.hpp"
#include "VulkanMemoryAllocator.hpp"

#include <vulkan/vulkan.h>

#include <stdexcept>

class Buffer
{

public:

	struct CreateInfo
	{
		VkDeviceSize size;
		VkBufferUsageFlags usage;
		VkMemoryPropertyFlags memory_properties;
	};

	Buffer();
	Buffer(
		VkDevice device,
		VkPhysicalDevice physical_device,
		const CreateInfo & create_info
	);
	~Buffer();

	Buffer(const Buffer &) = delete;
	Buffer(Buffer && other) noexcept;
	Buffer & operator=(const Buffer &) = delete;
	Buffer & operator=(Buffer && other) noexcept;

	void clear();

	VkDeviceSize size() const { return m_size; }

	void * mappedMemory();

	VkBuffer buffer;
	VkDeviceMemory memory;

private:

	VkDevice m_device;

	VkDeviceSize m_size;

	void * m_mapped_memory;
};
