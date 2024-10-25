#include "Image.hpp"

#include "vk_helper.hpp"
#include "logger.hpp"
#include "Buffer.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <fstream>
#include <optional>
#include <cassert>

Image::Image():
	image(VK_NULL_HANDLE),
	memory(VK_NULL_HANDLE),
	view(VK_NULL_HANDLE),
	sampler(VK_NULL_HANDLE),
	m_device(VK_NULL_HANDLE)
{
}

Image::Image(Image && other) noexcept:
	image(other.image),
	memory(other.memory),
	view(other.view),
	sampler(other.sampler),
	format(other.format),
	extent2D(other.extent2D),
	extent3D(other.extent3D),
	aspect_mask(other.aspect_mask),
	mip_levels(other.mip_levels),
	array_layers(other.array_layers),
	m_device(other.m_device)
{
	other.image = VK_NULL_HANDLE;
	other.memory = VK_NULL_HANDLE;
	other.view = VK_NULL_HANDLE;
	other.sampler = VK_NULL_HANDLE;
	other.m_device = VK_NULL_HANDLE;
}

Image & Image::operator=(Image && other) noexcept
{
	if (this != &other)
	{
		clear();

		image = other.image;
		memory = other.memory;
		view = other.view;
		sampler = other.sampler;
		format = other.format;
		extent2D = other.extent2D;
		extent3D = other.extent3D;
		aspect_mask = other.aspect_mask;
		mip_levels = other.mip_levels;
		array_layers = other.array_layers;
		m_device = other.m_device;

		other.image = VK_NULL_HANDLE;
		other.memory = VK_NULL_HANDLE;
		other.view = VK_NULL_HANDLE;
		other.sampler = VK_NULL_HANDLE;
		other.m_device = VK_NULL_HANDLE;
	}

	return *this;
}

Image::~Image()
{
	clear();
}

