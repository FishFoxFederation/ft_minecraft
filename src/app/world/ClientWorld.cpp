#include "ClientWorld.hpp"
#include "DebugGui.hpp"

ClientWorld::ClientWorld(
	RenderAPI & render_api,
	Sound::Engine & sound_engine,
	Event::Manager & event_manager,
	Client & client,
	uint64_t my_player_id
)
:
	World(),
	m_render_api(render_api),
	m_sound_engine(sound_engine),
	m_event_manager(event_manager),
	m_client(client)
{
	m_my_player_id = my_player_id;
	addPlayer(m_my_player_id, glm::dvec3(0.0, 150.0, 0.0));
	createBaseMob(glm::vec3(0.0, 150.0, 0.0), m_my_player_id);
}

ClientWorld::~ClientWorld()
{
}

void ClientWorld::updateBlock(glm::dvec3 position)
{
	ZoneScoped;
	updateChunks(position);
	// waitForFinishedFutures();
	m_executor.waitForFinishedTasks();
}

// void ClientWorld::update(glm::dvec3 nextPlayerPosition)
// {
// 	updateChunks(nextPlayerPosition);
// 	waitForFinishedFutures();
// }

void ClientWorld::addChunk(std::shared_ptr<Chunk> chunk)
{
	glm::ivec3 chunk_position = chunk->getPosition();
	{
		std::lock_guard lock2(m_loaded_chunks_mutex);
		std::lock_guard lock3(m_chunks_to_mesh_mutex);
		std::lock_guard lock(m_chunks_mutex);
		LockMark(m_chunks_mutex);
		LockMark(m_loaded_chunks_mutex);
		LockMark(m_chunks_to_mesh_mutex);

		m_loaded_chunks.insert(glm::ivec2(chunk_position.x, chunk_position.z));
		m_chunks_to_mesh.insert(glm::ivec2(chunk_position.x, chunk_position.z));
		m_chunks.insert(std::make_pair(chunk_position, std::move(chunk)));
	}

	//set all neighbors as not visible to force a mesh update
	// setChunkNotMeshed(glm::ivec2(chunk_position.x - 1, chunk_position.z + 1));
	// setChunkNotMeshed(glm::ivec2(chunk_position.x - 1, chunk_position.z));
	// setChunkNotMeshed(glm::ivec2(chunk_position.x - 1, chunk_position.z - 1));

	// setChunkNotMeshed(glm::ivec2(chunk_position.x + 1, chunk_position.z + 1));
	// setChunkNotMeshed(glm::ivec2(chunk_position.x + 1, chunk_position.z));
	// setChunkNotMeshed(glm::ivec2(chunk_position.x + 1, chunk_position.z - 1));

	// setChunkNotMeshed(glm::ivec2(chunk_position.x, chunk_position.z + 1));
	// setChunkNotMeshed(glm::ivec2(chunk_position.x, chunk_position.z));
	// setChunkNotMeshed(glm::ivec2(chunk_position.x, chunk_position.z - 1));
}

void ClientWorld::removeChunk(const glm::ivec3 & chunkPosition)
{
	std::lock_guard lock(m_loaded_chunks_mutex);
	unloadChunk(chunkPosition);
}

void ClientWorld::unloadChunk(const glm::ivec3 & chunkPos3D)
{
	ZoneScoped;
	std::shared_ptr<Chunk> chunk = getChunk(chunkPos3D);
	{
		std::lock_guard lock(m_chunks_mutex);
		m_chunks.erase(chunkPos3D);
	}
	{
		std::lock_guard lock(m_chunks_to_mesh_mutex);
		m_chunks_to_mesh.erase(glm::ivec2(chunkPos3D.x, chunkPos3D.z));
	}
	m_loaded_chunks.erase(glm::ivec2(chunkPos3D.x, chunkPos3D.z));

	if (chunk == nullptr)
		return;
	m_executor.submit([this, chunk]()
	{
		ZoneScopedN("Chunk Unloading");
		/**************************************************************
		 * CHUNK UNLOADING FUNCTION
		 **************************************************************/
		std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
		uint64_t mesh_scene_id;

		//will block and wait
		std::lock_guard lock(chunk->status);

		mesh_scene_id = chunk->getMeshID();

		m_render_api.removeChunkFromScene(mesh_scene_id);

		std::chrono::duration time_elapsed = std::chrono::steady_clock::now() - start;
		DebugGui::chunk_unload_time_history.push(std::chrono::duration_cast<std::chrono::microseconds>(time_elapsed).count());
	});
}

