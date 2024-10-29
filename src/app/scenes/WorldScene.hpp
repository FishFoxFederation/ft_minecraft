#pragma once

#include "define.hpp"
#include "logger.hpp"
#include "Transform.hpp"
#include "Camera.hpp"
#include "List.hpp"
#include "Model.hpp"
#include "Item.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Tracy.hpp"

#include <vector>
#include <mutex>
#include <optional>
#include <map>
#include <algorithm>
#include <atomic>

/**
 * @brief Class to hold the world scene. The instance of this class will be
 * accessed by multiple threads so it will include synchronization logic.
 *
 * @details The WorldScene class will hold the mesh data and the camera data.
 */
class WorldScene
{

public:

	struct ChunkMeshRenderData
	{
		uint64_t id;
		uint64_t water_id;
		glm::dmat4 model;
	};

	struct MeshRenderData
	{
		uint64_t id;
		glm::dmat4 model;
	};

	struct PlayerRenderData
	{
		glm::dvec3 position;
		double yaw = 0;
		double pitch = 0;

		PlayerModel::WalkAnimation walk_animation;
		PlayerModel::AttackAnimation attack_animation;

		bool visible = true;
	};

	/**
	 * @brief Construct a new WorldScene object
	 */
	WorldScene();

	/**
	 * @brief Destroy the WorldScene object
	 */
	~WorldScene();

	WorldScene(WorldScene & scene) = delete;
	WorldScene(WorldScene && scene) = delete;
	WorldScene & operator=(WorldScene & scene) = delete;
	WorldScene & operator=(WorldScene && scene) = delete;

	/**
	 * @brief Function to get the camera.
	 *
	 * @return A reference to the camera.
	 */
	Camera & camera() { return m_camera; }

	/**
	 * @brief Function const to get the camera.
	 *
	 * @return A const reference to the camera.
	 *
	 */
	const Camera & camera() const { return m_camera; }

	/**
	 * @brief Function to set the target block.
	 *
	 * @param target_block The position of the block the camera is looking at. Can be std::nullopt.
	 */
	void setTargetBlock(const std::optional<glm::vec3> & target_block);

	/**
	 * @brief Function to get the target block.
	 *
	 * @return The position of the block the camera is looking at.
	 */
	std::optional<glm::vec3> targetBlock() const;

	std::vector<PlayerRenderData> getPlayers() const;

	struct DebugBlock
	{
		glm::vec3 position;
		float size;
		glm::vec4 color;
	};

	void setDebugBlock(const std::vector<DebugBlock> & debug_block);
	void clearDebugBlocks();
	std::vector<DebugBlock> debugBlocks() const;


	// MeshList chunk_mesh_list;
	IdList<uint64_t, ChunkMeshRenderData> chunk_mesh_list;
	IdList<uint64_t, MeshRenderData> entity_mesh_list;

	std::map<uint64_t, PlayerRenderData> m_players;
	mutable TracyLockableN(std::mutex, m_player_mutex, "Player Render Data");

	std::atomic<bool> show_debug_text = false;

	// hud
	std::array<ItemInfo::Type, 9> toolbar_items;
	mutable std::mutex toolbar_items_mutex;
	std::atomic<int> toolbar_cursor_index = 0;

	mutable std::atomic<bool> enable_frustum_culling = false;

private:

	Camera m_camera;

	// position of the block the camera is looking at
	std::optional<glm::vec3> m_target_block;
	mutable TracyLockableN(std::mutex, m_target_block_mutex, "Target Block");

	std::vector<DebugBlock> m_debug_block;
	mutable TracyLockableN(std::mutex, m_debug_block_mutex, "Debug Block");
};