Image::Image(
	VkDevice device,
	VkPhysicalDevice physical_device,
	SingleTimeCommand & command_buffer,
	const CreateInfo & create_info
):
	image(VK_NULL_HANDLE),
	memory(VK_NULL_HANDLE),
	view(VK_NULL_HANDLE),
	sampler(VK_NULL_HANDLE),
	m_device(device),
	m_physical_device(physical_device)
{

	//############################################################################################################
	//                     																                         #
	//                                      Read images from files (if any)                                      #
	//                     																                         #
	//############################################################################################################

	uint32_t files_count = static_cast<uint32_t>(create_info.file_paths.size());
	std::vector<Image> staging_images;
	std::vector<Buffer> staging_buffers;

	VkExtent2D images_extent = {0, 0};
	bool all_images_same_size = create_info.file_paths.size() > 0;

	for (const auto & file_path : create_info.file_paths)
	{
		int tex_width, tex_height, tex_channels;
		stbi_uc * pixel = stbi_load(
			file_path.c_str(),
			&tex_width,
			&tex_height,
			&tex_channels,
			STBI_rgb_alpha
		);
		VkDeviceSize image_size = tex_width * tex_height * 4;

		if (!pixel)
		{
			throw std::runtime_error("Failed to load texture image '" + file_path + "'.");
		}

		if (images_extent.width == 0 && images_extent.height == 0)
		{
			images_extent.width = tex_width;
			images_extent.height = tex_height;
		}
		else if (images_extent.width != static_cast<uint32_t>(tex_width)
			|| images_extent.height != static_cast<uint32_t>(tex_height))
		{
			all_images_same_size = false;
		}


		Buffer::CreateInfo staging_buffer_create_info = {};
		staging_buffer_create_info.size = image_size;
		staging_buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		staging_buffer_create_info.memory_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		Buffer staging_buffer = Buffer(device, physical_device, staging_buffer_create_info);

		memcpy(staging_buffer.mappedMemory(), pixel, static_cast<size_t>(image_size));
		vkUnmapMemory(device, staging_buffer.memory);


		CreateInfo staging_image_create_info = {};
		staging_image_create_info.extent.width = tex_width;
		staging_image_create_info.extent.height = tex_height;
		staging_image_create_info.format = VK_FORMAT_R8G8B8A8_SRGB;
		staging_image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		staging_image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		staging_image_create_info.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		staging_image_create_info.final_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

		Image staging_image = Image(device, physical_device, command_buffer, staging_image_create_info);


		VkBufferImageCopy region = {};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { staging_image.extent2D.width, staging_image.extent2D.height, 1 };

		vkCmdCopyBufferToImage(
			command_buffer,
			staging_buffer.buffer,
			staging_image.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region
		);

		staging_image.transitionLayout(
			command_buffer,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
		);

		staging_images.emplace_back(std::move(staging_image));
		staging_buffers.emplace_back(std::move(staging_buffer));

		stbi_image_free(pixel);
	}

	//############################################################################################################
	//                     																                         #
	//                                               Create image                                                #
	//                     																                         #
	//############################################################################################################

	// If the extent is not specified, use the extent of the images
	if (create_info.extent.width == 0 && create_info.extent.height == 0)
	{
		if (all_images_same_size)
			extent2D = images_extent;
		else
			throw std::runtime_error("Image creation: If the extent is not specified, all images must have the same size.");
	}
	else
		extent2D = create_info.extent;
	extent3D = {
		extent2D.width,
		extent2D.height,
		1
	};
	format = create_info.format;
	aspect_mask = create_info.aspect_mask;
	mip_levels = create_info.mip_levels;
	array_layers = files_count > 0 ? files_count : create_info.array_layers;


	VkImageCreateInfo image_create_info = {};
	image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_create_info.imageType = VK_IMAGE_TYPE_2D;
	image_create_info.extent = extent3D;
	image_create_info.mipLevels = create_info.mip_levels;
	image_create_info.arrayLayers = array_layers;
	image_create_info.format = create_info.format;
	image_create_info.tiling = create_info.tiling;
	image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_create_info.usage = create_info.usage;
	image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_create_info.flags = create_info.is_cube_map ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

	VK_CHECK(
		vkCreateImage(m_device, &image_create_info, nullptr, &image),
		"Failed to create image"
	);

	VkMemoryRequirements memory_requirements;
	vkGetImageMemoryRequirements(m_device, image, &memory_requirements);

	VkMemoryAllocateInfo memory_allocate_info = {};
	memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memory_allocate_info.allocationSize = memory_requirements.size;
	memory_allocate_info.memoryTypeIndex = vk_helper::findMemoryType(
		physical_device,
		memory_requirements.memoryTypeBits,
		create_info.memory_properties
	);

	VulkanMemoryAllocator & vma = VulkanMemoryAllocator::getInstance();
	VK_CHECK(
		vma.allocateMemory(m_device, &memory_allocate_info, nullptr, &memory),
		"Failed to allocate memory for image"
	);

	VK_CHECK(
		vkBindImageMemory(m_device, image, memory, 0),
		"Failed to bind image memory"
	);

	if (files_count > 0)
	{
		//############################################################################################################
		//                     																                         #
		//                                            Copy data to image                                             #
		//                     																                         #
		//############################################################################################################

		transitionLayout(
			command_buffer,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
		);

		VkImageBlit image_blit = {};
		image_blit.srcOffsets[0] = { 0, 0, 0 };
		image_blit.srcSubresource.aspectMask = aspect_mask;
		image_blit.srcSubresource.mipLevel = 0;
		image_blit.srcSubresource.baseArrayLayer = 0;
		image_blit.srcSubresource.layerCount = 1;

		image_blit.dstOffsets[0] = { 0, 0, 0 };
		image_blit.dstOffsets[1] = { static_cast<int32_t>(extent2D.width), static_cast<int32_t>(extent2D.height), 1 };
		image_blit.dstSubresource.aspectMask = aspect_mask;
		image_blit.dstSubresource.mipLevel = 0;
		image_blit.dstSubresource.baseArrayLayer = 0;
		image_blit.dstSubresource.layerCount = 1;

		for (uint32_t i = 0; i < files_count; i++)
		{
			image_blit.srcOffsets[1] = {
				static_cast<int32_t>(staging_images[i].extent2D.width),
				static_cast<int32_t>(staging_images[i].extent2D.height),
				1
			};
			image_blit.dstSubresource.baseArrayLayer = i;

			vkCmdBlitImage(
				command_buffer,
				staging_images[i].image,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&image_blit,
				VK_FILTER_NEAREST
			);
		}


		if (mip_levels > 1)
		{
			//############################################################################################################
			//                     																                         #
			//                                              Create mipmaps                                               #
			//                     																                         #
			//############################################################################################################

			VkImageBlit mipmaps_blit = {};
			mipmaps_blit.srcOffsets[0] = { 0, 0, 0 };
			mipmaps_blit.srcSubresource.aspectMask = aspect_mask;
			mipmaps_blit.srcSubresource.baseArrayLayer = 0;
			mipmaps_blit.srcSubresource.layerCount = array_layers;

			mipmaps_blit.dstOffsets[0] = { 0, 0, 0 };
			mipmaps_blit.dstSubresource.aspectMask = aspect_mask;
			mipmaps_blit.dstSubresource.baseArrayLayer = 0;
			mipmaps_blit.dstSubresource.layerCount = array_layers;

			for (u_int32_t level = 1; level < mip_levels; level++)
			{
				VkImageMemoryBarrier barrier = {};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.image = image;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.subresourceRange.aspectMask = aspect_mask;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.layerCount = array_layers;
				barrier.subresourceRange.baseMipLevel = level - 1;
				barrier.subresourceRange.levelCount = 1;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

				vkCmdPipelineBarrier(
					command_buffer,
					VK_PIPELINE_STAGE_TRANSFER_BIT,
					VK_PIPELINE_STAGE_TRANSFER_BIT,
					0,
					0, nullptr,
					0, nullptr,
					1, &barrier
				);

				mipmaps_blit.srcOffsets[1] = {
					static_cast<int32_t>(extent2D.width >> (level - 1)),
					static_cast<int32_t>(extent2D.height >> (level - 1)),
					1
				};
				mipmaps_blit.srcSubresource.mipLevel = level - 1;

				mipmaps_blit.dstOffsets[1] = {
					static_cast<int32_t>(extent2D.width >> level),
					static_cast<int32_t>(extent2D.height >> level),
					1
				};
				mipmaps_blit.dstSubresource.mipLevel = level;

				vkCmdBlitImage(
					command_buffer,
					image,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1,
					&mipmaps_blit,
					VK_FILTER_NEAREST
				);
			}

			// Transition last mip level to shader read
			VkImageMemoryBarrier barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = image;
			barrier.subresourceRange.aspectMask = aspect_mask;
			barrier.subresourceRange.baseMipLevel = mip_levels - 1;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = create_info.final_layout;

			vkCmdPipelineBarrier(
				command_buffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);


			// Transition image layout to shader read (except last mip level)
			VkImageMemoryBarrier second_barrier = {};
			second_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			second_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			second_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			second_barrier.image = image;
			second_barrier.subresourceRange.aspectMask = aspect_mask;
			second_barrier.subresourceRange.baseMipLevel = 0;
			second_barrier.subresourceRange.levelCount = mip_levels - 1;
			second_barrier.subresourceRange.baseArrayLayer = 0;
			second_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
			second_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			second_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			second_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			second_barrier.newLayout = create_info.final_layout;

			vkCmdPipelineBarrier(
				command_buffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &second_barrier
			);
		}
		else // No mipmaps
		{
			transitionLayout(
				command_buffer,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				create_info.final_layout
			);
		}

	}
	else // No files
	{
		transitionLayout(
			command_buffer,
			VK_IMAGE_LAYOUT_UNDEFINED,
			create_info.final_layout
		);
	}

	//############################################################################################################
	//                     																                         #
	//                                       Create image view and sampler                                       #
	//                     																                         #
	//############################################################################################################

	if (create_info.create_view)
	{
		VkImageViewCreateInfo view_create_info = {};
		view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_create_info.image = image;
		view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_create_info.format = create_info.format;
		view_create_info.subresourceRange.aspectMask = aspect_mask;
		view_create_info.subresourceRange.baseMipLevel = 0;
		view_create_info.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		view_create_info.subresourceRange.baseArrayLayer = 0;
		view_create_info.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

		if (array_layers > 1)
		{
			view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		}
		if (create_info.is_cube_map)
		{
			view_create_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		}

		VK_CHECK(
			vkCreateImageView(m_device, &view_create_info, nullptr, &view),
			"Failed to create image view"
		);
	}

	if (create_info.create_sampler)
	{
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(physical_device, &properties);

		VkSamplerCreateInfo sampler_create_info = {};
		sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler_create_info.magFilter = create_info.sampler_filter;
		sampler_create_info.minFilter = create_info.sampler_filter;
		sampler_create_info.addressModeU = create_info.sampler_address_mode;
		sampler_create_info.addressModeV = create_info.sampler_address_mode;
		sampler_create_info.addressModeW = create_info.sampler_address_mode;
		sampler_create_info.anisotropyEnable = create_info.sampler_anisotropy_enable;
		sampler_create_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
		sampler_create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		sampler_create_info.unnormalizedCoordinates = VK_FALSE;
		sampler_create_info.compareEnable = VK_FALSE;
		sampler_create_info.compareOp = VK_COMPARE_OP_ALWAYS;
		sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler_create_info.mipLodBias = 0.0f;
		sampler_create_info.minLod = 0.0f;
		sampler_create_info.maxLod = static_cast<float>(mip_levels);

		VK_CHECK(
			vkCreateSampler(m_device, &sampler_create_info, nullptr, &sampler),
			"Failed to create sampler"
		);
	}

	// End command buffer if there are files because else the stagging images will be destroyed before the command buffer is ended
	if (files_count > 0)
	{
		command_buffer.end();
	}
}