void ClientWorld::meshChunks(const glm::vec3 & playerPosition)
{
	ZoneScoped;
	glm::ivec3 playerChunk3D = glm::ivec3(playerPosition) / CHUNK_SIZE_IVEC3;
	glm::ivec2 playerChunk2D = glm::ivec2(playerChunk3D.x, playerChunk3D.z);

	std::lock_guard lock(m_chunks_to_mesh_mutex);
	std::unordered_set<glm::ivec2> temp_chunks_to_mesh = m_chunks_to_mesh;

	for(auto chunkPos2D : temp_chunks_to_mesh)
	{
		std::shared_ptr<Chunk> chunk = getChunk(glm::ivec3(chunkPos2D.x, 0, chunkPos2D.y));
		// if(chunk->status.try_lock_shared() == false)
		// 	continue;
		float distanceX = std::abs(chunkPos2D.x - playerChunk2D.x);
		float distanceZ = std::abs(chunkPos2D.y - playerChunk2D.y);

		if (distanceX < m_render_distance && distanceZ < m_render_distance)
		{
			bool unavailable_neighbours = false;
			for(int x = -1; x < 2; x++)
			{
				for(int z = -1; z < 2; z++)
				{
					glm::ivec3 chunkPos = glm::ivec3(x, 0, z) + glm::ivec3(chunkPos2D.x, 0, chunkPos2D.y);
					std::shared_ptr<Chunk> chunk = getChunk(chunkPos);
					if(chunk == nullptr)
					{
						unavailable_neighbours = true;
						break;
					}
				}
			}
			if (unavailable_neighbours)
				continue;
			// chunk->status.unlock_shared();
			m_chunks_to_mesh.erase(chunkPos2D);
			meshChunk(chunkPos2D);
			continue;
		}
		// chunk->status.unlock_shared();
	}
}

void ClientWorld::meshChunk(const glm::ivec2 & chunkPos2D)
{
	ZoneScoped;
	glm::ivec3 chunkPos3D = glm::ivec3(chunkPos2D.x, 0, chunkPos2D.y);
	/********
	 * CHECKING IF NEIGHBOURS EXIST AND ARE AVAILABLE
	********/

	//this is possible and thread safe to test if they are readable and then to modify their statuses
	//only because we have the guarantee that not other task will try to write to them
	//since task dispatching is done in order

	// LOG_INFO("Meshing chunk: " << chunkPos3D.x << " " << chunkPos3D.z);

	std::shared_ptr<Chunk> chunk = getChunk(chunkPos3D);

	//The constructor will mark the chunk as being read
	//create all mesh data needed ( pointers to neighbors basically )
	m_chunks_mutex.lock();
	LockMark(m_chunks_mutex);
	// LOG_INFO("mchunks size: " << m_chunks.size());
	CreateMeshData mesh_data(chunkPos3D, {1, 1, 1}, m_chunks);
	chunk->setMeshed(true);
	m_chunks_mutex.unlock();

	/********
	* PUSHING TASK TO THREAD POOL
	********/
	m_executor.submit([this, chunkPos3D, mesh_data = std::move(mesh_data)]() mutable
	{
		ZoneScopedN("Chunk Meshing");
		/**************************************************************
		 * CALCULATE MESH FUNCTION
		 **************************************************************/
		// LOG_DEBUG("Meshing chunk: " << chunkPos3D.x << " " << chunkPos3D.y);
		std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

		std::shared_ptr<Chunk> chunk = mesh_data.getCenterChunk();
		if (chunk == nullptr)
			throw std::runtime_error("Center chunk is null");
		chunk->setMeshed(true);

		//destroy old mesh if it exists
		uint64_t old_mesh_scene_id = chunk->getMeshID();


		mesh_data.create(); //CPU intensive task to create the mesh

		//storing mesh in the GPU
		RenderAPI::ChunkMeshCreateInfo chunk_mesh_info = {
			.block_vertex = std::move(mesh_data.vertices),
			.block_index = std::move(mesh_data.indices),
			.water_vertex = std::move(mesh_data.water_vertices),
			.water_index = std::move(mesh_data.water_indices),
			.model = Transform(glm::vec3(chunkPos3D * CHUNK_SIZE_IVEC3)).model()
		};
		RenderAPI::InstanceId chunk_scene_id = m_render_api.addChunkToScene(chunk_mesh_info);
		chunk->setMeshID(chunk_scene_id);

		m_render_api.removeChunkFromScene(old_mesh_scene_id);

		mesh_data.unlock();

		std::chrono::duration time_elapsed = std::chrono::steady_clock::now() - start;
		DebugGui::chunk_render_time_history.push(std::chrono::duration_cast<std::chrono::microseconds>(time_elapsed).count());
	});
}

void ClientWorld::updateChunks(const glm::vec3 & playerPosition)
{
	ZoneScoped;
	// static std::chrono::steady_clock::time_point last_update = std::chrono::steady_clock::now();
	// std::lock_guard lock(m_chunks_mutex);
	// std::lock_guard lock5(m_unload_set_mutex);
	// std::lock_guard lock6(m_blocks_to_set_mutex);
	std::lock_guard lock(m_loaded_chunks_mutex);

	glm::ivec3 playerChunk3D = getChunkPos(playerPosition);
	playerChunk3D.y = 0;
	const std::shared_ptr<Chunk> playerChunk = getChunk(playerChunk3D);
	if (playerChunk != nullptr)
	{
		Chunk::biomeInfo biome = playerChunk->getBiome(getBlockChunkPos(playerPosition).x, getBlockChunkPos(playerPosition).z);

		DebugGui::continentalness = biome.continentalness;
		DebugGui::erosion = biome.erosion;
		DebugGui::humidity = biome.humidity;
		DebugGui::temperature = biome.temperature;
		DebugGui::weirdness = biome.weirdness;
		DebugGui::PV = biome.PV;
		// DebugGui::isLand = biome.isLand;
		// DebugGui::isOcean = biome.isOcean;
		DebugGui::biome = static_cast<uint8_t>(biome.biome);
	}
	// loadChunks(playerPosition);
	// unloadChunks(playerPosition);
	doBlockSets();

	updateLights();

	meshChunks(playerPosition);
}

