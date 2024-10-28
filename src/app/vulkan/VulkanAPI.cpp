#include "VulkanAPI.hpp"
#include "logger.hpp"
#include "Block.hpp"
#include "Model.hpp"
#include "ObjLoader.hpp"
#include "Item.hpp"

#include "Tracy.hpp"

#include <stb_image.h>

#include <cstring>
#include <set>
#include <algorithm>
#include <map>

VulkanAPI::VulkanAPI(GLFWwindow * window):
	window(window)
{
	ZoneScoped;

	createInstance();
	loadVulkanFunctions();
	setupDebugMessenger();
	createSurface(window);
	pickPhysicalDevice();
	createLogicalDevice();
	createSwapChain(window);
	createCommandPool();
	createCommandBuffer();
	createSyncObjects();

	createColorAttachement();
	createDepthAttachement();
	createUniformBuffers();
	createTextureArray(BlockInfo::texture_names, 64);
	createCubeMap({
		"assets/textures/skybox/right.jpg",
		"assets/textures/skybox/left.jpg",
		"assets/textures/skybox/top.jpg",
		"assets/textures/skybox/bottom.jpg",
		"assets/textures/skybox/front.jpg",
		"assets/textures/skybox/back.jpg"
	}, 512);
	createTextureImage();

	createDescriptors();
	createRenderPass();
	createPipelines();
	createFramebuffers();

	createMeshes();
	createItemMeshes();

	setupTextRenderer();

	prerenderItemIconImages();

	setupImgui();

	setupTracy();

	LOG_INFO("VulkanAPI initialized");
}

VulkanAPI::~VulkanAPI()
{
	ZoneScoped;

	vkDeviceWaitIdle(device);

	VulkanMemoryAllocator & vma = VulkanMemoryAllocator::getInstance();

	destroyTracy();

	destroyImGuiTextures();

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	destroyTextRenderer();

	{
		std::lock_guard lock(mesh_map_mutex);
		for (auto & [key, mesh] : mesh_map)
		{
			vkDestroyBuffer(device, mesh.buffer, nullptr);
			vma.freeMemory(device, mesh.buffer_memory, nullptr);
		}
	}

	for (int i = 0; i < max_frames_in_flight; i++)
	{
		vkUnmapMemory(device, camera_ubo.memory[i]);
		vma.freeMemory(device, camera_ubo.memory[i], nullptr);
		vkDestroyBuffer(device, camera_ubo.buffers[i], nullptr);

		vkUnmapMemory(device, light_mat_ubo.memory[i]);
		vma.freeMemory(device, light_mat_ubo.memory[i], nullptr);
		vkDestroyBuffer(device, light_mat_ubo.buffers[i], nullptr);

		vkUnmapMemory(device, atmosphere_ubo.memory[i]);
		vma.freeMemory(device, atmosphere_ubo.memory[i], nullptr);
		vkDestroyBuffer(device, atmosphere_ubo.buffers[i], nullptr);
	}

	for (int i = 0; i < max_frames_in_flight; i++)
	{
		vkDestroyFramebuffer(device, lighting_framebuffers[i], nullptr);
		vkDestroyFramebuffer(device, shadow_framebuffers[i], nullptr);
		vkDestroyFramebuffer(device, water_framebuffers[i], nullptr);
		vkDestroyFramebuffer(device, hud_framebuffers[i], nullptr);
	}
	vkDestroyFramebuffer(device, prerender_item_icon_framebuffer, nullptr);

	vkDestroyRenderPass(device, lighting_render_pass, nullptr);
	vkDestroyRenderPass(device, shadow_render_pass, nullptr);
	vkDestroyRenderPass(device, water_render_pass, nullptr);
	vkDestroyRenderPass(device, hud_render_pass, nullptr);
	vkDestroyRenderPass(device, prerender_item_icon_render_pass, nullptr);

	vkDestroyDescriptorPool(device, imgui_descriptor_pool, nullptr);

	for (int i = 0; i < max_frames_in_flight; i++)
	{
		vkDestroySemaphore(device, image_available_semaphores[i], nullptr);
		vkDestroySemaphore(device, shadow_pass_finished_semaphores[i], nullptr);
		vkDestroySemaphore(device, main_render_finished_semaphores[i], nullptr);
		vkDestroySemaphore(device, compute_finished_semaphores[i], nullptr);
		vkDestroySemaphore(device, copy_finished_semaphores[i], nullptr);
		vkDestroySemaphore(device, imgui_render_finished_semaphores[i], nullptr);
		vkDestroyFence(device, in_flight_fences[i], nullptr);
	}
	vkDestroyFence(device, single_time_command_fence, nullptr);

	vkFreeCommandBuffers(device, command_pool, static_cast<uint32_t>(draw_shadow_pass_command_buffers.size()), draw_shadow_pass_command_buffers.data());
	vkFreeCommandBuffers(device, command_pool, static_cast<uint32_t>(draw_command_buffers.size()), draw_command_buffers.data());
	vkFreeCommandBuffers(device, command_pool, static_cast<uint32_t>(copy_command_buffers.size()), copy_command_buffers.data());
	vkFreeCommandBuffers(device, command_pool, static_cast<uint32_t>(compute_command_buffers.size()), compute_command_buffers.data());
	vkFreeCommandBuffers(device, command_pool, static_cast<uint32_t>(imgui_command_buffers.size()), imgui_command_buffers.data());
	vkDestroyCommandPool(device, command_pool, nullptr);
	vkFreeCommandBuffers(device, transfer_command_pool, 1, &transfer_command_buffers);
	vkDestroyCommandPool(device, transfer_command_pool, nullptr);

	// Bad design, but better than nothing until I find a better solution
	{
		output_attachement.clear();
		color_attachement.clear();
		depth_attachement.clear();
		shadow_map_depth_attachement.clear();
		block_textures.clear();
		skybox_cube_map.clear();
		crosshair_image.clear();
		toolbar_image.clear();
		toolbar_cursor_image.clear();
		player_skin_image.clear();
		debug_info_image.clear();
		item_icon_images.clear();

		debug_info_buffers.clear();

		camera_descriptor.clear();
		block_textures_descriptor.clear();
		cube_map_descriptor.clear();
		shadow_map_descriptor.clear();
		water_renderpass_input_attachement_descriptor.clear();
		test_image_descriptor.clear();
		light_view_proj_descriptor.clear();
		crosshair_image_descriptor.clear();
		toolbar_image_descriptor.clear();
		toolbar_cursor_image_descriptor.clear();
		player_skin_image_descriptor.clear();
		atmosphere_descriptor.clear();
		debug_info_image_descriptor.clear();
		item_icon_descriptor.clear();

		chunk_pipeline.clear();
		water_pipeline.clear();
		line_pipeline.clear();
		skybox_pipeline.clear();
		sun_pipeline.clear();
		shadow_pipeline.clear();
		test_image_pipeline.clear();
		entity_pipeline.clear();
		player_pipeline.clear();
		hud_pipeline.clear();
		prerender_item_icon_pipeline.clear();
		item_icon_pipeline.clear();

		swapchain.clear();
	}

	vkDestroyDevice(device, nullptr);

	#ifndef NDEBUG
		vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
	#endif

	vkDestroySurfaceKHR(instance, surface, nullptr);

	vkDestroyInstance(instance, nullptr);
}

void VulkanAPI::createInstance()
{
	#ifndef NDEBUG
		if (!checkValidationLayerSupport())
		{
			throw std::runtime_error("Validation layers requested, but not available!");
		}
	#endif

	VkApplicationInfo app_info = {};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "Vulkan Tutorial";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "No Engine";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;

	std::vector<const char *> extensions = getRequiredExtensions();
	create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	create_info.ppEnabledExtensionNames = extensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debug_create_info;
	populateDebugMessengerCreateInfo(debug_create_info);
	#ifndef NDEBUG
		create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
		create_info.ppEnabledLayerNames = validation_layers.data();

		// Shader printf is a feature of the validation layers that needs to be enabled
		// std::vector<VkValidationFeatureEnableEXT>  validation_feature_enables = { VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT };

		// VkValidationFeaturesEXT validation_features = { VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT };
		// validation_features.enabledValidationFeatureCount = static_cast<uint32_t>(validation_feature_enables.size());
		// validation_features.pEnabledValidationFeatures = validation_feature_enables.data();

		// debug_create_info.pNext = &validation_features;
		create_info.pNext = &debug_create_info;
	#else
		create_info.enabledLayerCount = 0;
	#endif

	VK_CHECK(
		vkCreateInstance(&create_info, nullptr, &instance),
		"Failed to create instance"
	);
}

bool VulkanAPI::checkValidationLayerSupport()
{
	uint32_t layer_count;
	vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
	std::vector<VkLayerProperties> available_layers(layer_count);
	vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

	for (const char * layer_name : validation_layers)
	{
		bool layer_found = false;

		for (const auto & layer_properties : available_layers)
		{
			if (strcmp(layer_name, layer_properties.layerName) == 0)
			{
				layer_found = true;
				break;
			}
		}

		if (!layer_found)
		{
			return false;
		}
	}

	return true;
}

std::vector<const char *> VulkanAPI::getRequiredExtensions()
{
	uint32_t glfw_extension_count = 0;
	const char** glfw_extensions;
	glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

	std::vector<const char *> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

	#ifndef NDEBUG
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	#endif

	return extensions;
}

void VulkanAPI::loadVulkanFunctions()
{
#define VK_LOAD_FUNCTION(name) name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(instance, #name)); \
	if (name == nullptr) { \
		throw std::runtime_error("Failed to load Vulkan function: " #name); \
	}

	#ifndef NDEBUG
		VK_LOAD_FUNCTION(vkCreateDebugUtilsMessengerEXT)
		VK_LOAD_FUNCTION(vkDestroyDebugUtilsMessengerEXT)
	#endif

	VK_LOAD_FUNCTION(vkGetPhysicalDeviceCalibrateableTimeDomainsEXT)
	VK_LOAD_FUNCTION(vkGetCalibratedTimestampsEXT)

#undef VK_LOAD_FUNCTION
}

void VulkanAPI::setupDebugMessenger()
{
	#ifndef NDEBUG
		VkDebugUtilsMessengerCreateInfoEXT create_info;
		populateDebugMessengerCreateInfo(create_info);

		VK_CHECK(
			vkCreateDebugUtilsMessengerEXT(instance, &create_info, nullptr, &debug_messenger),
			"Failed to set up debug messenger"
		);
	#endif
}

void VulkanAPI::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT & create_info)
{
	create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	create_info.pfnUserCallback = debugCallback;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanAPI::debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
	VkDebugUtilsMessageTypeFlagsEXT message_type,
	const VkDebugUtilsMessengerCallbackDataEXT * callback_data,
	void *
)
{
	(void)message_type;

	// if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
	// {
	// 	LOG_TRACE(callback_data->pMessage);
	// }
	// if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
	// {
	// 	LOG_INFO(callback_data->pMessage);
	// }
	if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		LOG_WARNING(callback_data->pMessage);
	}
	if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	{
		LOG_ERROR(callback_data->pMessage);
	}

	return VK_FALSE;
}

void VulkanAPI::createSurface(GLFWwindow * window)
{
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create window surface");
	}
}

void VulkanAPI::pickPhysicalDevice()
{
	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(instance, &device_count, nullptr);

	if (device_count == 0)
	{
		throw std::runtime_error("Failed to find GPUs with Vulkan support");
	}

	std::vector<VkPhysicalDevice> devices(device_count);
	vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

	std::multimap<int, VkPhysicalDevice, std::greater<int>> candidates;

	for (const auto & device : devices)
	{
		int score = ratePhysicalDevice(device);
		candidates.insert(std::make_pair(score, device));
	}

	if (candidates.begin()->first > 0)
	{
		physical_device = candidates.begin()->second;
	}
	else
	{
		throw std::runtime_error("Failed to find a suitable GPU");
	}

	queue_family_indices = findQueueFamilies(physical_device);

	VkPhysicalDeviceProperties device_properties;
	vkGetPhysicalDeviceProperties(physical_device, &device_properties);

	LOG_INFO("device name: " << device_properties.deviceName);
}

