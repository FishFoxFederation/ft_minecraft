#include "application.hpp"
#include "logger.hpp"

#include <iostream>

Application::Application(const int & player_id, const std::string & ip_address, const int & port)
:
	m_start_time(std::chrono::steady_clock::now().time_since_epoch()),
	m_client(ip_address, port),
	m_settings(),
	m_world_scene(),
	m_window("Vox", 800, 600),
	m_vulkan_api(m_window.getGLFWwindow()),
	m_thread_pool(),
	m_world(m_world_scene, m_vulkan_api, m_thread_pool, player_id),
	m_render_thread(m_settings, m_vulkan_api, m_world_scene, m_start_time),
	m_update_thread(m_client, m_settings, m_window, m_world_scene, m_world, m_vulkan_api, m_start_time),
	m_block_update_thread(m_world_scene, m_world)
	// m_network_thread(m_client)
{
	LOG_INFO("Application::Application()");
}

Application::~Application()
{
	LOG_INFO("Application::~Application()");
}

void Application::run()
{
	LOG_INFO("Application::run()");

	while (!m_window.shouldClose())
	{
		glfwWaitEvents();
	}
}