void Image::transitionLayout(
	VkCommandBuffer command_buffer,
	VkImageLayout old_layout,
	VkImageLayout new_layout,
	VkPipelineStageFlags src_stage,
	VkPipelineStageFlags dst_stage
)
{
	assert(image != VK_NULL_HANDLE && "Cannot transition layout of null image");

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = aspect_mask;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = 0;

	if (src_stage == VK_PIPELINE_STAGE_NONE || dst_stage == VK_PIPELINE_STAGE_NONE)
	{
		switch (old_layout)
		{
		case VK_IMAGE_LAYOUT_UNDEFINED:
		{
			barrier.srcAccessMask = 0;
			src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

			switch (new_layout)
			{
			case VK_IMAGE_LAYOUT_GENERAL:
			{
				barrier.dstAccessMask = 0;
				dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
				break;
			}
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			{
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
				break;
			}
			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			{
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
				break;
			}
			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			{
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				break;
			}
			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			{
				barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				break;
			}
			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			{
				barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
				break;
			}
			default:
			{
				throw std::invalid_argument("Unsupported new layout");
				break;
			}
			}

			break;
		}
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

			switch (new_layout)
			{
			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			{
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
				break;
			}
			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			{
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				break;
			}
			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			{
				barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				break;
			}
			default:
			{
				throw std::invalid_argument("Unsupported new layout");
				break;
			}
			}

			break;
		}
		default:
		{
			throw std::invalid_argument("Unsupported old layout");
			break;
		}
		}
	}

	vkCmdPipelineBarrier(
		command_buffer,
		src_stage,
		dst_stage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);
}

void Image::clear()
{
	if (m_device != VK_NULL_HANDLE)
	{
		if (image != VK_NULL_HANDLE)
		{
			vkDestroyImage(m_device, image, nullptr);
			image = VK_NULL_HANDLE;
		}
		if (memory != VK_NULL_HANDLE)
		{
			VulkanMemoryAllocator & vma = VulkanMemoryAllocator::getInstance();
			vma.freeMemory(m_device, memory, nullptr);
			memory = VK_NULL_HANDLE;
		}
		if (view != VK_NULL_HANDLE)
		{
			vkDestroyImageView(m_device, view, nullptr);
			view = VK_NULL_HANDLE;
		}
		if (sampler != VK_NULL_HANDLE)
		{
			vkDestroySampler(m_device, sampler, nullptr);
			sampler = VK_NULL_HANDLE;
		}
		m_device = VK_NULL_HANDLE;
	}
}
