#include "UpdateThread.hpp"
#include "logger.hpp"
#include "DebugGui.hpp"

#include "Tracy.hpp"

#include <unistd.h>

UpdateThread::UpdateThread(
	Client & client,
	const Settings & settings,
	Window & window,
	WorldScene & world_scene,
	ClientWorld & world,
	VulkanAPI & vulkan_api,
	Sound::Engine & sound_engine,
	Event::Manager & event_manager,
	std::chrono::nanoseconds start_time
):
	m_settings(settings),
	m_window(window),
	m_world_scene(world_scene),
	m_world(world),
	m_vulkan_api(vulkan_api),
	m_client(client),
	m_packet_handler(client, world),
	m_sound_engine(sound_engine),
	m_event_manager(event_manager),
	m_start_time(start_time),
	m_last_frame_time(start_time),
	m_update_count(0),
	m_start_time_counting_ups(start_time),
	m_thread(&UpdateThread::launch, this)
{
	(void)m_vulkan_api;
	(void)m_start_time;
}

UpdateThread::~UpdateThread()
{
	this->m_thread.request_stop();
	this->m_thread.join();
}

void UpdateThread::launch()
{
	try
	{
		init();

		while (!m_thread.get_stop_token().stop_requested())
		{
			loop();
		}
	}
	catch (const std::exception & e)
	{
		LOG_ERROR("UPDATE THREAD Thread exception: " << e.what());
	}
	LOG_DEBUG("Thread stopped");
}

void UpdateThread::init()
{
	LOG_INFO("UpdateThread launched :" << gettid());
	tracy::SetThreadName(str_update_thread);
	auto packet = std::make_shared<ConnectionPacket>(m_world.m_my_player_id, m_world.getPlayerPosition(m_world.m_my_player_id));
	m_client.sendPacket(packet);

	// m_world.createMob();
}

void UpdateThread::loop()
{
	ZoneScoped;
	m_client.runOnce(1);
	updateTime();
	readInput();
	//only move player every 20 ms
	static auto last_move = std::chrono::steady_clock::now();
	if (std::chrono::steady_clock::now() - last_move > std::chrono::milliseconds(20))
	{
		movePlayer();
		last_move = std::chrono::steady_clock::now();
	}
	handlePackets();
	m_world.updateBlock(m_world.getPlayerPosition(m_world.m_my_player_id));

	m_world.updateMobs(m_delta_time.count() / 1e9);

	{ // Update walk animation
		std::lock_guard lock(m_world_scene.m_player_mutex);
		for (auto & [id, player] : m_world_scene.m_players)
		{
			player.walk_animation.update();
			player.attack_animation.update();
		}
	}

	m_world.otherUpdate();
}

void UpdateThread::updateTime()
{
	m_current_time = std::chrono::steady_clock::now().time_since_epoch();
	m_delta_time = m_current_time - m_last_frame_time;
	m_last_frame_time = m_current_time;

	m_update_count++;
	if (m_current_time - m_start_time_counting_ups >= std::chrono::seconds(1))
	{
		DebugGui::ups = static_cast<double>(m_update_count) / std::chrono::duration_cast<std::chrono::seconds>(m_current_time - m_start_time_counting_ups).count();
		m_update_count = 0;
		m_start_time_counting_ups = m_current_time;
	}
}