bool VulkanAPI::isDeviceSuitable(VkPhysicalDevice device)
{
	QueueFamilyIndices indices = findQueueFamilies(device);

	bool extensions_supported = checkDeviceExtensionSupport(device);

	bool swapchain_adequate = false;
	if (extensions_supported)
	{
		Swapchain::SupportDetails swapchain_support = Swapchain::querySwapChainSupport(device, surface);
		swapchain_adequate = !swapchain_support.formats.empty() && !swapchain_support.present_modes.empty();
	}

	VkPhysicalDeviceFeatures supported_features;
	vkGetPhysicalDeviceFeatures(device, &supported_features);

	return indices.isComplete()
		&& extensions_supported
		&& swapchain_adequate
		&& supported_features.samplerAnisotropy;
}

int VulkanAPI::ratePhysicalDevice(VkPhysicalDevice device)
{
	if (!isDeviceSuitable(device))
	{
		return 0;
	}

	int score = 1;

	VkPhysicalDeviceProperties device_properties;
	vkGetPhysicalDeviceProperties(device, &device_properties);

	// VkPhysicalDeviceFeatures device_features;
	// vkGetPhysicalDeviceFeatures(device, &device_features);

	if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
		score += 1000;
	}

	return score;
}

QueueFamilyIndices VulkanAPI::findQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;

	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
	std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

	std::vector<uint32_t> queue_families_use_count(queue_family_count, 0);

	int i = 0;
	for (const auto & queue_family : queue_families)
	{
		// Find a queue family that supports graphics
		if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT && queue_families_use_count[i] < queue_family.queueCount)
		{
			indices.graphics_family = i;
			queue_families_use_count[i]++;
		}

		// Find a queue family that supports presentation
		VkBool32 present_support = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
		if (present_support && queue_families_use_count[i] < queue_family.queueCount)
		{
			indices.present_family = i;
			queue_families_use_count[i]++;
		}

		// Find a queue family that supports transfer
		if (queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT && queue_families_use_count[i] < queue_family.queueCount)
		{
			indices.transfer_family = i;
			queue_families_use_count[i]++;
		}

		if (indices.isComplete())
		{
			break;
		}

		i++;
	}

	return indices;
}

bool VulkanAPI::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t extension_count;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
	std::vector<VkExtensionProperties> available_extensions(extension_count);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

	std::set<std::string> required_extensions(device_extensions.begin(), device_extensions.end());

	for (const auto & extension : available_extensions)
	{
		required_extensions.erase(extension.extensionName);
	}

	for (const auto & extension : required_extensions)
	{
		LOG_ERROR("Missing extension: " << extension);
	}

	return required_extensions.empty();
}

void VulkanAPI::createLogicalDevice()
{
	std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
	std::set<uint32_t> unique_queue_families = {
		queue_family_indices.graphics_family.value(),
		queue_family_indices.present_family.value(),
		queue_family_indices.transfer_family.value()
	};
	std::map<uint32_t, std::vector<float>> queue_priorities;

	for (uint32_t queue_family : unique_queue_families)
	{
		VkDeviceQueueCreateInfo queue_create_info = {};

		if (queue_family == queue_family_indices.graphics_family.value())
		{
			queue_create_info.queueCount += 1;
		}
		if (queue_family == queue_family_indices.present_family.value())
		{
			queue_create_info.queueCount += 1;
		}
		if (queue_family == queue_family_indices.transfer_family.value())
		{
			queue_create_info.queueCount += 1;
		}
		queue_priorities.insert({queue_family, std::vector<float>(queue_create_info.queueCount, 1.0f)});

		queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_info.queueFamilyIndex = queue_family;
		queue_create_info.pQueuePriorities = queue_priorities[queue_family].data();
		queue_create_infos.push_back(queue_create_info);
	}




	VkPhysicalDeviceFeatures device_features = {};
	device_features.samplerAnisotropy = VK_TRUE;
	device_features.fillModeNonSolid = VK_TRUE;
	device_features.wideLines = VK_TRUE;
	device_features.shaderInt64 = VK_TRUE;
	device_features.geometryShader = VK_TRUE;

	VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_features = {};
	dynamic_rendering_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
	dynamic_rendering_features.dynamicRendering = VK_TRUE;

	VkPhysicalDeviceVulkan12Features vulkan_12_features = {};
	vulkan_12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	vulkan_12_features.bufferDeviceAddress = VK_TRUE;
	vulkan_12_features.descriptorIndexing = VK_TRUE;
	vulkan_12_features.shaderInt8 = VK_TRUE;
	vulkan_12_features.storageBuffer8BitAccess = VK_TRUE;
	vulkan_12_features.pNext = &dynamic_rendering_features;


	VkDeviceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.pQueueCreateInfos = queue_create_infos.data();
	create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
	create_info.pEnabledFeatures = &device_features;
	create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
	create_info.ppEnabledExtensionNames = device_extensions.data();

	create_info.pNext = &vulkan_12_features;

	#ifndef NDEBUG
		create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
		create_info.ppEnabledLayerNames = validation_layers.data();
	#else
		create_info.enabledLayerCount = 0;
	#endif


	VK_CHECK(
		vkCreateDevice(physical_device, &create_info, nullptr, &device),
		"Failed to create logical device"
	);

	std::map<uint32_t, uint32_t> queues_indices = {
		{queue_family_indices.graphics_family.value(), 0},
		{queue_family_indices.present_family.value(), 0},
		{queue_family_indices.transfer_family.value(), 0}
	};

	vkGetDeviceQueue(device, queue_family_indices.graphics_family.value(), queues_indices[queue_family_indices.graphics_family.value()]++, &graphics_queue);
	vkGetDeviceQueue(device, queue_family_indices.present_family.value(), queues_indices[queue_family_indices.present_family.value()]++, &present_queue);
	vkGetDeviceQueue(device, queue_family_indices.transfer_family.value(), queues_indices[queue_family_indices.transfer_family.value()]++, &transfer_queue);
}

void VulkanAPI::createSwapChain(GLFWwindow * window)
{
	Swapchain::SupportDetails swapchain_support = Swapchain::querySwapChainSupport(physical_device, surface);

	Swapchain::CreateInfo create_info = {};
	create_info.surface_format = chooseSwapSurfaceFormat(swapchain_support.formats);
	create_info.present_mode = chooseSwapPresentMode(swapchain_support.present_modes);
	create_info.extent = chooseSwapExtent(swapchain_support.capabilities, window);
	create_info.old_swapchain = swapchain.swapchain;

	swapchain = Swapchain(device, physical_device, surface, create_info);
}

void VulkanAPI::recreateSwapChain(GLFWwindow * window)
{
	int width = 0, height = 0;
	glfwGetFramebufferSize(window, &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(window, &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(device);

	for (int i = 0; i < max_frames_in_flight; i++)
	{
		vkDestroyFramebuffer(device, lighting_framebuffers[i], nullptr);
		vkDestroyFramebuffer(device, shadow_framebuffers[i], nullptr);
		vkDestroyFramebuffer(device, water_framebuffers[i], nullptr);
		vkDestroyFramebuffer(device, hud_framebuffers[i], nullptr);
	}
	vkDestroyFramebuffer(device, prerender_item_icon_framebuffer, nullptr);

	createSwapChain(window);
	createColorAttachement();
	createDepthAttachement();
	createPipelines();
	createFramebuffers();

	shadow_map_descriptor.update(
		device,
		shadow_map_depth_attachement.view,
		shadow_map_depth_attachement.sampler,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);

	test_image_descriptor.update(
		device,
		shadow_map_depth_attachement.view,
		shadow_map_depth_attachement.sampler,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);

	{ // Update water subpass input attachement descriptor
		VkDescriptorImageInfo color_image_info = {};
		color_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		color_image_info.imageView = color_attachement.view;
		color_image_info.sampler = VK_NULL_HANDLE;

		VkWriteDescriptorSet color_descriptor_write = {};
		color_descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		color_descriptor_write.dstSet = water_renderpass_input_attachement_descriptor.set;
		color_descriptor_write.dstBinding = 0;
		color_descriptor_write.dstArrayElement = 0;
		color_descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		color_descriptor_write.descriptorCount = 1;
		color_descriptor_write.pImageInfo = &color_image_info;

		VkDescriptorImageInfo depth_image_info = {};
		depth_image_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
		depth_image_info.imageView = depth_attachement.view;
		depth_image_info.sampler = VK_NULL_HANDLE;

		VkWriteDescriptorSet depth_descriptor_write = {};
		depth_descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		depth_descriptor_write.dstSet = water_renderpass_input_attachement_descriptor.set;
		depth_descriptor_write.dstBinding = 1;
		depth_descriptor_write.dstArrayElement = 0;
		depth_descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		depth_descriptor_write.descriptorCount = 1;
		depth_descriptor_write.pImageInfo = &depth_image_info;

		std::vector<VkWriteDescriptorSet> descriptor_writes = { color_descriptor_write, depth_descriptor_write };

		vkUpdateDescriptorSets(
			device,
			static_cast<uint32_t>(descriptor_writes.size()),
			descriptor_writes.data(),
			0, nullptr
		);
	}
}

VkSurfaceFormatKHR VulkanAPI::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> & available_formats)
{
	for (const auto & available_format : available_formats)
	{
		if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return available_format;
		}
	}

	return available_formats[0];
}