void ClientWorld::doBlockSets()
{
	{
		std::lock_guard lock(m_blocks_to_set_mutex);
		if (m_blocks_to_set.empty())
			return;
	}
		while(1)
		{
			glm::vec3 position;
			BlockInfo::Type block_id;
			glm::vec3 block_chunk_position;
			glm::vec3 chunk_position;
			glm::ivec2 chunk_position2D;
			std::shared_ptr<Chunk> chunk;
			{
				std::lock_guard lock(m_chunks_mutex);
				std::lock_guard lock3(m_blocks_to_set_mutex);
				if (m_blocks_to_set.empty())
					break;
				auto ret_pair = m_blocks_to_set.front();
				m_blocks_to_set.pop();
				position = ret_pair.first;
				block_id = ret_pair.second;
				block_chunk_position = getBlockChunkPosition(position);
				chunk_position = getChunkPosition(position);
				chunk_position2D = glm::ivec2(chunk_position.x, chunk_position.z);
				if (!m_loaded_chunks.contains(chunk_position2D))
					continue;
				chunk = m_chunks.at(glm::ivec3(chunk_position.x, 0, chunk_position.z));
			}

			chunk->status.lock();
			chunk->setBlock(block_chunk_position, block_id);
			chunk->setMeshed(false);
			chunk->status.unlock();

			{
				std::lock_guard lock(m_block_light_update_mutex);
				m_block_light_update.push(glm::ivec3(position));
			}

			{
				std::lock_guard lock(m_chunks_to_mesh_mutex);
				m_chunks_to_mesh.insert(chunk_position2D);
			}

			if (block_chunk_position.x == 0)
				setChunkNotMeshed(glm::ivec2(chunk_position2D.x - 1, chunk_position2D.y));
			if (block_chunk_position.x == CHUNK_X_SIZE - 1)
				setChunkNotMeshed(glm::ivec2(chunk_position2D.x + 1, chunk_position2D.y));
			if (block_chunk_position.z == 0)
				setChunkNotMeshed(glm::ivec2(chunk_position2D.x, chunk_position2D.y - 1));
			if (block_chunk_position.z == CHUNK_Z_SIZE - 1)
				setChunkNotMeshed(glm::ivec2(chunk_position2D.x, chunk_position2D.y + 1));
		}

		// {
			// std::lock_guard lock(m_finished_futures_mutex);
			// m_finished_futures.push(current_id);
		// }
	// });
	// m_futures.insert(std::make_pair(current_id, std::move(future)));
}

void ClientWorld::updateLights()
{
	/*
	 * This function is the same for the server and the client.
	 * TODO: decide if it should be moved to the World class
	 *
	 * If this comment is still here and the functions are different,
	 * ask me why the fuck it is the case :)
	 *
	 * not anymore since the system for remeshing clientside has changed we need to gather a list of modified chunks
	 * so the code is a bit different than serverside i think we can keep those separate for now
	 */
	std::unordered_set<glm::ivec3> modified_chunks;
	for (;;)
	{
		glm::ivec3 position;
		{
			std::lock_guard light_update_lock(m_block_light_update_mutex);
			if (m_block_light_update.empty())
				break;

			position = m_block_light_update.front();
			m_block_light_update.pop();
		}

		// This check is also present in the updateLight functions below
		// TODO: decide where it should be kept
		const glm::ivec3 chunk_position = getChunkPosition(position);
		std::shared_ptr<Chunk> chunk = getChunk(chunk_position);
		if (chunk == nullptr)
			continue;

		modified_chunks.merge(updateSkyLight(position));
		modified_chunks.merge(updateBlockLight(position));
	}
	{
		std::lock_guard lock(m_chunks_to_mesh_mutex);
		for(auto position : modified_chunks)
			m_chunks_to_mesh.insert({position.x, position.z});
	}
}

void ClientWorld::applyPlayerMovement(const uint64_t & player_id, const glm::dvec3 & displacement)
{
	std::shared_ptr<Player> player = std::dynamic_pointer_cast<Player>(m_players.at(player_id));
	std::lock_guard lock(player->mutex);

	// apply displacement
	player->transform.position += displacement;

	DebugGui::player_position = player->transform.position;

	m_render_api.updatePlayer(player_id, [displacement](PlayerRenderData & data)
	{
		data.position += displacement;
	});
}

void ClientWorld::updatePlayerPosition(const uint64_t & player_id, const glm::dvec3 & position)
{
	if (!m_players.contains(player_id))
		return;
	std::shared_ptr<Player> player = std::dynamic_pointer_cast<Player>(m_players.at(player_id));
	std::lock_guard lock(player->mutex);

	// apply displacement
	player->transform.position = position;

	m_render_api.updatePlayer(player_id, [position](PlayerRenderData & data)
	{
		data.position = position;
	});

	if (player_id == m_my_player_id)
	{
		DebugGui::player_position = player->transform.position;

		// update camera
		m_render_api.setCamera(player->camera());
	}

	// play footstep sound
	static auto last_footstep = std::chrono::steady_clock::now();
	if (player->on_ground &&
		glm::length(player->velocity) > 0.1 &&
		player_id == m_my_player_id &&
		std::chrono::steady_clock::now() - last_footstep > std::chrono::milliseconds(400))
	{
		m_sound_engine.playSound(SoundName::GRASS1);
		last_footstep = std::chrono::steady_clock::now();
	}
}

void ClientWorld::otherUpdate()
{
	ZoneScoped;
}

