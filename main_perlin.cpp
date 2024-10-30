#include "World.hpp"
#include "DebugGui.hpp"
#include "VulkanAPI.hpp"
#include "RenderThread.hpp"
#include "window.hpp"
#include "math_utils.hpp"


int main()
{

	World world;
	WorldScene world_scene;
	Window window("Vox", 800, 600);
	Settings settings;
	VulkanAPI vulkan_api(window.getGLFWwindow());
	RenderThread render_thread(settings, vulkan_api, world_scene, std::chrono::steady_clock::now().time_since_epoch());

	//fill all perlin
	world.getWorldGenerator().drawNoises(vulkan_api);

	while (!window.shouldClose())
	{
		glfwWaitEvents();
	}
}