VkPresentModeKHR VulkanAPI::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> & available_present_modes)
{
	for (const auto & available_present_mode : available_present_modes)
	{
		if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return available_present_mode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanAPI::chooseSwapExtent(const VkSurfaceCapabilitiesKHR & capabilities, GLFWwindow * window)
{
	if (capabilities.currentExtent.width != UINT32_MAX)
	{
		return capabilities.currentExtent;
	}
	else
	{
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		VkExtent2D actual_extent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actual_extent.width = std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actual_extent;
	}
}

void VulkanAPI::createCommandPool()
{
	QueueFamilyIndices queue_family_indices = findQueueFamilies(physical_device);

	VkCommandPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.queueFamilyIndex = queue_family_indices.graphics_family.value();
	pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VK_CHECK(
		vkCreateCommandPool(device, &pool_info, nullptr, &command_pool),
		"Failed to create command pool"
	);

	pool_info.queueFamilyIndex = queue_family_indices.transfer_family.value();

	VK_CHECK(
		vkCreateCommandPool(device, &pool_info, nullptr, &transfer_command_pool),
		"Failed to create command pool"
	);
}

void VulkanAPI::createCommandBuffer()
{
	draw_shadow_pass_command_buffers.resize(max_frames_in_flight);
	draw_command_buffers.resize(max_frames_in_flight);
	copy_command_buffers.resize(max_frames_in_flight);
	compute_command_buffers.resize(max_frames_in_flight);
	imgui_command_buffers.resize(max_frames_in_flight);

	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = static_cast<uint32_t>(max_frames_in_flight);

	VK_CHECK(
		vkAllocateCommandBuffers(device, &alloc_info, draw_shadow_pass_command_buffers.data()),
		"Failed to allocate command buffers"
	);
	VK_CHECK(
		vkAllocateCommandBuffers(device, &alloc_info, draw_command_buffers.data()),
		"Failed to allocate command buffers"
	);
	VK_CHECK(
		vkAllocateCommandBuffers(device, &alloc_info, copy_command_buffers.data()),
		"Failed to allocate command buffers"
	);
	VK_CHECK(
		vkAllocateCommandBuffers(device, &alloc_info, imgui_command_buffers.data()),
		"Failed to allocate command buffers"
	);
	VK_CHECK(
		vkAllocateCommandBuffers(device, &alloc_info, compute_command_buffers.data()),
		"Failed to allocate command buffers"
	);

	alloc_info.commandPool = transfer_command_pool;
	alloc_info.commandBufferCount = 1;

	VK_CHECK(
		vkAllocateCommandBuffers(device, &alloc_info, &transfer_command_buffers),
		"Failed to allocate command buffers"
	);
}

void VulkanAPI::createSyncObjects()
{
	image_available_semaphores.resize(max_frames_in_flight);
	shadow_pass_finished_semaphores.resize(max_frames_in_flight);
	main_render_finished_semaphores.resize(max_frames_in_flight);
	compute_finished_semaphores.resize(max_frames_in_flight);
	copy_finished_semaphores.resize(max_frames_in_flight);
	imgui_render_finished_semaphores.resize(max_frames_in_flight);
	in_flight_fences.resize(max_frames_in_flight);

	VkSemaphoreCreateInfo semaphore_info = {};
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fence_info = {};
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < static_cast<size_t>(max_frames_in_flight); i++)
	{
		VK_CHECK(
			vkCreateSemaphore(device, &semaphore_info, nullptr, &image_available_semaphores[i]),
			"Failed to create semaphores"
		);
		VK_CHECK(
			vkCreateSemaphore(device, &semaphore_info, nullptr, &shadow_pass_finished_semaphores[i]),
			"Failed to create semaphores"
		);
		VK_CHECK(
			vkCreateSemaphore(device, &semaphore_info, nullptr, &main_render_finished_semaphores[i]),
			"Failed to create semaphores"
		);
		VK_CHECK(
			vkCreateSemaphore(device, &semaphore_info, nullptr, &compute_finished_semaphores[i]),
			"Failed to create semaphores"
		);
		VK_CHECK(
			vkCreateSemaphore(device, &semaphore_info, nullptr, &copy_finished_semaphores[i]),
			"Failed to create semaphores"
		);
		VK_CHECK(
			vkCreateSemaphore(device, &semaphore_info, nullptr, &imgui_render_finished_semaphores[i]),
			"Failed to create semaphores"
		);
		VK_CHECK(
			vkCreateFence(device, &fence_info, nullptr, &in_flight_fences[i]),
			"Failed to create fences"
		);
	}

	VK_CHECK(
		vkCreateFence(device, &fence_info, nullptr, &single_time_command_fence),
		"Failed to create fences"
	);
}

void VulkanAPI::createColorAttachement()
{
	SingleTimeCommand command_buffer(device, command_pool, graphics_queue);


	Image::CreateInfo color_attachement_info = {};
	color_attachement_info.extent = { swapchain.extent.width * 2, swapchain.extent.height * 2 };
	color_attachement_info.format = swapchain.image_format;
	color_attachement_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
								| VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
								| VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	color_attachement_info.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	color_attachement_info.final_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	color_attachement_info.create_view = true;

	color_attachement = Image(device, physical_device, command_buffer, color_attachement_info);


	color_attachement_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
								| VK_IMAGE_USAGE_TRANSFER_SRC_BIT
								| VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	output_attachement = Image(device, physical_device, command_buffer, color_attachement_info);
}

void VulkanAPI::createDepthAttachement()
{
	SingleTimeCommand command_buffer(device, command_pool, graphics_queue);


	Image::CreateInfo depth_attachement_info = {};
	depth_attachement_info.extent = { swapchain.extent.width * 2, swapchain.extent.height * 2 };
	depth_attachement_info.aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
	depth_attachement_info.format = findSupportedFormat(
		{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
	depth_attachement_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
	depth_attachement_info.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	depth_attachement_info.final_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depth_attachement_info.create_view = true;

	depth_attachement = Image(device, physical_device, command_buffer, depth_attachement_info);


	depth_attachement_info.extent = { shadow_map_size, shadow_map_size };
	depth_attachement_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	depth_attachement_info.create_sampler = true;
	depth_attachement_info.sampler_address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	depth_attachement_info.sampler_filter = VK_FILTER_LINEAR;
	depth_attachement_info.sampler_anisotropy_enable = VK_FALSE;
	depth_attachement_info.array_layers = shadow_maps_count;

	shadow_map_depth_attachement = Image(device, physical_device, command_buffer, depth_attachement_info);
}

void VulkanAPI::createUBO(UBO & ubo, const VkDeviceSize size, const uint32_t count)
{
	ubo.buffers.resize(count);
	ubo.memory.resize(count);
	ubo.mapped_memory.resize(count);

	for (uint32_t i = 0; i < count; i++)
	{
		createBuffer(
			size,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			ubo.buffers[i],
			ubo.memory[i]
		);

		VK_CHECK(
			vkMapMemory(device, ubo.memory[i], 0, size, 0, &ubo.mapped_memory[i]),
			"Failed to map memory for camera uniform buffer."
		);
	}
}

void VulkanAPI::createUniformBuffers()
{
	createUBO(camera_ubo, sizeof(ViewProjMatrices), max_frames_in_flight);
	createUBO(light_mat_ubo, sizeof(ShadowMapLight), max_frames_in_flight);
	createUBO(atmosphere_ubo, sizeof(AtmosphereParams), max_frames_in_flight);
}

void VulkanAPI::createTextureArray(const std::vector<std::string> & file_paths, uint32_t size)
{
	Image::CreateInfo image_info = {};
	image_info.extent = {size, size};
	image_info.mip_levels = static_cast<uint32_t>(std::floor(std::log2(size))) + 1;
	image_info.format = VK_FORMAT_R8G8B8A8_SRGB;
	image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	image_info.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	image_info.final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	image_info.create_view = true;
	image_info.create_sampler = true;
	image_info.sampler_filter = VK_FILTER_NEAREST;
	image_info.file_paths = file_paths;

	SingleTimeCommand command_buffer(device, command_pool, graphics_queue);

	block_textures = Image(device, physical_device, command_buffer, image_info);
}

void VulkanAPI::createCubeMap(const std::array<std::string, 6> & file_paths, uint32_t size)
{
	Image::CreateInfo _image_info = {};
	_image_info.extent = {size, size};
	_image_info.format = VK_FORMAT_R8G8B8A8_SRGB;
	_image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	_image_info.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	_image_info.final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	_image_info.create_view = true;
	_image_info.create_sampler = true;
	_image_info.file_paths = {
		file_paths[0], file_paths[1], file_paths[2], file_paths[3], file_paths[4], file_paths[5]
	};
	_image_info.is_cube_map = true;

	SingleTimeCommand command_buffer_2(device, command_pool, graphics_queue);

	skybox_cube_map = Image(device, physical_device, command_buffer_2, _image_info);
}

void VulkanAPI::createHudImages(
	const std::string & file_path,
	Image & image
)
{
	Image::CreateInfo image_info = {};
	image_info.file_paths = {file_path};
	image_info.format = VK_FORMAT_R8G8B8A8_SRGB;
	image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	image_info.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	image_info.final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	image_info.create_view = true;
	image_info.create_sampler = true;

	SingleTimeCommand command_buffer(device, command_pool, graphics_queue);

	image = Image(device, physical_device, command_buffer, image_info);
}

void VulkanAPI::createTextureImage()
{
	{ // player skin
		Image::CreateInfo image_info = {};
		image_info.file_paths = {"assets/textures/skin/player/steve.png"};
		image_info.extent = {64, 64};
		image_info.format = VK_FORMAT_R8G8B8A8_SRGB;
		image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		image_info.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		image_info.final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		image_info.create_view = true;
		image_info.create_sampler = true;
		image_info.sampler_filter = VK_FILTER_NEAREST;

		SingleTimeCommand command_buffer(device, command_pool, graphics_queue);

		player_skin_image = Image(device, physical_device, command_buffer, image_info);
	}

	{ // Hud images
		createHudImages("assets/textures/hud/crosshair.png", crosshair_image);
		createHudImages("assets/textures/hud/toolbar.png", toolbar_image);
		createHudImages("assets/textures/hud/toolbar_cursor.png", toolbar_cursor_image);
	}

	{ // Debug info
		Image::CreateInfo image_info = {};
		image_info.extent = {512, 512};
		image_info.format = VK_FORMAT_R8G8B8A8_SRGB;
		image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		image_info.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		image_info.final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		image_info.create_view = true;
		image_info.create_sampler = true;

		SingleTimeCommand command_buffer(device, command_pool, graphics_queue);

		debug_info_image = Image(device, physical_device, command_buffer, image_info);

		// create debug info buffer
		debug_info_buffers.resize(max_frames_in_flight);
		for (int i = 0; i < max_frames_in_flight; i++)
		{
			Buffer::CreateInfo buffer_info = {};
			buffer_info.size = debug_info_image.extent2D.width * debug_info_image.extent2D.height * 4;
			buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			buffer_info.memory_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

			debug_info_buffers[i] = Buffer(device, physical_device, buffer_info);
		}
	}

	{ // Item icons
		Image::CreateInfo image_info = {};
		image_info.extent = {256, 256};
		image_info.array_layers = g_items_info.count();
		image_info.format = VK_FORMAT_R8G8B8A8_SRGB;
		image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		image_info.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		image_info.final_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		image_info.create_view = true;
		image_info.create_sampler = true;
		image_info.sampler_filter = VK_FILTER_NEAREST;

		SingleTimeCommand command_buffer(device, command_pool, graphics_queue);

		item_icon_images = Image(device, physical_device, command_buffer, image_info);
	}

}


void VulkanAPI::createHudDescriptors(
	const Image & image,
	Descriptor & descriptor
)
{
	VkDescriptorSetLayoutBinding sampler_layout_binding = {};
	sampler_layout_binding.binding = 0;
	sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sampler_layout_binding.descriptorCount = 1;
	sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	sampler_layout_binding.pImmutableSamplers = nullptr;

	Descriptor::CreateInfo descriptor_info = {};
	descriptor_info.bindings = { sampler_layout_binding };
	descriptor_info.descriptor_count = static_cast<uint32_t>(max_frames_in_flight);

	descriptor = Descriptor(device, descriptor_info);

	descriptor.update(
		device,
		image.view,
		image.sampler,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);
}

void VulkanAPI::createDescriptors()
{
	{ // Camera descriptor
		VkDescriptorSetLayoutBinding ubo_layout_binding = {};
		ubo_layout_binding.binding = 0;
		ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubo_layout_binding.descriptorCount = 1;
		ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		ubo_layout_binding.pImmutableSamplers = nullptr;

		Descriptor::CreateInfo descriptor_info = {};
		descriptor_info.bindings = { ubo_layout_binding };
		descriptor_info.descriptor_count = static_cast<uint32_t>(max_frames_in_flight);
		descriptor_info.set_count = static_cast<uint32_t>(max_frames_in_flight);

		camera_descriptor = Descriptor(device, descriptor_info);

		for (int i = 0; i < max_frames_in_flight; i++)
		{
			VkDescriptorBufferInfo buffer_info = {};
			buffer_info.buffer = camera_ubo.buffers[i];
			buffer_info.offset = 0;
			buffer_info.range = sizeof(ViewProjMatrices);

			VkWriteDescriptorSet descriptor_write = {};
			descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptor_write.dstSet = camera_descriptor.sets[i];
			descriptor_write.dstBinding = 0;
			descriptor_write.dstArrayElement = 0;
			descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptor_write.descriptorCount = 1;
			descriptor_write.pBufferInfo = &buffer_info;

			vkUpdateDescriptorSets(
				device,
				1,
				&descriptor_write,
				0, nullptr
			);
		}
	}

	{ // Block textures descriptor
		VkDescriptorSetLayoutBinding sampler_layout_binding = {};
		sampler_layout_binding.binding = 0;
		sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		sampler_layout_binding.descriptorCount = 1;
		sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		sampler_layout_binding.pImmutableSamplers = nullptr;

		Descriptor::CreateInfo descriptor_info = {};
		descriptor_info.bindings = { sampler_layout_binding };
		descriptor_info.descriptor_count = static_cast<uint32_t>(max_frames_in_flight);

		block_textures_descriptor = Descriptor(device, descriptor_info);

		block_textures_descriptor.update(
			device,
			block_textures.view,
			block_textures.sampler,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);
	}

	{ // Cube map descriptor
		VkDescriptorSetLayoutBinding sampler_layout_binding = {};
		sampler_layout_binding.binding = 0;
		sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		sampler_layout_binding.descriptorCount = 1;
		sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		sampler_layout_binding.pImmutableSamplers = nullptr;

		Descriptor::CreateInfo descriptor_info = {};
		descriptor_info.bindings = { sampler_layout_binding };
		descriptor_info.descriptor_count = static_cast<uint32_t>(max_frames_in_flight);

		cube_map_descriptor = Descriptor(device, descriptor_info);

		cube_map_descriptor.update(
			device,
			skybox_cube_map.view,
			skybox_cube_map.sampler,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);
	}

	{ // Shadow map descriptor
		VkDescriptorSetLayoutBinding sampler_layout_binding = {};
		sampler_layout_binding.binding = 0;
		sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		sampler_layout_binding.descriptorCount = 1;
		sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		sampler_layout_binding.pImmutableSamplers = nullptr;

		Descriptor::CreateInfo descriptor_info = {};
		descriptor_info.bindings = { sampler_layout_binding };
		descriptor_info.descriptor_count = static_cast<uint32_t>(max_frames_in_flight);

		shadow_map_descriptor = Descriptor(device, descriptor_info);

		shadow_map_descriptor.update(
			device,
			shadow_map_depth_attachement.view,
			shadow_map_depth_attachement.sampler,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);
	}

	{ // Water subpass input attachements descriptor
		VkDescriptorSetLayoutBinding color_attachement_layout_binding = {};
		color_attachement_layout_binding.binding = 0;
		color_attachement_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		color_attachement_layout_binding.descriptorCount = 1;
		color_attachement_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		color_attachement_layout_binding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutBinding depth_attachement_layout_binding = {};
		depth_attachement_layout_binding.binding = 1;
		depth_attachement_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		depth_attachement_layout_binding.descriptorCount = 1;
		depth_attachement_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		depth_attachement_layout_binding.pImmutableSamplers = nullptr;

		Descriptor::CreateInfo descriptor_info = {};
		descriptor_info.bindings = { color_attachement_layout_binding, depth_attachement_layout_binding };
		descriptor_info.descriptor_count = static_cast<uint32_t>(max_frames_in_flight);

		water_renderpass_input_attachement_descriptor = Descriptor(device, descriptor_info);

		VkDescriptorImageInfo color_image_info = {};
		color_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		color_image_info.imageView = color_attachement.view;
		color_image_info.sampler = VK_NULL_HANDLE;

		VkWriteDescriptorSet color_descriptor_write = {};
		color_descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		color_descriptor_write.dstSet = water_renderpass_input_attachement_descriptor.set;
		color_descriptor_write.dstBinding = 0;
		color_descriptor_write.dstArrayElement = 0;
		color_descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		color_descriptor_write.descriptorCount = 1;
		color_descriptor_write.pImageInfo = &color_image_info;

		VkDescriptorImageInfo depth_image_info = {};
		depth_image_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
		depth_image_info.imageView = depth_attachement.view;
		depth_image_info.sampler = VK_NULL_HANDLE;

		VkWriteDescriptorSet depth_descriptor_write = {};
		depth_descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		depth_descriptor_write.dstSet = water_renderpass_input_attachement_descriptor.set;
		depth_descriptor_write.dstBinding = 1;
		depth_descriptor_write.dstArrayElement = 0;
		depth_descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		depth_descriptor_write.descriptorCount = 1;
		depth_descriptor_write.pImageInfo = &depth_image_info;

		std::vector<VkWriteDescriptorSet> descriptor_writes = { color_descriptor_write, depth_descriptor_write };

		vkUpdateDescriptorSets(
			device,
			static_cast<uint32_t>(descriptor_writes.size()),
			descriptor_writes.data(),
			0, nullptr
		);
	}

	{ // Test image descriptor
		VkDescriptorSetLayoutBinding sampler_layout_binding = {};
		sampler_layout_binding.binding = 0;
		sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		sampler_layout_binding.descriptorCount = 1;
		sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		sampler_layout_binding.pImmutableSamplers = nullptr;

		Descriptor::CreateInfo descriptor_info = {};
		descriptor_info.bindings = { sampler_layout_binding };
		descriptor_info.descriptor_count = static_cast<uint32_t>(max_frames_in_flight);

		test_image_descriptor = Descriptor(device, descriptor_info);

		test_image_descriptor.update(
			device,
			shadow_map_depth_attachement.view,
			shadow_map_depth_attachement.sampler,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);
	}

	{ // Hud image descriptor
		createHudDescriptors(crosshair_image, crosshair_image_descriptor);
		createHudDescriptors(debug_info_image, debug_info_image_descriptor);
		createHudDescriptors(toolbar_image, toolbar_image_descriptor);
		createHudDescriptors(toolbar_cursor_image, toolbar_cursor_image_descriptor);
	}

	{ // Player skin image descriptor
		VkDescriptorSetLayoutBinding sampler_layout_binding = {};
		sampler_layout_binding.binding = 0;
		sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		sampler_layout_binding.descriptorCount = 1;
		sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		sampler_layout_binding.pImmutableSamplers = nullptr;

		Descriptor::CreateInfo descriptor_info = {};
		descriptor_info.bindings = { sampler_layout_binding };
		descriptor_info.descriptor_count = static_cast<uint32_t>(max_frames_in_flight);

		player_skin_image_descriptor = Descriptor(device, descriptor_info);

		player_skin_image_descriptor.update(
			device,
			player_skin_image.view,
			player_skin_image.sampler,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);
	}

	{ // Sun matrices descriptor
		VkDescriptorSetLayoutBinding ubo_layout_binding = {};
		ubo_layout_binding.binding = 0;
		ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubo_layout_binding.descriptorCount = 1;
		ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		ubo_layout_binding.pImmutableSamplers = nullptr;

		Descriptor::CreateInfo descriptor_info = {};
		descriptor_info.bindings = { ubo_layout_binding };
		descriptor_info.descriptor_count = static_cast<uint32_t>(max_frames_in_flight);
		descriptor_info.set_count = static_cast<uint32_t>(max_frames_in_flight);

		light_view_proj_descriptor = Descriptor(device, descriptor_info);

		for (int i = 0; i < max_frames_in_flight; i++)
		{
			VkDescriptorBufferInfo buffer_info = {};
			buffer_info.buffer = light_mat_ubo.buffers[i];
			buffer_info.offset = 0;
			buffer_info.range = sizeof(ShadowMapLight);

			VkWriteDescriptorSet descriptor_write = {};
			descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptor_write.dstSet = light_view_proj_descriptor.sets[i];
			descriptor_write.dstBinding = 0;
			descriptor_write.dstArrayElement = 0;
			descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptor_write.descriptorCount = 1;
			descriptor_write.pBufferInfo = &buffer_info;

			vkUpdateDescriptorSets(
				device,
				1,
				&descriptor_write,
				0, nullptr
			);
		}
	}

	{ // Atmosphere descriptor
		VkDescriptorSetLayoutBinding ubo_layout_binding = {};
		ubo_layout_binding.binding = 0;
		ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubo_layout_binding.descriptorCount = 1;
		ubo_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
										| VK_SHADER_STAGE_MISS_BIT_KHR
										| VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		ubo_layout_binding.pImmutableSamplers = nullptr;

		Descriptor::CreateInfo descriptor_info = {};
		descriptor_info.bindings = { ubo_layout_binding };
		descriptor_info.descriptor_count = static_cast<uint32_t>(max_frames_in_flight);
		descriptor_info.set_count = static_cast<uint32_t>(max_frames_in_flight);

		atmosphere_descriptor = Descriptor(device, descriptor_info);

		for (int i = 0; i < max_frames_in_flight; i++)
		{
			VkDescriptorBufferInfo buffer_info = {};
			buffer_info.buffer = atmosphere_ubo.buffers[i];
			buffer_info.offset = 0;
			buffer_info.range = sizeof(AtmosphereParams);

			VkWriteDescriptorSet descriptor_write = {};
			descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptor_write.dstSet = atmosphere_descriptor.sets[i];
			descriptor_write.dstBinding = 0;
			descriptor_write.dstArrayElement = 0;
			descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptor_write.descriptorCount = 1;
			descriptor_write.pBufferInfo = &buffer_info;

			vkUpdateDescriptorSets(
				device,
				1,
				&descriptor_write,
				0, nullptr
			);
		}
	}

	{ // Item icon descriptor
		VkDescriptorSetLayoutBinding sampler_layout_binding = {};
		sampler_layout_binding.binding = 0;
		sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		sampler_layout_binding.descriptorCount = 1;
		sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		sampler_layout_binding.pImmutableSamplers = nullptr;

		Descriptor::CreateInfo descriptor_info = {};
		descriptor_info.bindings = { sampler_layout_binding };
		descriptor_info.descriptor_count = static_cast<uint32_t>(max_frames_in_flight);

		item_icon_descriptor = Descriptor(device, descriptor_info);

		item_icon_descriptor.update(
			device,
			item_icon_images.view,
			item_icon_images.sampler,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);

	}
}

void VulkanAPI::createRenderPass()
{
	{ // lighting render pass
		VkAttachmentDescription color_attachement_description = {};
		color_attachement_description.format = color_attachement.format;
		color_attachement_description.samples = VK_SAMPLE_COUNT_1_BIT;
		color_attachement_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color_attachement_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color_attachement_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attachement_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color_attachement_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		color_attachement_description.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

		VkAttachmentDescription depth_attachement_description = {};
		depth_attachement_description.format = depth_attachement.format;
		depth_attachement_description.samples = VK_SAMPLE_COUNT_1_BIT;
		depth_attachement_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachement_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depth_attachement_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachement_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth_attachement_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depth_attachement_description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription output_attachement_description = {};
		output_attachement_description.format = output_attachement.format;
		output_attachement_description.samples = VK_SAMPLE_COUNT_1_BIT;
		output_attachement_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		output_attachement_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		output_attachement_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		output_attachement_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		output_attachement_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		output_attachement_description.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

		std::vector<VkAttachmentDescription> attachments = {
			color_attachement_description,
			depth_attachement_description,
			output_attachement_description
		};


		VkAttachmentReference color_attachement_ref = {};
		color_attachement_ref.attachment = 0;
		color_attachement_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depth_attachement_ref = {};
		depth_attachement_ref.attachment = 1;
		depth_attachement_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


		VkSubpassDescription opaque_subpass = {};
		opaque_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		opaque_subpass.colorAttachmentCount = 1;
		opaque_subpass.pColorAttachments = &color_attachement_ref;
		opaque_subpass.pDepthStencilAttachment = &depth_attachement_ref;

		std::vector<VkSubpassDescription> subpasses = {
			opaque_subpass
		};


		VkRenderPassCreateInfo render_pass_info = {};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
		render_pass_info.pAttachments = attachments.data();
		render_pass_info.subpassCount = static_cast<uint32_t>(subpasses.size());
		render_pass_info.pSubpasses = subpasses.data();
		render_pass_info.dependencyCount = 0;
		render_pass_info.pDependencies = nullptr;

		VK_CHECK(
			vkCreateRenderPass(device, &render_pass_info, nullptr, &lighting_render_pass),
			"Failed to create render pass"
		);
	}

	{ // shadow render pass
		VkAttachmentDescription depth_attachement_description = {};
		depth_attachement_description.format = shadow_map_depth_attachement.format;
		depth_attachement_description.samples = VK_SAMPLE_COUNT_1_BIT;
		depth_attachement_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachement_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depth_attachement_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachement_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth_attachement_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depth_attachement_description.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentReference depth_attachement_ref = {};
		depth_attachement_ref.attachment = 0;
		depth_attachement_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.pDepthStencilAttachment = &depth_attachement_ref;

		std::vector<VkAttachmentDescription> attachments = {
			depth_attachement_description
		};

		VkRenderPassCreateInfo render_pass_info = {};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
		render_pass_info.pAttachments = attachments.data();
		render_pass_info.subpassCount = 1;
		render_pass_info.pSubpasses = &subpass;

		VK_CHECK(
			vkCreateRenderPass(device, &render_pass_info, nullptr, &shadow_render_pass),
			"Failed to create render pass"
		);
	}

	{ // water render pass
		VkAttachmentDescription color_attachement_description = {};
		color_attachement_description.format = color_attachement.format;
		color_attachement_description.samples = VK_SAMPLE_COUNT_1_BIT;
		color_attachement_description.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		color_attachement_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color_attachement_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attachement_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color_attachement_description.initialLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		color_attachement_description.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription depth_attachement_description = {};
		depth_attachement_description.format = depth_attachement.format;
		depth_attachement_description.samples = VK_SAMPLE_COUNT_1_BIT;
		depth_attachement_description.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		depth_attachement_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depth_attachement_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachement_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth_attachement_description.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depth_attachement_description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription output_attachement_description = {};
		output_attachement_description.format = output_attachement.format;
		output_attachement_description.samples = VK_SAMPLE_COUNT_1_BIT;
		output_attachement_description.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		output_attachement_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		output_attachement_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		output_attachement_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		output_attachement_description.initialLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		output_attachement_description.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		std::vector<VkAttachmentDescription> attachments = {
			color_attachement_description,
			depth_attachement_description,
			output_attachement_description
		};

		VkAttachmentReference input_color_attachement_ref = {};
		input_color_attachement_ref.attachment = 0;
		input_color_attachement_ref.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentReference input_depth_attachement_ref = {};
		input_depth_attachement_ref.attachment = 1;
		input_depth_attachement_ref.layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference output_attachement_ref = {};
		output_attachement_ref.attachment = 2;
		output_attachement_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


		std::vector<VkAttachmentReference> water_subpass_input_attachements = {
			input_color_attachement_ref,
			input_depth_attachement_ref
		};
		VkSubpassDescription water_subpass = {};
		water_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		water_subpass.colorAttachmentCount = 1;
		water_subpass.pColorAttachments = &output_attachement_ref;
		water_subpass.inputAttachmentCount = static_cast<uint32_t>(water_subpass_input_attachements.size());
		water_subpass.pInputAttachments = water_subpass_input_attachements.data();

		std::vector<VkSubpassDescription> subpasses = {
			water_subpass
		};


		VkRenderPassCreateInfo render_pass_info = {};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
		render_pass_info.pAttachments = attachments.data();
		render_pass_info.subpassCount = static_cast<uint32_t>(subpasses.size());
		render_pass_info.pSubpasses = subpasses.data();
		render_pass_info.dependencyCount = 0;
		render_pass_info.pDependencies = nullptr;

		VK_CHECK(
			vkCreateRenderPass(device, &render_pass_info, nullptr, &water_render_pass),
			"Failed to create render pass"
		);
	}

	{ // hud render pass
		VkAttachmentDescription output_attachement_description = {};
		output_attachement_description.format = output_attachement.format;
		output_attachement_description.samples = VK_SAMPLE_COUNT_1_BIT;
		output_attachement_description.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		output_attachement_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		output_attachement_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		output_attachement_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		output_attachement_description.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		output_attachement_description.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

		std::vector<VkAttachmentDescription> attachments = {
			output_attachement_description
		};

		VkAttachmentReference output_attachement_ref = {};
		output_attachement_ref.attachment = 0;
		output_attachement_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


		VkSubpassDescription hud_subpass = {};
		hud_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		hud_subpass.colorAttachmentCount = 1;
		hud_subpass.pColorAttachments = &output_attachement_ref;

		std::vector<VkSubpassDescription> subpasses = {
			hud_subpass
		};


		VkRenderPassCreateInfo render_pass_info = {};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
		render_pass_info.pAttachments = attachments.data();
		render_pass_info.subpassCount = static_cast<uint32_t>(subpasses.size());
		render_pass_info.pSubpasses = subpasses.data();
		render_pass_info.dependencyCount = 0;
		render_pass_info.pDependencies = nullptr;

		VK_CHECK(
			vkCreateRenderPass(device, &render_pass_info, nullptr, &hud_render_pass),
			"Failed to create render pass"
		);
	}

	{ // item icon render pass
		VkAttachmentDescription color_attachement_description = {};
		color_attachement_description.format = item_icon_images.format;
		color_attachement_description.samples = VK_SAMPLE_COUNT_1_BIT;
		color_attachement_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color_attachement_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color_attachement_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attachement_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color_attachement_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		color_attachement_description.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		std::vector<VkAttachmentDescription> attachments = {
			color_attachement_description
		};


		VkAttachmentReference color_attachement_ref = {};
		color_attachement_ref.attachment = 0;
		color_attachement_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_attachement_ref;

		std::vector<VkSubpassDescription> subpasses = {
			subpass
		};


		VkRenderPassCreateInfo render_pass_info = {};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
		render_pass_info.pAttachments = attachments.data();
		render_pass_info.subpassCount = static_cast<uint32_t>(subpasses.size());
		render_pass_info.pSubpasses = subpasses.data();
		render_pass_info.dependencyCount = 0;
		render_pass_info.pDependencies = nullptr;

		VK_CHECK(
			vkCreateRenderPass(device, &render_pass_info, nullptr, &prerender_item_icon_render_pass),
			"Failed to create render pass"
		);
	}
}

void VulkanAPI::createPipelines()
{
	{ // chunk pipeline
		Pipeline::CreateInfo pipeline_info = {};
		pipeline_info.extent = color_attachement.extent2D;
		pipeline_info.vert_path = "shaders/rasterization/chunk_shader.vert.spv";
		pipeline_info.frag_path = "shaders/rasterization/chunk_shader.frag.spv";
		pipeline_info.binding_description = BlockVertex::getBindingDescription();
		pipeline_info.attribute_descriptions = BlockVertex::getAttributeDescriptions();
		pipeline_info.color_formats = { color_attachement.format };
		pipeline_info.depth_format = depth_attachement.format;
		pipeline_info.descriptor_set_layouts = {
			camera_descriptor.layout,
			light_view_proj_descriptor.layout,
			block_textures_descriptor.layout,
			shadow_map_descriptor.layout
		};
		pipeline_info.push_constant_ranges = {
			{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ModelMatrice) }
		};
		pipeline_info.render_pass = lighting_render_pass;

		chunk_pipeline = Pipeline(device, pipeline_info);
	}

	{ // water pipeline
		Pipeline::CreateInfo pipeline_info = {};
		pipeline_info.extent = output_attachement.extent2D;
		pipeline_info.vert_path = "shaders/rasterization/water/water_shader.vert.spv";
		pipeline_info.frag_path = "shaders/rasterization/water/water_shader.frag.spv";
		pipeline_info.binding_description = BlockVertex::getBindingDescription();
		pipeline_info.attribute_descriptions = BlockVertex::getAttributeDescriptions();
		pipeline_info.color_formats = { output_attachement.format };
		// pipeline_info.depth_format = depth_attachement.format;
		pipeline_info.enable_alpha_blending = true;
		pipeline_info.cull_mode = VK_CULL_MODE_NONE;
		pipeline_info.descriptor_set_layouts = {
			camera_descriptor.layout,
			light_view_proj_descriptor.layout,
			block_textures_descriptor.layout,
			shadow_map_descriptor.layout,
			water_renderpass_input_attachement_descriptor.layout
		};
		pipeline_info.push_constant_ranges = {
			{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ModelMatrice) }
		};
		pipeline_info.render_pass = water_render_pass;

		water_pipeline = Pipeline(device, pipeline_info);
	}

	{ // line pipeline
		Pipeline::CreateInfo pipeline_info = {};
		pipeline_info.extent = color_attachement.extent2D;
		pipeline_info.vert_path = "shaders/rasterization/line_shader.vert.spv";
		pipeline_info.frag_path = "shaders/rasterization/line_shader.frag.spv";
		pipeline_info.binding_description = ObjVertex::getBindingDescription();
		const std::vector<VkVertexInputAttributeDescription> attribute_descriptions = ObjVertex::getAttributeDescriptions();
		const std::vector<VkVertexInputAttributeDescription> attribute_descriptions_2 = {
			attribute_descriptions[0]
		};
		pipeline_info.attribute_descriptions = attribute_descriptions_2;
		pipeline_info.polygon_mode = VK_POLYGON_MODE_LINE;
		pipeline_info.color_formats = { color_attachement.format };
		pipeline_info.depth_format = depth_attachement.format;
		pipeline_info.descriptor_set_layouts = {
			camera_descriptor.layout
		};
		pipeline_info.push_constant_ranges = {
			{ VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(LinePipelinePushConstant) }
		};
		pipeline_info.render_pass = lighting_render_pass;
		pipeline_info.dynamic_states = { VK_DYNAMIC_STATE_LINE_WIDTH };

		line_pipeline = Pipeline(device, pipeline_info);
	}

	{ // skybox pipeline
		Pipeline::CreateInfo pipeline_info = {};
		pipeline_info.extent = color_attachement.extent2D;
		pipeline_info.vert_path = "shaders/rasterization/sky/box.vert.spv";
		pipeline_info.frag_path = "shaders/rasterization/sky/box.frag.spv";
		pipeline_info.color_formats = { color_attachement.format };
		pipeline_info.depth_format = depth_attachement.format;
		pipeline_info.descriptor_set_layouts = {
			camera_descriptor.layout,
			cube_map_descriptor.layout
		};
		pipeline_info.push_constant_ranges = {
			{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ModelMatrice) }
		};
		pipeline_info.render_pass = lighting_render_pass;

		skybox_pipeline = Pipeline(device, pipeline_info);
	}

	{ // sun pipeline
		Pipeline::CreateInfo pipeline_info = {};
		pipeline_info.extent = color_attachement.extent2D;
		pipeline_info.vert_path = "shaders/rasterization/sky/sun.vert.spv";
		pipeline_info.frag_path = "shaders/rasterization/sky/sun.frag.spv";
		pipeline_info.binding_description = ObjVertex::getBindingDescription();
		pipeline_info.attribute_descriptions = ObjVertex::getAttributeDescriptions();
		pipeline_info.color_formats = { color_attachement.format };
		pipeline_info.depth_format = depth_attachement.format;
		pipeline_info.descriptor_set_layouts = {
			camera_descriptor.layout,
			atmosphere_descriptor.layout
		};
		pipeline_info.push_constant_ranges = {
			{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ModelMatrice) }
		};
		pipeline_info.render_pass = lighting_render_pass;
		pipeline_info.front_face = VK_FRONT_FACE_CLOCKWISE;

		sun_pipeline = Pipeline(device, pipeline_info);
	}

	{ // shadow map pipeline
		Pipeline::CreateInfo pipeline_info = {};
		pipeline_info.extent = shadow_map_depth_attachement.extent2D;
		pipeline_info.vert_path = "shaders/rasterization/shadow/shadow_shader.vert.spv";
		pipeline_info.geom_path = "shaders/rasterization/shadow/shadow_shader.geom.spv";
		pipeline_info.frag_path = "shaders/rasterization/shadow/shadow_shader.frag.spv";
		pipeline_info.binding_description = BlockVertex::getBindingDescription();
		const std::vector<VkVertexInputAttributeDescription> attribute_descriptions = BlockVertex::getAttributeDescriptions();
		const std::vector<VkVertexInputAttributeDescription> attribute_descriptions_2 = {
			attribute_descriptions[0],
			attribute_descriptions[2],
			attribute_descriptions[3]
		};
		pipeline_info.attribute_descriptions = attribute_descriptions_2;
		pipeline_info.cull_mode = VK_CULL_MODE_NONE;
		pipeline_info.depth_format = shadow_map_depth_attachement.format;
		pipeline_info.depth_bias_enable = VK_TRUE;
		pipeline_info.depth_bias_constant_factor = 0.005f;
		pipeline_info.depth_bias_slope_factor = 0.1f;
		pipeline_info.descriptor_set_layouts = {
			light_view_proj_descriptor.layout,
			block_textures_descriptor.layout
		};
		pipeline_info.push_constant_ranges = {
			{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ModelMatrice) }
		};
		pipeline_info.render_pass = shadow_render_pass;

		shadow_pipeline = Pipeline(device, pipeline_info);
	}

	{ // test image pipeline
		Pipeline::CreateInfo pipeline_info = {};
		// pipeline_info.extent = { output_attachement.extent2D.width / 3, output_attachement.extent2D.height / 3 };
		pipeline_info.extent = { 800, 800 };
		pipeline_info.vert_path = "shaders/rasterization/hud/test_image_shader.vert.spv";
		pipeline_info.frag_path = "shaders/rasterization/hud/test_image_shader.frag.spv";
		pipeline_info.color_formats = { output_attachement.format };
		pipeline_info.depth_format = depth_attachement.format;
		pipeline_info.descriptor_set_layouts = {
			test_image_descriptor.layout
		};
		pipeline_info.push_constant_ranges = {
			{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ModelMatrice) }
		};
		pipeline_info.render_pass = hud_render_pass;

		test_image_pipeline = Pipeline(device, pipeline_info);
	}

	{ // Entity pipeline
		Pipeline::CreateInfo pipeline_info = {};
		pipeline_info.extent = color_attachement.extent2D;
		pipeline_info.vert_path = "shaders/rasterization/entity_shader.vert.spv";
		pipeline_info.frag_path = "shaders/rasterization/entity_shader.frag.spv";
		pipeline_info.binding_description = EntityVertex::getBindingDescription();
		pipeline_info.attribute_descriptions = EntityVertex::getAttributeDescriptions();
		pipeline_info.color_formats = { color_attachement.format };
		pipeline_info.depth_format = depth_attachement.format;
		pipeline_info.descriptor_set_layouts = {
			camera_descriptor.layout
		};
		pipeline_info.push_constant_ranges = {
			{ VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(EntityMatrices) }
		};
		pipeline_info.render_pass = lighting_render_pass;

		entity_pipeline = Pipeline(device, pipeline_info);
	}

	{ // Player pipeline
		Pipeline::CreateInfo pipeline_info = {};
		pipeline_info.extent = color_attachement.extent2D;
		pipeline_info.vert_path = "shaders/rasterization/player_shader.vert.spv";
		pipeline_info.frag_path = "shaders/rasterization/player_shader.frag.spv";
		pipeline_info.binding_description = ObjVertex::getBindingDescription();
		pipeline_info.attribute_descriptions = ObjVertex::getAttributeDescriptions();
		pipeline_info.color_formats = { color_attachement.format };
		pipeline_info.depth_format = depth_attachement.format;
		pipeline_info.descriptor_set_layouts = {
			camera_descriptor.layout,
			player_skin_image_descriptor.layout
		};
		pipeline_info.push_constant_ranges = {
			{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ModelMatrice) }
		};
		pipeline_info.render_pass = lighting_render_pass;

		player_pipeline = Pipeline(device, pipeline_info);
	}

	{ // Hud pipeline
		Pipeline::CreateInfo pipeline_info = {};
		pipeline_info.extent = output_attachement.extent2D;
		pipeline_info.vert_path = "shaders/rasterization/hud/hud_shader.vert.spv";
		pipeline_info.frag_path = "shaders/rasterization/hud/hud_shader.frag.spv";
		pipeline_info.color_formats = { output_attachement.format };
		pipeline_info.descriptor_set_layouts = {
			crosshair_image_descriptor.layout
		};
		pipeline_info.render_pass = hud_render_pass;
		pipeline_info.dynamic_states = { VK_DYNAMIC_STATE_VIEWPORT };
		pipeline_info.enable_alpha_blending = true;

		hud_pipeline = Pipeline(device, pipeline_info);
	}

	{ // Prerender item icon pipeline
		Pipeline::CreateInfo pipeline_info = {};
		pipeline_info.extent = item_icon_images.extent2D;
		pipeline_info.vert_path = "shaders/rasterization/misc/prerender_item_icon_shader.vert.spv";
		pipeline_info.geom_path = "shaders/rasterization/misc/prerender_item_icon_shader.geom.spv";
		pipeline_info.frag_path = "shaders/rasterization/misc/prerender_item_icon_shader.frag.spv";
		pipeline_info.binding_description = ItemVertex::getBindingDescription();
		pipeline_info.attribute_descriptions = ItemVertex::getAttributeDescriptions();
		pipeline_info.color_formats = { item_icon_images.format };
		pipeline_info.descriptor_set_layouts = {
			block_textures_descriptor.layout
		};
		pipeline_info.push_constant_ranges = {
			{ VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(PreRenderItemIconPushConstant) }
		};
		pipeline_info.render_pass = prerender_item_icon_render_pass;
		pipeline_info.front_face = VK_FRONT_FACE_CLOCKWISE;

		prerender_item_icon_pipeline = Pipeline(device, pipeline_info);
	}

	{ // Item icon pipeline
		Pipeline::CreateInfo pipeline_info = {};
		pipeline_info.extent = output_attachement.extent2D;
		pipeline_info.vert_path = "shaders/rasterization/hud/item_icon_shader.vert.spv";
		pipeline_info.frag_path = "shaders/rasterization/hud/item_icon_shader.frag.spv";
		pipeline_info.color_formats = { output_attachement.format };
		pipeline_info.descriptor_set_layouts = {
			item_icon_descriptor.layout
		};
		pipeline_info.push_constant_ranges = {
			{ VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ItemIconPushConstant) }
		};
		pipeline_info.render_pass = hud_render_pass;
		pipeline_info.dynamic_states = { VK_DYNAMIC_STATE_VIEWPORT };
		pipeline_info.enable_alpha_blending = true;

		item_icon_pipeline = Pipeline(device, pipeline_info);
	}
}