std::pair<glm::dvec3, glm::dvec3> ClientWorld::calculatePlayerMovement(
	const uint64_t player_id,
	const int8_t forward,
	const int8_t backward,
	const int8_t left,
	const int8_t right,
	const int8_t up,
	const int8_t down,
	const double delta_time_second
)
{
	std::shared_ptr<Player> player;
	{
		std::lock_guard lock(m_players_mutex);
		player = m_players.at(player_id);
	}
	std::lock_guard lock(player->mutex);

	bool on_ground = hitboxCollisionWithBlock(player->feet, player->transform.position, BLOCK_PROPERTY_SOLID);
	bool in_fluid = hitboxCollisionWithBlock(player->feet, player->transform.position, BLOCK_PROPERTY_FLUID);
	if (on_ground && !player->on_ground) // player just landed
	{
		player->jump_remaining = 1;
		player->jumping = false;
	}
	if (!on_ground && player->on_ground) // player just started falling
	{
		player->startFall();
	}

	if (on_ground)
	{
		player->ground_block = rayCastOnBlock(player->transform.position, glm::dvec3(0.0, -1.0, 0.0), 1.0).block;
	}
	else
	{
		player->ground_block = BlockInfo::Type::Air;
	}

	player->on_ground = on_ground;
	player->swimming = in_fluid;



	// get the movement vector
	glm::dvec3 move(right - left, 0.0, forward - backward);
	// normalize the move vector to prevent faster diagonal movement
	// but without the y component because we don't want to slow down the player when moving jumping
	if (glm::length(move) > 0.0)
		move = glm::normalize(move);
	// transform the move vector to the player's local coordinate system
	move = player->getTransformedMovement(move);


	glm::dvec3 displacement;

	if (player->isFlying() == false)
	{
		if (!player->swimming) // if player is walking
		{
			double acc = 40.0;
			double ground_friction = 10.0;
			double air_friction = 0.8;
			glm::dvec3 friction = glm::dvec3(ground_friction, air_friction, ground_friction);

			double jump_force = 9.0;
			double gravity = -25.0;

			if (up && player->canJump())
			{
				player->startJump();
				player->velocity.y = jump_force;
			}

			player->velocity.y += gravity * delta_time_second;

			player->velocity += move * acc * delta_time_second;

			displacement = player->velocity * delta_time_second;

			player->velocity *= (1.0 - glm::min(delta_time_second * friction, 1.0));
		}
		else // if player is swimming
		{
			double acc = 20.0;
			double fluid_friction = 10.0;
			glm::dvec3 friction = glm::dvec3(fluid_friction, fluid_friction, fluid_friction);

			double jump_force = 4.0;
			double gravity = -25.0;

			if (up)
			{
				player->velocity.y = jump_force;
			}

			player->velocity.y += gravity * delta_time_second;

			player->velocity += move * acc * delta_time_second;

			displacement = player->velocity * delta_time_second;

			player->velocity *= (1.0 - glm::min(delta_time_second * friction, 1.0));
		}
	}
	else // if player is flying
	{
		double acc = 2000.0;
		double drag = 10.0;

		move.y = up - down;

		player->velocity += move * acc * delta_time_second;

		displacement = player->velocity * delta_time_second;

		player->velocity *= (1.0 - glm::min(delta_time_second * drag, 1.0));

	}

	if (player->shouldCollide())
	{
		const glm::dvec3 move_x = {displacement.x, 0.0, 0.0};
		const glm::dvec3 move_y = {0.0, displacement.y, 0.0};
		const glm::dvec3 move_z = {0.0, 0.0, displacement.z};
		const glm::dvec3 move_xz = {displacement.x, 0.0, displacement.z};
		const glm::dvec3 move_yz = {0.0, displacement.y, displacement.z};
		const glm::dvec3 move_xy = {displacement.x, displacement.y, 0.0};

		bool collision_x = hitboxCollisionWithBlock(player->hitbox, player->transform.position + move_x, BLOCK_PROPERTY_SOLID);
		bool collision_y = hitboxCollisionWithBlock(player->hitbox, player->transform.position + move_y, BLOCK_PROPERTY_SOLID);
		bool collision_z = hitboxCollisionWithBlock(player->hitbox, player->transform.position + move_z, BLOCK_PROPERTY_SOLID);
		bool collision_xz = hitboxCollisionWithBlock(player->hitbox, player->transform.position + move_xz, BLOCK_PROPERTY_SOLID);
		bool collision_yz = hitboxCollisionWithBlock(player->hitbox, player->transform.position + move_yz, BLOCK_PROPERTY_SOLID);
		bool collision_xy = hitboxCollisionWithBlock(player->hitbox, player->transform.position + move_xy, BLOCK_PROPERTY_SOLID);

		// edge case when the player is perfectly aligned with the corner of a block
		if (!collision_x && !collision_z && collision_xz)
		{
			// artificially set the collision on the axis with the smallest displacement
			if (std::abs(displacement.x) > std::abs(displacement.z))
				collision_z = true;
			else
				collision_x = true;
		}
		if ((!collision_x && !collision_y && collision_xy) || (!collision_y && !collision_z && collision_yz))
		{
			collision_y = true;
		}

		if (collision_x)
		{
			displacement.x = 0.0;
			player->velocity.x = 0.0;
		}
		if (collision_y)
		{
			displacement.y = -(player->transform.position.y - glm::floor(player->transform.position.y));
			player->velocity.y = 0.0;
		}
		if (collision_z)
		{
			displacement.z = 0.0;
			player->velocity.z = 0.0;
		}
	}

	DebugGui::player_velocity_vec = player->velocity;
	DebugGui::player_velocity = glm::length(player->velocity);

	// update player walking animation
	m_render_api.updatePlayer(player_id, [move](PlayerRenderData & data)
	{
		if (glm::length(move) > 0.0)
		{
			if (data.walk_animation.isActive() == false)
			{
				data.walk_animation.start();
			}
		}
		else
		{
			data.walk_animation.stop();
		}
	});

	return std::make_pair(player->transform.position, displacement);
}