void UpdateThread::readInput()
{
	const Input::KeyState move_forward_key_status = m_window.input().getKeyState(GLFW_KEY_W);
	const Input::KeyState move_left_key_status = m_window.input().getKeyState(GLFW_KEY_A);
	const Input::KeyState move_backward_key_status = m_window.input().getKeyState(GLFW_KEY_S);
	const Input::KeyState move_right_key_status = m_window.input().getKeyState(GLFW_KEY_D);
	const Input::KeyState jump_key_status = m_window.input().getKeyState(GLFW_KEY_SPACE);
	const Input::KeyState sneak_key_status = m_window.input().getKeyState(GLFW_KEY_LEFT_SHIFT);
	const Input::KeyState attack_key_status = m_window.input().getMouseButtonState(GLFW_MOUSE_BUTTON_LEFT);
	const Input::KeyState use_key_status = m_window.input().getMouseButtonState(GLFW_MOUSE_BUTTON_RIGHT);

	if (move_forward_key_status == Input::KeyState::PRESSED) m_move_forward = 1;
	else if (move_forward_key_status == Input::KeyState::RELEASED) m_move_forward = 0;

	if (move_left_key_status == Input::KeyState::PRESSED) m_move_left = 1;
	else if (move_left_key_status == Input::KeyState::RELEASED) m_move_left = 0;

	if (move_backward_key_status == Input::KeyState::PRESSED) m_move_backward = 1;
	else if (move_backward_key_status == Input::KeyState::RELEASED) m_move_backward = 0;

	if (move_right_key_status == Input::KeyState::PRESSED) m_move_right = 1;
	else if (move_right_key_status == Input::KeyState::RELEASED) m_move_right = 0;

	if (jump_key_status == Input::KeyState::PRESSED) m_jump = 1;
	else if (jump_key_status == Input::KeyState::RELEASED) m_jump = 0;

	if (sneak_key_status == Input::KeyState::PRESSED) m_sneak = 1;
	else if (sneak_key_status == Input::KeyState::RELEASED) m_sneak = 0;

	if (attack_key_status == Input::KeyState::PRESSED) m_attack = 1;
	else if (attack_key_status == Input::KeyState::RELEASED) m_attack = 0;

	if (use_key_status == Input::KeyState::PRESSED) m_use = 1;
	else if (use_key_status == Input::KeyState::RELEASED) m_use = 0;


	const Input::KeyState ping_key_status = m_window.input().getKeyState(GLFW_KEY_P);
	if (ping_key_status == Input::KeyState::PRESSED)
	{
		uint64_t id = std::rand();

		auto packet = std::make_shared<PingPacket>(id);
		m_client.m_pings[id] = std::chrono::high_resolution_clock::now();
		m_client.sendPacket(packet);

		m_sound_engine.playSound(SoundName::PING);
	}

	const Input::KeyState f3_key_status = m_window.input().getKeyState(GLFW_KEY_F3);
	if (f3_key_status == Input::KeyState::PRESSED)
	{
		m_world_scene.show_debug_text = !m_world_scene.show_debug_text;
	}

	auto ret = m_world.playerAttack(m_world.m_my_player_id, m_attack);
	if (ret.first)
	{
		auto packet = std::make_shared<BlockActionPacket>(BlockInfo::Type::Air, ret.second, BlockActionPacket::Action::PLACE);
		m_client.sendPacket(packet);
	}
	ClientWorld::PlayerUseResult player_use = m_world.playerUse(m_world.m_my_player_id, m_use);
	if (player_use.hit && player_use.used_item != ItemInfo::Type::None)
	{
		auto packet = std::make_shared<BlockActionPacket>(
			g_items_info.get(player_use.used_item).block_id,
			player_use.block_position,
			BlockActionPacket::Action::PLACE
		);
		m_client.sendPacket(packet);
	}

	const Input::KeyState gamemode_0 = m_window.input().getKeyState(GLFW_KEY_0);
	const Input::KeyState gamemode_1 = m_window.input().getKeyState(GLFW_KEY_1);
	const Input::KeyState gamemode_2 = m_window.input().getKeyState(GLFW_KEY_2);
	const Input::KeyState fly = m_window.input().getKeyState(GLFW_KEY_F);

	m_world.updatePlayer(
		m_world.m_my_player_id,
		[
			gamemode_0,
			gamemode_1,
			gamemode_2,
			fly
		](Player & player)
		{
			if (gamemode_0 == Input::KeyState::PRESSED)
			{
				player.gameMode = Player::GameMode::SURVIVAL;
				player.flying = false;
				player.startFall();
			}
			if (gamemode_1 == Input::KeyState::PRESSED)
			{
				player.gameMode = Player::GameMode::CREATIVE;
				player.startFall();
			}
			if (gamemode_2 == Input::KeyState::PRESSED)
			{
				player.gameMode = Player::GameMode::SPECTATOR;
			}

			if (fly == Input::KeyState::PRESSED)
			{
				if (player.flying)
				{
					player.startFall();
				}
				player.flying = !player.flying;
			}
		}
	);

	const Input::KeyState view_mode_key_status = m_window.input().getKeyState(GLFW_KEY_F5);
	if (view_mode_key_status == Input::KeyState::PRESSED)
	{
		m_world.changePlayerViewMode(m_world.m_my_player_id);
	}

	m_window.input().getCursorPos(m_mouse_x, m_mouse_y);


	double scroll_x, scroll_y;
	m_window.input().getScroll(scroll_x, scroll_y);
	m_world.manageScroll(scroll_x, scroll_y);

	const Input::KeyState v_key_status = m_window.input().getKeyState(GLFW_KEY_V);
	if (v_key_status == Input::KeyState::PRESSED)
	{
		m_world_scene.enable_frustum_culling = !m_world_scene.enable_frustum_culling;
	}
}

void UpdateThread::movePlayer()
{
	auto static last_time = std::chrono::steady_clock::now();
	auto now = std::chrono::steady_clock::now();
	glm::dvec2 look = glm::dvec2(m_mouse_x - m_last_mouse_x, m_mouse_y - m_last_mouse_y) * m_settings.mouseSensitivity();
	m_last_mouse_x = m_mouse_x;
	m_last_mouse_y = m_mouse_y;

	if (!m_window.input().isCursorCaptured())
	{
		look = glm::dvec2(0.0);
	}

	if (m_current_time - m_last_target_block_update_time > m_target_block_update_interval)
	{
		m_world.updatePlayerTargetBlock(m_world.m_my_player_id);
		m_last_target_block_update_time = m_current_time;
	}

	auto [position, displacement]  = m_world.calculatePlayerMovement(
		m_world.m_my_player_id,
		m_move_forward,
		m_move_backward,
		m_move_left,
		m_move_right,
		m_jump,
		m_sneak,
		static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count() / 1000.0)
	);

	// m_world.applyPlayerMovement(m_world.m_my_player_id, displacement);
	if (glm::length(displacement) > 0.01)
	{
		// glm::vec3 new_position = position + displacement;
		auto packet = std::make_shared<PlayerMovePacket>(m_world.m_my_player_id, position, displacement);
		m_client.sendPacket(packet);
	}

	m_world.updatePlayerCamera(
		m_world.m_my_player_id,
		look.x,
		look.y
	);

	m_world_scene.camera() = m_world.getCamera(m_world.m_my_player_id);
	last_time = now;
}

void UpdateThread::handlePackets()
{
	ZoneScoped;
	int i = 0;
	while (i < 10 && m_client.getQueueSize())
	{
		auto packet = m_client.popPacket();
		m_packet_handler.handlePacket(packet);
	}
}