void VulkanAPI::createFramebuffers()
{
	lighting_framebuffers.resize(max_frames_in_flight);
	shadow_framebuffers.resize(max_frames_in_flight);
	water_framebuffers.resize(max_frames_in_flight);
	hud_framebuffers.resize(max_frames_in_flight);

	for (int i = 0; i < max_frames_in_flight; i++)
	{
		std::vector<VkImageView> attachments = {
			color_attachement.view,
			depth_attachement.view,
			output_attachement.view
		};

		VkFramebufferCreateInfo framebuffer_info = {};
		framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_info.renderPass = lighting_render_pass;
		framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebuffer_info.pAttachments = attachments.data();
		framebuffer_info.width = color_attachement.extent2D.width;
		framebuffer_info.height = color_attachement.extent2D.height;
		framebuffer_info.layers = 1;

		VK_CHECK(
			vkCreateFramebuffer(device, &framebuffer_info, nullptr, &lighting_framebuffers[i]),
			"Failed to create framebuffer"
		);
	}

	for (int i = 0; i < max_frames_in_flight; i++)
	{
		std::vector<VkImageView> attachments = {
			shadow_map_depth_attachement.view
		};

		VkFramebufferCreateInfo framebuffer_info = {};
		framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_info.renderPass = shadow_render_pass;
		framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebuffer_info.pAttachments = attachments.data();
		framebuffer_info.width = shadow_map_depth_attachement.extent2D.width;
		framebuffer_info.height = shadow_map_depth_attachement.extent2D.height;
		framebuffer_info.layers = shadow_maps_count;

		VK_CHECK(
			vkCreateFramebuffer(device, &framebuffer_info, nullptr, &shadow_framebuffers[i]),
			"Failed to create framebuffer"
		);
	}

	for (int i = 0; i < max_frames_in_flight; i++)
	{
		std::vector<VkImageView> attachments = {
			color_attachement.view,
			depth_attachement.view,
			output_attachement.view
		};

		VkFramebufferCreateInfo framebuffer_info = {};
		framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_info.renderPass = water_render_pass;
		framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebuffer_info.pAttachments = attachments.data();
		framebuffer_info.width = output_attachement.extent2D.width;
		framebuffer_info.height = output_attachement.extent2D.height;
		framebuffer_info.layers = 1;

		VK_CHECK(
			vkCreateFramebuffer(device, &framebuffer_info, nullptr, &water_framebuffers[i]),
			"Failed to create framebuffer"
		);
	}

	for (int i = 0; i < max_frames_in_flight; i++)
	{
		std::vector<VkImageView> attachments = {
			output_attachement.view
		};

		VkFramebufferCreateInfo framebuffer_info = {};
		framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_info.renderPass = hud_render_pass;
		framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebuffer_info.pAttachments = attachments.data();
		framebuffer_info.width = output_attachement.extent2D.width;
		framebuffer_info.height = output_attachement.extent2D.height;
		framebuffer_info.layers = 1;

		VK_CHECK(
			vkCreateFramebuffer(device, &framebuffer_info, nullptr, &hud_framebuffers[i]),
			"Failed to create framebuffer"
		);
	}

	{ // item icon framebuffer
		std::vector<VkImageView> attachments = {
			item_icon_images.view
		};

		VkFramebufferCreateInfo framebuffer_info = {};
		framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_info.renderPass = prerender_item_icon_render_pass;
		framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebuffer_info.pAttachments = attachments.data();
		framebuffer_info.width = item_icon_images.extent2D.width;
		framebuffer_info.height = item_icon_images.extent2D.height;
		framebuffer_info.layers = item_icon_images.array_layers;

		VK_CHECK(
			vkCreateFramebuffer(device, &framebuffer_info, nullptr, &prerender_item_icon_framebuffer),
			"Failed to create framebuffer"
		);
	}
}