void ClientWorld::updatePlayerTargetBlock(
	const uint64_t player_id
)
{
	std::shared_ptr<Player> player = m_players.at(player_id);
	std::lock_guard guard(player->mutex);

	glm::dvec3 position = player->transform.position + player->eye_position;
	glm::dvec3 direction = player->direction();

	RayCastOnBlockResult raycast = rayCastOnBlock(position, direction, 5.0);

	std::optional<glm::vec3> target_block = raycast.hit && !raycast.inside_block ? std::make_optional(raycast.block_position) : std::nullopt;

	m_render_api.setTargetBlock(target_block);

	player->targeted_block = raycast;

	if (player->targeted_block.hit)
	{
		glm::ivec3 pos = player->targeted_block.block_position + player->targeted_block.normal;
		glm::ivec3 chunk_pos = getChunkPosition(pos);
		if (m_chunks.contains(chunk_pos))
		{
			std::shared_ptr<Chunk> chunk = m_chunks.at(chunk_pos);
			if (chunk->status.try_lock_shared())
			{
				const glm::ivec3 block_chunk_pos = getBlockChunkPosition(pos);
				uint8_t sky_light = chunk->getSkyLight(block_chunk_pos);
				uint8_t block_light = chunk->getBlockLight(block_chunk_pos);
				chunk->status.unlock_shared();

				DebugGui::looked_face_sky_light = sky_light;
				DebugGui::looked_face_block_light = block_light;
			}
		}
	}
	else
	{
		DebugGui::looked_face_sky_light = -1;
		DebugGui::looked_face_block_light = -1;
	}

}

std::pair<bool, glm::vec3> ClientWorld::playerAttack(
	const uint64_t player_id,
	bool attack
)
{
	std::shared_ptr<Player> player = m_players.at(player_id);
	std::lock_guard guard(player->mutex);

	if (!attack || !player->canAttack())
		return {false, glm::vec3(0.0)};
	player->startAttack();

	m_render_api.updatePlayer(player_id, [](PlayerRenderData & data)
	{
		if (data.attack_animation.isActive() == false)
		{
			data.attack_animation.start();
		}
	});

	if (player->targeted_block.hit)
	{
		// LOG_DEBUG("Block hit: "
		// 	<< player->targeted_block.block_position.x << " " << player->targeted_block.block_position.y << " " << player->targeted_block.block_position.z
		// 	<< " = " << int(player->targeted_block.block)
		// );

		// std::lock_guard lock(m_blocks_to_set_mutex);
		// m_blocks_to_set.push({player->targeted_block.block_position, BlockInfo::Type::Air});

		return std::make_pair(true, player->targeted_block.block_position);
	}
	// else
	// {
	// 	LOG_DEBUG("No block hit");
	// }
	return std::make_pair(false, glm::vec3(0.0));
}

ClientWorld::PlayerUseResult ClientWorld::playerUse(
	const uint64_t player_id,
	bool use
)
{
	std::shared_ptr<Player> player = m_players.at(player_id);
	std::lock_guard guard(player->mutex);

	if (!use || !player->canUse())
		return {false, glm::vec3(0.0)};
	player->startUse();

	ItemInfo::Type item_type = player->toolbar_items.at(player->toolbar_cursor);

	m_render_api.updatePlayer(player_id, [](PlayerRenderData & data)
	{
		if (data.attack_animation.isActive() == false)
		{
			data.attack_animation.start();
		}
	});

	if (player->targeted_block.hit)
	{
		glm::vec3 block_placed_position = player->targeted_block.block_position + player->targeted_block.normal;

		// check collision with player
		if (isColliding(
			player->hitbox,
			player->transform.position,
			g_blocks_info.get(player->targeted_block.block).hitbox,
			block_placed_position
		))
			return {false, glm::vec3(0.0), item_type};

		// std::lock_guard lock(m_blocks_to_set_mutex);
		// m_blocks_to_set.push({block_placed_position, BlockInfo::Stone.id});
		return {true, block_placed_position, item_type};
	}
	return {false, glm::vec3(0.0), item_type};
}

void ClientWorld::updatePlayerCamera(
	const uint64_t player_id,
	const double x_offset,
	const double y_offset
)
{
	std::shared_ptr<Player> player = m_players.at(player_id);
	std::lock_guard guard(player->mutex);

	player->moveDirection(x_offset, y_offset);

	// copy pitch and yaw because the update is async and we don't want to access player without locking it's mutex
	double pitch = player->pitch;
	double yaw = player->yaw;
	m_render_api.updatePlayer(player_id, [pitch, yaw](PlayerRenderData & data)
	{
		data.pitch = pitch;
		data.yaw = yaw;
	});
}

