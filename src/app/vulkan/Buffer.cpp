#include "Buffer.hpp"
#include "vk_helper.hpp"
#include "logger.hpp"

Buffer::Buffer():
	buffer(VK_NULL_HANDLE),
	memory(VK_NULL_HANDLE),
	m_device(VK_NULL_HANDLE),
	m_size(0),
	m_mapped_memory(nullptr)
{
}

Buffer::Buffer(
	VkDevice device,
	VkPhysicalDevice physical_device,
	const CreateInfo & create_info
):
	m_device(device),
	m_size(create_info.size),
	m_mapped_memory(nullptr)
{
	VkBufferCreateInfo buffer_info = {};
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.size = create_info.size;
	buffer_info.usage = create_info.usage;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CHECK(
		vkCreateBuffer(device, &buffer_info, nullptr, &buffer),
		"Failed to create buffer"
	);

	VkMemoryRequirements mem_requirements;
	vkGetBufferMemoryRequirements(device, buffer, &mem_requirements);

	VkMemoryAllocateFlagsInfo memory_allocate_flags_info = {};
	memory_allocate_flags_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;

	if (create_info.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
	{
		memory_allocate_flags_info.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
	}

	VkMemoryAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_requirements.size;
	alloc_info.memoryTypeIndex = vk_helper::findMemoryType(physical_device, mem_requirements.memoryTypeBits, create_info.memory_properties);
	alloc_info.pNext = &memory_allocate_flags_info;

	VulkanMemoryAllocator & vma = VulkanMemoryAllocator::getInstance();
	VK_CHECK(
		vma.allocateMemory(device, &alloc_info, nullptr, &memory),
		"Failed to allocate buffer memory"
	);

	vkBindBufferMemory(device, buffer, memory, 0);

	if (create_info.memory_properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		VK_CHECK(
			vkMapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, &m_mapped_memory),
			"Failed to map buffer memory"
		);
	}
}

Buffer::~Buffer()
{
	clear();
}

Buffer::Buffer(Buffer && other) noexcept:
	buffer(other.buffer),
	memory(other.memory),
	m_device(other.m_device),
	m_size(other.m_size),
	m_mapped_memory(other.m_mapped_memory)
{
	other.buffer = VK_NULL_HANDLE;
	other.memory = VK_NULL_HANDLE;
	other.m_device = VK_NULL_HANDLE;
	other.m_size = 0;
	other.m_mapped_memory = nullptr;
}

Buffer & Buffer::operator=(Buffer && other) noexcept
{
	if (this != &other)
	{
		clear();

		buffer = other.buffer;
		memory = other.memory;
		m_device = other.m_device;
		m_size = other.m_size;
		m_mapped_memory = other.m_mapped_memory;

		other.buffer = VK_NULL_HANDLE;
		other.memory = VK_NULL_HANDLE;
		other.m_device = VK_NULL_HANDLE;
		other.m_size = 0;
		other.m_mapped_memory = nullptr;
	}

	return *this;
}

void Buffer::clear()
{
	if (m_device != VK_NULL_HANDLE)
	{
		if (buffer != VK_NULL_HANDLE)
		{
			vkDestroyBuffer(m_device, buffer, nullptr);
			buffer = VK_NULL_HANDLE;
		}
		if (memory != VK_NULL_HANDLE)
		{
			VulkanMemoryAllocator & vma = VulkanMemoryAllocator::getInstance();
			vma.freeMemory(m_device, memory, nullptr);
			memory = VK_NULL_HANDLE;
		}
	}
}

void * Buffer::mappedMemory()
{
	return m_mapped_memory;
}