void VulkanAPI::createMeshes()
{
	{ // load cube mesh
		ObjLoader obj_loader("assets/models/cube.obj");

		cube_mesh_id = storeMesh(
			obj_loader.vertices().data(),
			obj_loader.vertices().size(),
			sizeof(ObjVertex),
			obj_loader.indices().data(),
			obj_loader.indices().size()
		);
	}

	{ // load icosphere mesh
		ObjLoader obj_loader("assets/models/icosphere.obj");
		icosphere_mesh_id = storeMesh(
			obj_loader.vertices().data(),
			obj_loader.vertices().size(),
			sizeof(ObjVertex),
			obj_loader.indices().data(),
			obj_loader.indices().size()
		);
	}

	{ // load player mesh
		ObjLoader chest_geometry("assets/models/entity/player/chest.obj");
		player_chest_mesh_id = storeMesh(
			chest_geometry.vertices().data(),
			chest_geometry.vertices().size(),
			sizeof(ObjVertex),
			chest_geometry.indices().data(),
			chest_geometry.indices().size()
		);

		ObjLoader head_geometry("assets/models/entity/player/head.obj");
		player_head_mesh_id = storeMesh(
			head_geometry.vertices().data(),
			head_geometry.vertices().size(),
			sizeof(ObjVertex),
			head_geometry.indices().data(),
			head_geometry.indices().size()
		);

		ObjLoader right_leg_geometry("assets/models/entity/player/right_leg.obj");
		player_right_leg_mesh_id = storeMesh(
			right_leg_geometry.vertices().data(),
			right_leg_geometry.vertices().size(),
			sizeof(ObjVertex),
			right_leg_geometry.indices().data(),
			right_leg_geometry.indices().size()
		);

		ObjLoader left_leg_geometry("assets/models/entity/player/left_leg.obj");
		player_left_leg_mesh_id = storeMesh(
			left_leg_geometry.vertices().data(),
			left_leg_geometry.vertices().size(),
			sizeof(ObjVertex),
			left_leg_geometry.indices().data(),
			left_leg_geometry.indices().size()
		);

		ObjLoader right_arm_geometry("assets/models/entity/player/right_arm.obj");
		player_right_arm_mesh_id = storeMesh(
			right_arm_geometry.vertices().data(),
			right_arm_geometry.vertices().size(),
			sizeof(ObjVertex),
			right_arm_geometry.indices().data(),
			right_arm_geometry.indices().size()
		);

		ObjLoader left_arm_geometry("assets/models/entity/player/left_arm.obj");
		player_left_arm_mesh_id = storeMesh(
			left_arm_geometry.vertices().data(),
			left_arm_geometry.vertices().size(),
			sizeof(ObjVertex),
			left_arm_geometry.indices().data(),
			left_arm_geometry.indices().size()
		);
	}
}