void ClientWorld::changePlayerViewMode(
	const uint64_t player_id
)
{
	std::shared_ptr<Player> player = m_players.at(player_id);
	std::lock_guard lock(player->mutex);

	switch (player->view_mode)
	{
	case Player::ViewMode::FIRST_PERSON:
		player->view_mode = Player::ViewMode::THIRD_PERSON_BACK;
		m_render_api.updatePlayer(player_id, [](PlayerRenderData & data) { data.visible = true; });
		break;
	case Player::ViewMode::THIRD_PERSON_BACK:
		player->view_mode = Player::ViewMode::FIRST_PERSON;
		m_render_api.updatePlayer(player_id, [](PlayerRenderData & data) { data.visible = false; });
		break;
	}
}

void ClientWorld::manageScroll(
	const double x_offset,
	const double y_offset
)
{
	std::shared_ptr<Player> player = m_players.at(m_my_player_id);
	std::lock_guard lock(player->mutex);

	player->toolbar_cursor = (player->toolbar_cursor - static_cast<int>(y_offset) + 9) % 9;
	m_render_api.setToolbarCursor(player->toolbar_cursor);
}

void ClientWorld::updatePlayer(
	const uint64_t player_id,
	std::function<void(Player &)> update
)
{
	std::shared_ptr<Player> player = m_players.at(player_id);
	std::lock_guard lock(player->mutex);
	update(*player);
}

void ClientWorld::createMob()
{
	std::lock_guard lock(m_mobs_mutex);

	std::shared_ptr<Mob> mob = std::make_shared<Mob>();
	mob->transform.position = glm::dvec3(0.0, 220.0, 0.0);
	uint64_t mob_id = m_mob_id++;
	m_mobs.insert(std::make_pair(mob_id, mob));

	m_render_api.addEntity(
		mob_id,
		{
			m_render_api.getCubeMeshId(),
			Transform(
				mob->transform.position + mob->hitbox.position,
				glm::vec3(0.0f),
				mob->hitbox.size
			).model()
		}
	);
}

void ClientWorld::updateMobs(
	const double delta_time_second
)
{
	std::shared_ptr<Player> player;
	{
		std::lock_guard lock1(m_players_mutex);
		player = m_players.at(m_my_player_id);
	}
	std::lock_guard lock2(player->mutex);



	std::lock_guard lock(m_mobs_mutex);
	for (auto & [id, mob] : m_mobs)
	{
		std::lock_guard lock(mob->mutex);

		{
			glm::dvec3 chunk_position3D = getChunkPosition(mob->transform.position);
			glm::ivec2 chunk_position2D = glm::ivec2(chunk_position3D.x, chunk_position3D.z);
			std::lock_guard lock(m_chunks_mutex);
			if (m_loaded_chunks.contains(chunk_position2D) == false)
				continue;
		}

		mob->target_position = player->transform.position;

		// determine if mob is on the ground or in the air and detect
		bool on_ground = hitboxCollisionWithBlock(mob->feet, mob->transform.position, BLOCK_PROPERTY_SOLID);
		if (on_ground && !mob->on_ground) // mob just landed
		{
			mob->jump_remaining = 1;
			mob->jumping = false;
		}
		if (!on_ground && mob->on_ground) // mob just started falling
		{
			mob->startFall();
		}
		mob->on_ground = on_ground;

		glm::dvec3 diff = mob->target_position - mob->transform.position;

		// get the movement vector
		glm::dvec3 move{diff.x, 0.0, diff.z};
		// normalize the move vector to prevent faster diagonal movement
		// but without the y component because we don't want to slow down the mob when moving jumping
		if (glm::length(move) > 0.0)
			move = glm::normalize(move);

		double acc = 30.0;
		double ground_friction = 10.0;
		double air_friction = 0.8;
		glm::dvec3 friction = glm::dvec3(ground_friction, air_friction, ground_friction);

		double jump_force = 9.0;
		double gravity = -25.0;

		if (mob->should_jump && mob->canJump())
		{
			mob->startJump();
			mob->velocity.y = jump_force;
			mob->should_jump = false;
		}

		mob->velocity.y += gravity * delta_time_second;

		mob->velocity += move * acc * delta_time_second;

		glm::dvec3 displacement = mob->velocity * delta_time_second;

		mob->velocity *= (1.0 - glm::min(delta_time_second * friction, 1.0));

		// collision detection
		const glm::dvec3 move_x = {displacement.x, 0.0, 0.0};
		const glm::dvec3 move_y = {0.0, displacement.y, 0.0};
		const glm::dvec3 move_z = {0.0, 0.0, displacement.z};
		const glm::dvec3 move_xz = {displacement.x, 0.0, displacement.z};
		const glm::dvec3 move_yz = {0.0, displacement.y, displacement.z};
		const glm::dvec3 move_xy = {displacement.x, displacement.y, 0.0};

		bool collision_x = hitboxCollisionWithBlock(mob->hitbox, mob->transform.position + move_x, BLOCK_PROPERTY_SOLID);
		bool collision_y = hitboxCollisionWithBlock(mob->hitbox, mob->transform.position + move_y, BLOCK_PROPERTY_SOLID);
		bool collision_z = hitboxCollisionWithBlock(mob->hitbox, mob->transform.position + move_z, BLOCK_PROPERTY_SOLID);
		bool collision_xz = hitboxCollisionWithBlock(mob->hitbox, mob->transform.position + move_xz, BLOCK_PROPERTY_SOLID);
		bool collision_yz = hitboxCollisionWithBlock(mob->hitbox, mob->transform.position + move_yz, BLOCK_PROPERTY_SOLID);
		bool collision_xy = hitboxCollisionWithBlock(mob->hitbox, mob->transform.position + move_xy, BLOCK_PROPERTY_SOLID);
		// edge case when the mob is perfectly aligned with the corner of a block
		if (!collision_x && !collision_z && collision_xz)
		{
			// artificially set the collision on the axis with the smallest displacement
			if (std::abs(displacement.x) > std::abs(displacement.z))
				collision_z = true;
			else
				collision_x = true;
		}
		if ((!collision_x && !collision_y && collision_xy) || (!collision_y && !collision_z && collision_yz))
		{
			collision_y = true;
		}

		if (collision_x)
		{
			displacement.x = 0.0;
			mob->velocity.x = 0.0;
			mob->should_jump = true;
		}
		if (collision_y)
		{
			displacement.y = -(mob->transform.position.y - glm::floor(mob->transform.position.y));
			mob->velocity.y = 0.0;
		}
		if (collision_z)
		{
			displacement.z = 0.0;
			mob->velocity.z = 0.0;
			mob->should_jump = true;
		}

		mob->transform.position += displacement;

		const glm::mat4 model = Transform(
			mob->transform.position + mob->hitbox.position,
			glm::vec3(0.0f),
			mob->hitbox.size
		).model();
		m_render_api.updateEntity(id, [model](MeshRenderData & data) {
			data.model = model;
		});
	}
}