void VulkanAPI::setupTextRenderer()
{
	text_renderer.initialize();
}

void VulkanAPI::destroyTextRenderer()
{
	text_renderer.destroy();
}

void VulkanAPI::writeTextToDebugImage(
	VkCommandBuffer command_buffer,
	const std::string & text,
	const uint32_t x,
	const uint32_t y,
	const uint32_t font_size
)
{
	const VkDeviceSize image_size = debug_info_image.extent2D.width * debug_info_image.extent2D.height * 4;

	Buffer & staging_buffer = debug_info_buffers[current_frame];

	memset(staging_buffer.mappedMemory(), 0, image_size);

	text_renderer.renderText(
		text,
		x, y,
		font_size,
		staging_buffer.mappedMemory(),
		debug_info_image.extent2D.width,
		debug_info_image.extent2D.height
	);

	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = { debug_info_image.extent2D.width, debug_info_image.extent2D.height, 1 };

	debug_info_image.transitionLayout(
		command_buffer,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	);

	vkCmdCopyBufferToImage(
		command_buffer,
		staging_buffer.buffer,
		debug_info_image.image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region
	);

	debug_info_image.transitionLayout(
		command_buffer,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);
}


uint64_t VulkanAPI::createImGuiTexture(const uint32_t width, const uint32_t height)
{
	ImGuiTexture imgui_texture;

	imgui_texture.extent = { width, height };
	imgui_texture.format = VK_FORMAT_R8G8B8A8_SRGB;

	createImage(
		imgui_texture.extent.width,
		imgui_texture.extent.height,
		1,
		imgui_texture.format,
		VK_IMAGE_TILING_LINEAR,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		imgui_texture.image,
		imgui_texture.memory
	);

	createImageView(
		imgui_texture.image,
		imgui_texture.format,
		VK_IMAGE_ASPECT_COLOR_BIT,
		imgui_texture.view
	);

	VkSamplerCreateInfo sampler_info = {};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.anisotropyEnable = VK_FALSE;
	sampler_info.maxAnisotropy = 1.0f;
	sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	sampler_info.unnormalizedCoordinates = VK_FALSE;
	sampler_info.compareEnable = VK_FALSE;
	sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_info.mipLodBias = 0.0f;
	sampler_info.minLod = 0.0f;
	sampler_info.maxLod = 0.0f;

	VK_CHECK(
		vkCreateSampler(device, &sampler_info, nullptr, &imgui_texture.sampler),
		"Failed to create imgui texture sampler"
	);

	imgui_texture.descriptor_set = ImGui_ImplVulkan_AddTexture(
		imgui_texture.sampler,
		imgui_texture.view,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);

	transitionImageLayout(
		imgui_texture.image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_ASPECT_COLOR_BIT,
		1,
		0,
		0,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT
	);

	VK_CHECK(
		vkMapMemory(device, imgui_texture.memory, 0, VK_WHOLE_SIZE, 0, &imgui_texture.mapped_memory),
		"Failed to map memory for ImGui texture."
	);

	const uint64_t id = next_imgui_texture_id++;
	{
		std::lock_guard lock(imgui_textures_mutex);
		imgui_textures[id] = imgui_texture;
	}

	return id;
}

void VulkanAPI::destroyImGuiTextures()
{
	std::lock_guard lock(imgui_textures_mutex);
	for (auto & [id, texture] : imgui_textures)
	{
		vkUnmapMemory(device, texture.memory);
		ImGui_ImplVulkan_RemoveTexture(texture.descriptor_set);
		vkDestroySampler(device, texture.sampler, nullptr);
		vkDestroyImageView(device, texture.view, nullptr);
		VulkanMemoryAllocator & vma = VulkanMemoryAllocator::getInstance();
		vma.freeMemory(device, texture.memory, nullptr);
		vkDestroyImage(device, texture.image, nullptr);
	}
}

void VulkanAPI::ImGuiTexturePutPixel(
	const uint64_t texture_id,
	const uint32_t x,
	const uint32_t y,
	const uint8_t r,
	const uint8_t g,
	const uint8_t b,
	const uint8_t a
)
{
	std::lock_guard lock(imgui_textures_mutex);
	ImGuiTexture & imgui_texture = imgui_textures.at(texture_id);
	imgui_texture.putPixel(x, y, r, g, b, a);
}

void VulkanAPI::ImGuiTextureClear(const uint64_t texture_id)
{
	std::lock_guard lock(imgui_textures_mutex);
	ImGuiTexture & imgui_texture = imgui_textures.at(texture_id);
	imgui_texture.clear();
}

void VulkanAPI::ImGuiTextureDraw(const uint64_t texture_id)
{
	std::lock_guard lock(imgui_textures_mutex);
	ImGuiTexture & imgui_texture = imgui_textures.at(texture_id);
	ImGui::Image(
		(ImTextureID)imgui_texture.descriptor_set,
		ImVec2(imgui_texture.width(), imgui_texture.height())
	);
}


void VulkanAPI::setupImgui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO & io = ImGui::GetIO();
	(void)io;

	ImGui::StyleColorsDark();

	std::vector<VkDescriptorPoolSize> pool_sizes =
	{
		{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
		{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
		{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
	};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
	pool_info.pPoolSizes = pool_sizes.data();
	pool_info.maxSets = 1000;

	VK_CHECK(
		vkCreateDescriptorPool(device, &pool_info, nullptr, &imgui_descriptor_pool),
		"Failed to create imgui descriptor pool"
	);

	ImGui_ImplGlfw_InitForVulkan(window, true);
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = instance;
	init_info.PhysicalDevice = physical_device;
	init_info.Device = device;
	init_info.QueueFamily = queue_family_indices.graphics_family.value();
	init_info.Queue = graphics_queue;
	init_info.PipelineCache = VK_NULL_HANDLE;
	init_info.DescriptorPool = imgui_descriptor_pool;
	init_info.Allocator = nullptr;
	init_info.MinImageCount = 2;
	init_info.ImageCount = static_cast<uint32_t>(swapchain.images.size());
	init_info.UseDynamicRendering = VK_TRUE;
	init_info.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchain.image_format;

	ImGui_ImplVulkan_Init(&init_info);
}

void VulkanAPI::setupTracy()
{
	{
		const char * const ctx_name = "Draw context";
		(void)ctx_name;
		draw_ctx = TracyVkContextCalibrated(
			physical_device,
			device,
			graphics_queue,
			draw_command_buffers[0],
			vkGetPhysicalDeviceCalibrateableTimeDomainsEXT,
			vkGetCalibratedTimestampsEXT
		);
		TracyVkContextName(draw_ctx, ctx_name, strlen(ctx_name));
	}

	{
		const char * const ctx_name = "Copy to swapchain context";
		(void)ctx_name;
		copy_to_swapchain_ctx = TracyVkContextCalibrated(
			physical_device,
			device,
			graphics_queue,
			draw_command_buffers[0],
			vkGetPhysicalDeviceCalibrateableTimeDomainsEXT,
			vkGetCalibratedTimestampsEXT
		);
		TracyVkContextName(copy_to_swapchain_ctx, ctx_name, strlen(ctx_name));
	}

	{
		const char * const ctx_name = "Imgui context";
		(void)ctx_name;
		imgui_ctx = TracyVkContextCalibrated(
			physical_device,
			device,
			graphics_queue,
			draw_command_buffers[0],
			vkGetPhysicalDeviceCalibrateableTimeDomainsEXT,
			vkGetCalibratedTimestampsEXT
		);
		TracyVkContextName(imgui_ctx, ctx_name, strlen(ctx_name));
	}
}

void VulkanAPI::destroyTracy()
{
	TracyVkDestroy(draw_ctx);
	TracyVkDestroy(copy_to_swapchain_ctx);
	TracyVkDestroy(imgui_ctx);
}


void VulkanAPI::prerenderItemIconImages()
{
	SingleTimeCommand command_buffer(device, command_pool, graphics_queue);

	VkRenderPassBeginInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = prerender_item_icon_render_pass;
	render_pass_info.framebuffer = prerender_item_icon_framebuffer;
	render_pass_info.renderArea.offset = { 0, 0 };
	render_pass_info.renderArea.extent = item_icon_images.extent2D;
	VkClearValue clear_color = { 0.0f, 0.0f, 0.0f, 0.0f };
	render_pass_info.clearValueCount = 1;
	render_pass_info.pClearValues = &clear_color;

	vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, prerender_item_icon_pipeline.pipeline);

	std::vector<VkDescriptorSet> descriptor_sets = { block_textures_descriptor.set };
	vkCmdBindDescriptorSets(
		command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		prerender_item_icon_pipeline.layout,
		0,
		static_cast<uint32_t>(descriptor_sets.size()),
		descriptor_sets.data(),
		0,
		nullptr
	);

	// should probably fit the model bounding box or something like that
	const glm::mat4 clip = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.5f, 0.0f,
		0.0f, 0.0f, 0.5f, 1.0f
	};
	glm::mat4 proj = glm::ortho(
		-1.0f, 1.0f,
		-1.0f, 1.0f,
		0.0f, 5.0f
	);
	glm::mat4 view = glm::lookAt(
		glm::vec3(2.0f, 2.0f, 2.0f),
		glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f)
	);
	glm::mat4 model = glm::mat4(1.0f);

	PreRenderItemIconPushConstant push_constants = {};
	push_constants.MVP = clip * proj * view * model;

	for (size_t i = 0; i < g_items_info.count(); i++)
	{
		push_constants.layer = i;

		drawMesh(
			command_buffer,
			prerender_item_icon_pipeline,
			g_items_info.get(i).mesh_id,
			&push_constants,
			sizeof(PreRenderItemIconPushConstant),
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT
		);
	}

	vkCmdEndRenderPass(command_buffer);

	command_buffer.end();
}