Camera ClientWorld::getCamera(const uint64_t player_id)
{
	std::shared_ptr<Player> player = m_players.at(player_id);
	std::lock_guard lock(player->mutex);
	return player->camera();
}

glm::dvec3 ClientWorld::getPlayerPosition(const uint64_t player_id)
{
	std::shared_ptr<Player> player = m_players.at(player_id);
	std::lock_guard lock(player->mutex);
	return player->transform.position;
}

bool ClientWorld::hitboxCollisionWithBlock(
	const HitBox & hitbox,
	const glm::dvec3 & position,
	const uint64_t block_properties
)
{

	glm::dvec3 offset = glm::dvec3(0.0);
	for (offset.x = -1; offset.x <= glm::ceil(hitbox.size.x); offset.x++)
	{
		for (offset.z = -1; offset.z <= glm::ceil(hitbox.size.z); offset.z++)
		{
			for (offset.y = -1; offset.y <= glm::ceil(hitbox.size.y); offset.y++)
			{
				glm::vec3 block_position = glm::floor(position + offset);
				glm::vec3 block_chunk_position = getBlockChunkPosition(block_position);
				glm::vec3 chunk_position = getChunkPosition(block_position);
				// glm::ivec2 chunk_position2D = glm::ivec2(chunk_position.x, chunk_position.z);

				std::shared_ptr<Chunk> chunk = getChunk(glm::ivec3(chunk_position));
				if (chunk == nullptr)
					continue;

				chunk->status.lock_shared();
				BlockInfo::Type block_id = chunk->getBlock(block_chunk_position);
				chunk->status.unlock_shared();

				if (g_blocks_info.hasProperty(block_id, block_properties))
				{
					HitBox block_hitbox = g_blocks_info.get(block_id).hitbox;

					if (isColliding(hitbox, position, block_hitbox, block_position))
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

RayCastOnBlockResult ClientWorld::rayCastOnBlock(
	const glm::vec3 & origin,
	const glm::vec3 & direction,
	const double max_distance
)
{
	glm::vec3 position = origin;
	glm::vec3 dir = glm::normalize(direction);
	glm::vec3 block_position = glm::floor(position);

	// step is sign of the direction (used to step through the block grid)
	glm::vec3 step = glm::sign(dir);
	// delta is the distance between blocks following the direction vector
	glm::vec3 delta{
		glm::length(dir * glm::abs(1.0f / dir.x)),
		glm::length(dir * glm::abs(1.0f / dir.y)),
		glm::length(dir * glm::abs(1.0f / dir.z))
	};
	// side_dist is the distance from the current position to the next block
	glm::vec3 side_dist{
		(dir.x > 0.0f ? (block_position.x + 1.0f - position.x) : (position.x - block_position.x)) * delta.x,
		(dir.y > 0.0f ? (block_position.y + 1.0f - position.y) : (position.y - block_position.y)) * delta.y,
		(dir.z > 0.0f ? (block_position.z + 1.0f - position.z) : (position.z - block_position.z)) * delta.z
	};

	int axis = -1;
	float min_side_dist = 0.0f;
	while (min_side_dist < max_distance)
	{
		glm::vec3 block_chunk_position = getBlockChunkPosition(block_position);
		glm::vec3 chunk_position = getChunkPosition(block_position);
		// glm::ivec2 chunk_position2D = glm::ivec2(chunk_position.x, chunk_position.z);
		{
			std::shared_ptr<Chunk> chunk = getChunk(glm::ivec3(chunk_position));
			if (chunk != nullptr)
			{
				chunk->status.lock_shared();
				BlockInfo::Type block_id = chunk->getBlock(block_chunk_position);
				chunk->status.unlock_shared();

				if (g_blocks_info.hasProperty(block_id, BLOCK_PROPERTY_SOLID))
				{
					// for now treat all blocks as cubes
					glm::vec3 normal = glm::vec3(0.0f);
					bool inside_block = false;
					if (axis != -1)
						normal[axis] = -step[axis];
					else
					{
						inside_block = true;
					}

					const glm::vec3 hit_position = position + dir * min_side_dist;

					return {
						true,
						block_position,
						normal,
						hit_position,
						block_id,
						inside_block
					};
				}
			}
		}

		// find the axis with the smallest side_dist
		axis = 0;
		if (side_dist.y < side_dist.x)
		{
			axis = 1;
			if (side_dist.z < side_dist.y)
				axis = 2;
		}
		else if (side_dist.z < side_dist.x)
		{
			axis = 2;
		}

		// save the minimum side_dist
		min_side_dist = side_dist[axis];

		// increment the side_dist
		side_dist[axis] += delta[axis];

		// increment the block position
		block_position[axis] += step[axis];
	}

	return {
		false,
		glm::vec3(0.0f),
		glm::vec3(0.0f),
		glm::vec3(0.0f),
		BlockInfo::Type::Air,
		false
	};
}

void ClientWorld::addPlayer(const uint64_t player_id, const glm::dvec3 & position)
{
	std::shared_ptr<Player> player = std::make_shared<Player>();
	player->transform.position = position;

	const PlayerRenderData render_data = {
		.position = position,
		.visible = (player_id != m_my_player_id)
	};
	m_render_api.addPlayer(player_id, render_data);
	for (int i = 0; i < 9; i++)
	{
		m_render_api.setToolbarItem(i, player->toolbar_items[i]);
	}

	m_players.insert(std::make_pair(player_id, player));
}

void ClientWorld::removePlayer(const uint64_t player_id)
{
	std::shared_ptr<Player> player = m_players.at(player_id);
	std::lock_guard lock(player->mutex);

	m_players.erase(player_id);
	m_render_api.removePlayer(player_id);
}

void ClientWorld::modifyBlock(const glm::vec3 & position, const BlockInfo::Type & block_id)
{
	std::lock_guard lock(m_blocks_to_set_mutex);
	m_blocks_to_set.push({position, block_id});
}

void ClientWorld::setChunkNotMeshed(const glm::ivec2 & chunk_position)
{
	std::shared_ptr<Chunk> chunk = getChunk(glm::ivec3(chunk_position.x, 0, chunk_position.y));
	if (!chunk)
		return;
	{
		std::lock_guard lock(m_chunks_to_mesh_mutex);
		m_chunks_to_mesh.insert(chunk_position);
	}
}

std::shared_ptr<Chunk> ClientWorld::localGetChunk(const glm::ivec3 & chunk_position) const
{
	std::lock_guard lock(m_chunks_mutex);
	auto it = m_chunks.find(chunk_position);
	if (it != m_chunks.end())
	{
		return it->second;
	}
	return nullptr;
}

void ClientWorld::setRenderDistance(const int & render_distance)
{
	m_render_distance = render_distance;
}

int ClientWorld::getRenderDistance() const
{
	return m_render_distance;
}

void ClientWorld::setServerLoadDistance(const int & server_load_distance)
{
	m_server_load_distance = server_load_distance;
}

int ClientWorld::getServerLoadDistance() const
{
	return m_server_load_distance;
}


/*************************************
 *  MOBS
 *************************************/

void ClientWorld::createBaseMob(const glm::dvec3 & position, uint64_t player_id)
{
	auto [succes, entity] = m_ecs_manager.createEntity();

	if (!succes)
	{
		LOG_ERROR("Failed to create mob entity");
		return;
	}
	m_render_api.addEntity(entity, {m_render_api.getCubeMeshId(), glm::translate(glm::dmat4(1.0), position)});

	m_ecs_manager.add(entity,
		Position{position},
		Velocity{glm::vec3(0.0f)},
		Acceleration{glm::vec3(10.0f, 10.0f, 10.0f)},
		AITarget{player_id},
		Mesh{0}
	);

	LOG_INFO("Created mob entity: " << entity);
}

void ClientWorld::MovementSystem(const double delta_time_second)
{
	auto view = m_ecs_manager.view<Position, Velocity>();


	for ( auto entity : view )
	{
		auto [ position, velocity ] = m_ecs_manager.getComponents<Position, Velocity>(entity);

		position.p += velocity.v * delta_time_second;
	}
}

void ClientWorld::renderSystem()
{
	auto view = m_ecs_manager.view<Position, Mesh>();
	for (auto entity : view )
	{
		auto [ position, mesh ] = m_ecs_manager.getComponents<Position, Mesh>(entity);

		m_render_api.updateEntity(entity, [position](MeshRenderData & data)
		{
			data.model = glm::translate(glm::dmat4(1.0), position.p);
		});
	}
}

void ClientWorld::AISystem()
{
	auto view = m_ecs_manager.view<Position, AITarget, Velocity, Acceleration>();

	for (auto entity : view)
	{
		auto [ position, ai_target, velocity, acceleration ] = m_ecs_manager.getComponents<Position, AITarget, Velocity, Acceleration>(entity);

		auto player_position = getPlayerPosition(ai_target.target_id);

		glm::dvec3 diff = player_position - position.p;
		if (glm::length(diff) >= 0.0001)
			diff = glm::normalize(diff);

		velocity.v = diff * acceleration.a;
	}
}

void ClientWorld::updateSystems(const double delta_time_second)
{
	// LOG_INFO("Updating systems");
	AISystem();
	MovementSystem(delta_time_second);
	renderSystem();
}