VkCommandBuffer VulkanAPI::beginSingleTimeCommands()
{
	VK_CHECK(
		vkResetFences(device, 1, &single_time_command_fence),
		"Failed to reset single time command buffer fence"
	);

	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandPool = command_pool;
	alloc_info.commandBufferCount = 1;

	VkCommandBuffer command_buffer;
	VK_CHECK(
		vkAllocateCommandBuffers(device, &alloc_info, &command_buffer),
		"Failed to allocate single time command buffer"
	);

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK(
		vkBeginCommandBuffer(command_buffer, &begin_info),
		"Failed to begin single time command buffer"
	);

	return command_buffer;
}

void VulkanAPI::endSingleTimeCommands(VkCommandBuffer command_buffer)
{
	VK_CHECK(
		vkEndCommandBuffer(command_buffer),
		"Failed to end single time command buffer"
	);


	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;

	VK_CHECK(
		vkQueueSubmit(graphics_queue, 1, &submit_info, single_time_command_fence),
		"Failed to submit single time command buffer"
	);
	VK_CHECK(
		vkWaitForFences(device, 1, &single_time_command_fence, VK_TRUE, UINT64_MAX),
		"Failed to wait for single time command buffer"
	);

	vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
}


void VulkanAPI::drawMesh(
	VkCommandBuffer command_buffer,
	const Pipeline & pipeline,
	const uint64_t mesh_id,
	const void * push_constants,
	const uint32_t push_constants_size,
	VkShaderStageFlags push_constants_stage
)
{
	Mesh mesh;
	{
		std::lock_guard lock(mesh_map_mutex);
		if (!mesh_map.contains(mesh_id))
		{
			LOG_WARNING("Mesh " << mesh_id << " not found in the mesh map.");
			return;
		}

		mesh = mesh_map.at(mesh_id);

		if (mesh.buffer == VK_NULL_HANDLE)
		{
			LOG_WARNING("Mesh " << mesh_id << " has a null buffer.");
			return;
		}

		mesh_map.at(mesh_id).used_by_frame[current_frame] = true;
	}

	const VkBuffer vertex_buffers[] = { mesh.buffer };
	const VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(
		command_buffer,
		0, 1,
		vertex_buffers,
		offsets
	);

	vkCmdBindIndexBuffer(
		command_buffer,
		mesh.buffer,
		mesh.index_offset,
		VK_INDEX_TYPE_UINT32
	);

	if (push_constants_size > 0)
	{
		vkCmdPushConstants(
			command_buffer,
			pipeline.layout,
			push_constants_stage,
			0,
			push_constants_size,
			push_constants
		);
	}

	vkCmdDrawIndexed(
		command_buffer,
		static_cast<uint32_t>(mesh.index_count),
		1, 0, 0, 0
	);
}

void VulkanAPI::drawHudImage(
	const Descriptor & descriptor,
	const VkViewport & viewport
)
{
	vkCmdBindDescriptorSets(
		draw_command_buffers[current_frame],
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		hud_pipeline.layout,
		0,
		1,
		descriptor.sets.data(),
		0,
		nullptr
	);

	vkCmdSetViewport(draw_command_buffers[current_frame], 0, 1, &viewport);

	vkCmdDraw(
		draw_command_buffers[current_frame],
		6,
		1,
		0,
		0
	);
}

void VulkanAPI::drawItemIcon(
	const VkViewport & viewport,
	const uint32_t layer
)
{
	ItemIconPushConstant push_constants = {};
	push_constants.layer = layer;

	vkCmdPushConstants(
		draw_command_buffers[current_frame],
		item_icon_pipeline.layout,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		0,
		sizeof(ItemIconPushConstant),
		&push_constants
	);

	vkCmdSetViewport(draw_command_buffers[current_frame], 0, 1, &viewport);

	vkCmdDraw(
		draw_command_buffers[current_frame],
		6,
		1,
		0,
		0
	);
}


VkFormat VulkanAPI::findSupportedFormat(
	const std::vector<VkFormat> & candidates,
	VkImageTiling tiling,
	VkFormatFeatureFlags features
)
{
	for (VkFormat format : candidates)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(physical_device, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
		{
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
		{
			return format;
		}
	}

	throw std::runtime_error("Failed to find supported format");
}

void VulkanAPI::createImage(
	uint32_t width,
	uint32_t height,
	uint32_t mip_levels,
	VkFormat format,
	VkImageTiling tiling,
	VkImageUsageFlags usage,
	VkMemoryPropertyFlags properties,
	VkImage & image,
	VkDeviceMemory & image_memory
)
{
	VkImageCreateInfo image_info = {};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.extent.width = width;
	image_info.extent.height = height;
	image_info.extent.depth = 1;
	image_info.mipLevels = mip_levels;
	image_info.arrayLayers = 1;
	image_info.format = format;
	image_info.tiling = tiling;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_info.usage = usage;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CHECK(
		vkCreateImage(device, &image_info, nullptr, &image),
		"Failed to create image"
	);

	VkMemoryRequirements mem_requirements;
	vkGetImageMemoryRequirements(device, image, &mem_requirements);

	VkMemoryAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_requirements.size;
	alloc_info.memoryTypeIndex = vk_helper::findMemoryType(physical_device, mem_requirements.memoryTypeBits, properties);

	VulkanMemoryAllocator & vma = VulkanMemoryAllocator::getInstance();
	VK_CHECK(
		vma.allocateMemory(device, &alloc_info, nullptr, &image_memory),
		"Failed to allocate image memory"
	);

	vkBindImageMemory(device, image, image_memory, 0);
}

void VulkanAPI::createImageView(
	VkImage image,
	VkFormat format,
	VkImageAspectFlags aspect_flags,
	VkImageView & image_view
)
{
	VkImageViewCreateInfo view_info = {};
	view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_info.image = image;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.format = format;
	view_info.subresourceRange.aspectMask = aspect_flags;
	view_info.subresourceRange.baseMipLevel = 0;
	view_info.subresourceRange.levelCount = 1;
	view_info.subresourceRange.baseArrayLayer = 0;
	view_info.subresourceRange.layerCount = 1;

	VK_CHECK(
		vkCreateImageView(device, &view_info, nullptr, &image_view),
		"Failed to create image view"
	);
}

void VulkanAPI::transitionImageLayout(
	VkImage image,
	VkImageLayout old_layout,
	VkImageLayout new_layout,
	VkImageAspectFlags aspect_mask,
	uint32_t mip_levels,
	VkAccessFlags src_access_mask,
	VkAccessFlags dst_access_mask,
	VkPipelineStageFlags src_stage_mask,
	VkPipelineStageFlags dst_stage_mask
)
{
	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandPool = command_pool;
	alloc_info.commandBufferCount = 1;

	VkCommandBuffer command_buffer;
	VK_CHECK(
		vkAllocateCommandBuffers(device, &alloc_info, &command_buffer),
		"Failed to allocate command buffers"
	);

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK(
		vkBeginCommandBuffer(command_buffer, &begin_info),
		"Failed to begin recording command buffer"
	);

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = aspect_mask;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mip_levels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = src_access_mask;
	barrier.dstAccessMask = dst_access_mask;

	vkCmdPipelineBarrier(
		command_buffer,
		src_stage_mask,
		dst_stage_mask,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&barrier
	);

	VK_CHECK(
		vkEndCommandBuffer(command_buffer),
		"Failed to end recording command buffer"
	);

	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;

	VK_CHECK(
		vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE),
		"Failed to submit queue 1"
	);

	VK_CHECK(
		vkQueueWaitIdle(graphics_queue),
		"Failed to wait for queue"
	);

	vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
}

void VulkanAPI::setImageLayout(
	VkCommandBuffer command_buffer,
	VkImage image,
	VkImageLayout old_layout,
	VkImageLayout new_layout,
	VkImageSubresourceRange subresource_range,
	VkAccessFlags srcAccessMask,
	VkAccessFlags dstAccessMask,
	VkPipelineStageFlags srcStageMask,
	VkPipelineStageFlags dstStageMask
)
{
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange = subresource_range;
	barrier.srcAccessMask = srcAccessMask;
	barrier.dstAccessMask = dstAccessMask;

	vkCmdPipelineBarrier(
		command_buffer,
		srcStageMask,
		dstStageMask,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&barrier
	);
}

void VulkanAPI::createBuffer(
	VkDeviceSize size,
	VkBufferUsageFlags usage,
	VkMemoryPropertyFlags properties,
	VkBuffer & buffer,
	VkDeviceMemory & buffer_memory
)
{
	VkBufferCreateInfo buffer_info = {};
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.size = size;
	buffer_info.usage = usage;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CHECK(
		vkCreateBuffer(device, &buffer_info, nullptr, &buffer),
		"Failed to create buffer"
	);

	VkMemoryRequirements mem_requirements;
	vkGetBufferMemoryRequirements(device, buffer, &mem_requirements);

	VkMemoryAllocateFlagsInfo memory_allocate_flags_info = {};
	memory_allocate_flags_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;

	if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
	{
		memory_allocate_flags_info.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
	}

	VkMemoryAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_requirements.size;
	alloc_info.memoryTypeIndex = vk_helper::findMemoryType(physical_device, mem_requirements.memoryTypeBits, properties);
	alloc_info.pNext = &memory_allocate_flags_info;

	VulkanMemoryAllocator & vma = VulkanMemoryAllocator::getInstance();
	VK_CHECK(
		vma.allocateMemory(device, &alloc_info, nullptr, &buffer_memory),
		"Failed to allocate buffer memory"
	);

	vkBindBufferMemory(device, buffer, buffer_memory, 0);
}

void VulkanAPI::copyBuffer(
	VkBuffer src_buffer,
	VkBuffer dst_buffer,
	VkDeviceSize size
)
{
	// VkCommandBufferAllocateInfo alloc_info = {};
	// alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	// alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	// alloc_info.commandPool = command_pool;
	// alloc_info.commandBufferCount = 1;

	// VkCommandBuffer command_buffer;
	// VK_CHECK(
	// 	vkAllocateCommandBuffers(device, &alloc_info, &command_buffer),
	// 	"Failed to allocate command buffers"
	// );

	std::unique_lock lock(transfer_operation_mutex);

	vkResetCommandBuffer(transfer_command_buffers, 0);

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK(
		vkBeginCommandBuffer(transfer_command_buffers, &begin_info),
		"Failed to begin recording command buffer"
	);

	VkBufferCopy copy_region = {};
	copy_region.size = size;

	vkCmdCopyBuffer(transfer_command_buffers, src_buffer, dst_buffer, 1, &copy_region);

	VK_CHECK(
		vkEndCommandBuffer(transfer_command_buffers),
		"Failed to end recording command buffer"
	);

	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &transfer_command_buffers;

	VK_CHECK(
		vkQueueSubmit(transfer_queue, 1, &submit_info, VK_NULL_HANDLE),
		"Failed to submit queue 2"
	);

	VK_CHECK(
		vkQueueWaitIdle(transfer_queue),
		"Failed to wait for queue"
	);

	// vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
}

void VulkanAPI::copyBufferToImage(
	VkBuffer buffer,
	VkImage image,
	uint32_t width,
	uint32_t height
)
{
	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandPool = command_pool;
	alloc_info.commandBufferCount = 1;

	VkCommandBuffer command_buffer;
	VK_CHECK(
		vkAllocateCommandBuffers(device, &alloc_info, &command_buffer),
		"Failed to allocate command buffers"
	);

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK(
		vkBeginCommandBuffer(command_buffer, &begin_info),
		"Failed to begin recording command buffer"
	);

	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = {0, 0, 0};
	region.imageExtent = {width, height, 1};

	vkCmdCopyBufferToImage(
		command_buffer,
		buffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region
	);

	VK_CHECK(
		vkEndCommandBuffer(command_buffer),
		"Failed to end recording command buffer"
	);

	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;

	VK_CHECK(
		vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE),
		"Failed to submit queue 3"
	);

	VK_CHECK(
		vkQueueWaitIdle(graphics_queue),
		"Failed to wait for queue"
	);

	vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
}

