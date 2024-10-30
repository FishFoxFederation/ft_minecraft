#include "World.hpp"

World::World()
: m_world_generator(*this)
{
}

World::~World()
{
}

World::WorldGenerator & World::getWorldGenerator()
{
	return m_world_generator;
}

glm::vec3 World::getBlockChunkPosition(const glm::vec3 & position)
{
	// glm::vec3 block_chunk_position = glm::ivec3(position) % CHUNK_SIZE_IVEC3;
	// if (block_chunk_position.x < 0) block_chunk_position.x += CHUNK_X_SIZE;
	// if (block_chunk_position.y < 0) block_chunk_position.y += CHUNK_Y_SIZE;
	// if (block_chunk_position.z < 0) block_chunk_position.z += CHUNK_Z_SIZE;

	// return block_chunk_position;

	// use the function from Chunk.hpp
	return ::getBlockChunkPos(glm::ivec3(position));
}

glm::vec3 World::getChunkPosition(const glm::vec3 & position)
{
	// return glm::floor(position / CHUNK_SIZE_VEC3);

	// use the function from Chunk.hpp
	return ::getChunkPos(glm::ivec3(position));
}

std::shared_ptr<Chunk> World::getChunk(const glm::ivec3 & position) const
{
	std::lock_guard lock(m_chunks_mutex);
	LockMark(m_chunks_mutex);
	return getChunkNoLock(position);
}

std::shared_ptr<Chunk> World::getChunkNoLock(const glm::ivec3 & position) const
{
	auto it = m_chunks.find(position);
	if (it != m_chunks.end())
		return it->second;
	return nullptr;
}

void World::insertChunk(const glm::ivec3 & position, std::shared_ptr<Chunk> chunk)
{
	std::lock_guard lock(m_chunks_mutex);
	auto ret =	m_chunks.insert(std::make_pair(position, chunk));
	if (!ret.second)
	{
		LOG_ERROR("Failed to insert chunk");
	}

	if (!m_chunks.contains(position))
	{
		LOG_CRITICAL("IDK WTF");
	}
}

void World::insertChunkNoLock(const glm::ivec3 & position, std::shared_ptr<Chunk> chunk)
{
	auto ret =	m_chunks.insert(std::make_pair(position, chunk));
	if (!ret.second)
	{
		LOG_ERROR("Failed to insert chunk");
	}

	if (!m_chunks.contains(position))
	{
		LOG_CRITICAL("IDK WTF");
	}
}

// void World::waitForFinishedFutures()
// {
// 	ZoneScoped;
// 	std::lock_guard lock(m_finished_futures_mutex);
// 	while(!m_finished_futures.empty())
// 	{
// 		uint64_t id = m_finished_futures.front();
// 		m_finished_futures.pop();
// 		auto & future = m_futures.at(id);
// 		future.get();
// 		m_futures.erase(id);
// 	}
// }

// void World::waitForFutures()
// {
// 	while(!m_futures.empty())
// 	{
// 		m_futures.begin()->second.get();
// 		m_futures.erase(m_futures.begin());
// 	}
// }

static bool insideSkyLightInfluenceZone(const glm::ivec3 & origin, const glm::ivec3 & position)
{
	const glm::ivec3 diff = glm::abs(position - origin);
	const bool is_below = position.y <= origin.y;
	// return is_below ? (diff.x + diff.z <= 16) : (diff.x + diff.y + diff.z <= 16);
	return (is_below && (diff.x + diff.z <= 16)) || (!is_below && (diff.x + diff.y + diff.z <= 16));
}

std::unordered_set<glm::ivec3> World::updateSkyLight(const glm::ivec3 & start_block_world_pos)
{
	/*
	 * https://minecraft.fandom.com/wiki/Light#Sky_light
	 *
	 * When sky light of a level of 15 spreads down through a transparent block, the level remains unchanged.
	 * When it spreads horizontally or upward, it reduces 1 light level.
	 * However, when it spreads through a light-filtering block, it does not follow the above two rules and attenuates specific light levels.
	 * Sky light with a level less than 15 spreads as block light - when it propagates to adjacent (including top and bottom, six blocks in total) blocks,
	 * it is attenuated until it is 0.
	 *
	 * This implementation reduces the light level by 1 even when passing through a light-filtering block.
	*/
	std::unordered_set<glm::ivec3> modified_chunks;
	const glm::ivec3 start_chunk_pos = getChunkPos(start_block_world_pos);
	std::shared_ptr<Chunk> start_chunk = getChunk(start_chunk_pos);
	if (start_chunk == nullptr)
	{
		LOG_WARNING("World::updateSkyLight: start_chunk is nullptr.");
		return modified_chunks;
	}

	constexpr std::array<glm::ivec3, 6> NEIGHBOR_OFFSETS = {
		glm::ivec3(1, 0, 0),
		glm::ivec3(-1, 0, 0),
		glm::ivec3(0, 1, 0),
		glm::ivec3(0, -1, 0),
		glm::ivec3(0, 0, 1),
		glm::ivec3(0, 0, -1)
	};

	std::queue<glm::ivec3> light_source_queue;
	std::queue<glm::ivec3> start_light_queue;
	std::unordered_set<glm::ivec3> visited_blocks;
	start_light_queue.push(start_block_world_pos);

	while (!start_light_queue.empty())
	{
		if (start_light_queue.size() > BLOCKS_PER_CHUNK)
		{
			LOG_WARNING("World::updateSkyLight: first loop: start_light_queue size > BLOCKS_PER_CHUNK (" << BLOCKS_PER_CHUNK << ")");
			break;
		}

		const glm::ivec3 current_block_world_pos = start_light_queue.front();
		start_light_queue.pop();

		const glm::ivec3 current_chunk_pos = getChunkPos(current_block_world_pos);
		const glm::ivec3 current_block_chunk_pos = getBlockChunkPos(current_block_world_pos);
		const std::shared_ptr<Chunk> current_chunk = getChunk(current_chunk_pos);
		if (current_chunk == nullptr)
		{
			LOG_WARNING("World::updateSkyLight: first loop: visited chunk is nullptr.");
			continue;
		}

		current_chunk->status.lock_shared();
		const BlockInfo::Type current_id = current_chunk->getBlock(current_block_chunk_pos);
		current_chunk->status.unlock_shared();
		const int current_absorb_light = g_blocks_info.get(current_id).absorb_light;
		int current_light = 0;

		for (int i = 0; i < 6; i++)
		{
			const glm::ivec3 neighbor_world_pos = current_block_world_pos + NEIGHBOR_OFFSETS[i];
			const glm::ivec3 neighbor_chunk_pos = getChunkPos(neighbor_world_pos);
			const glm::ivec3 neighbor_block_chunk_pos = getBlockChunkPos(neighbor_world_pos);

			// if current is at the top of the world
			if (neighbor_chunk_pos.y == 1)
			{
				if (!g_blocks_info.hasProperty(current_id, BLOCK_PROPERTY_OPAQUE) && visited_blocks.find(neighbor_world_pos) == visited_blocks.end())
				{
					const int current_new_light = 15 - current_absorb_light;
					if (current_new_light > current_light)
					{
						current_light = current_new_light;
					}
				}
				visited_blocks.insert(neighbor_world_pos);
				continue;
			}

			if (neighbor_chunk_pos.y != 0)
				continue;

			std::shared_ptr<Chunk> neighbor_chunk = getChunk(neighbor_chunk_pos);
			if (neighbor_chunk == nullptr)
			{
				LOG_WARNING("World::updateSkyLight: first loop: neighbor_chunk is nullptr.");
				continue;
			}

			neighbor_chunk->status.lock_shared();
			const BlockInfo::Type neighbor_id = neighbor_chunk->getBlock(neighbor_block_chunk_pos);
			const int neighbor_light = neighbor_chunk->getSkyLight(neighbor_block_chunk_pos);
			neighbor_chunk->status.unlock_shared();

			if (!g_blocks_info.hasProperty(neighbor_id, BLOCK_PROPERTY_OPAQUE) && visited_blocks.find(neighbor_world_pos) == visited_blocks.end())
			{
				if (insideSkyLightInfluenceZone(start_block_world_pos, neighbor_world_pos))
				{
					start_light_queue.push(neighbor_world_pos);
				}
				else
				{
					int current_new_light = neighbor_light - current_absorb_light
						- (i != 2 || neighbor_light != 15) * 1; // i == 2 is when the neighbor light spreads down
					current_new_light = std::max(current_new_light, 0);

					if (current_new_light > 0)
					{
						light_source_queue.push(current_block_world_pos);
					}
				}
				visited_blocks.insert(neighbor_world_pos);
			}
		}

		current_chunk->status.lock();
		current_chunk->setSkyLight(current_block_chunk_pos, current_light);
		current_chunk->status.unlock();

		if (current_light > 0)
		{
			light_source_queue.push(current_block_world_pos);
		}
	}

	while (!light_source_queue.empty())
	{
		if (light_source_queue.size() > BLOCKS_PER_CHUNK)
		{
			LOG_WARNING("World::updateSkyLight: second loop: light_source_queue size > BLOCKS_PER_CHUNK (" << BLOCKS_PER_CHUNK << ")");
			break;
		}

		const glm::ivec3 current_block_world_pos = light_source_queue.front();
		light_source_queue.pop();

		const glm::ivec3 chunk_pos = getChunkPos(current_block_world_pos);
		const glm::ivec3 block_chunk_pos = getBlockChunkPos(current_block_world_pos);
		const std::shared_ptr<Chunk> chunk = getChunk(chunk_pos);
		if (chunk == nullptr)
		{
			LOG_WARNING("World::updateSkyLight: second loop: current_chunk is nullptr.");
			continue;
		}

		chunk->status.lock_shared();
		const BlockInfo::Type current_id = chunk->getBlock(block_chunk_pos);
		// not const because it can change when looking at neighbors
		int current_light = chunk->getSkyLight(block_chunk_pos);
		chunk->status.unlock_shared();
		const int current_absorb_light = g_blocks_info.get(current_id).absorb_light;

		for (int i = 0; i < 6; i++)
		{
			const glm::ivec3 neighbor_world_pos = current_block_world_pos + NEIGHBOR_OFFSETS[i];
			const glm::ivec3 neighbor_chunk_pos = getChunkPos(neighbor_world_pos);
			const glm::ivec3 neighbor_block_chunk_pos = getBlockChunkPos(neighbor_world_pos);

			if (neighbor_chunk_pos.y != 0)
				continue;

			std::shared_ptr<Chunk> neighbor_chunk = getChunk(neighbor_chunk_pos);
			if (neighbor_chunk == nullptr)
			{
				LOG_WARNING("World::updateSkyLight: second loop: neighbor_chunk is nullptr.");
				continue;
			}

			neighbor_chunk->status.lock();
			const BlockInfo::Type neighbor_id = neighbor_chunk->getBlock(neighbor_block_chunk_pos);
			const int neighbor_light = neighbor_chunk->getSkyLight(neighbor_block_chunk_pos);
			neighbor_chunk->status.unlock();

			if (!g_blocks_info.hasProperty(neighbor_id, BLOCK_PROPERTY_OPAQUE))
			{
				const int neighbor_absorbed_light = g_blocks_info.get(neighbor_id).absorb_light;
				const int neighbor_new_light = current_light - neighbor_absorbed_light
					- (i != 3 || current_light != 15) * 1; // i == 3 is when the current light spreads down
				const int current_new_light = neighbor_light - current_absorb_light
					- (i != 2 || neighbor_light != 15) * 1; // i == 2 is when the neighbor light spreads down

				if (neighbor_new_light > neighbor_light)
				{
					neighbor_chunk->status.lock();
					neighbor_chunk->setSkyLight(neighbor_block_chunk_pos, neighbor_new_light);
					neighbor_chunk->setMeshed(false);
					modified_chunks.insert(neighbor_chunk_pos);
					neighbor_chunk->status.unlock();
					light_source_queue.push(neighbor_world_pos);
				}
				else if (current_new_light > current_light)
				{
					chunk->status.lock();
					chunk->setSkyLight(block_chunk_pos, current_new_light);
					chunk->status.unlock();
					light_source_queue.push(current_block_world_pos);
					current_light = current_new_light;
				}
			}
		}
	}
	return modified_chunks;
}

static bool insideBlockLightInfluenceZone(const glm::ivec3 & origin, const glm::ivec3 & position)
{
	const glm::ivec3 diff = glm::abs(position - origin);
	return diff.x + diff.y + diff.z <= 16;
}

std::unordered_set<glm::ivec3> World::updateBlockLight(const glm::ivec3 & start_block_world_pos)
{
	std::unordered_set<glm::ivec3> modified_chunks;
	const glm::ivec3 start_chunk_pos = getChunkPos(start_block_world_pos);
	std::shared_ptr<Chunk> start_chunk = getChunk(start_chunk_pos);
	if (start_chunk == nullptr)
	{
		LOG_WARNING("World::updateBlockLight: start_chunk is nullptr.");
		return modified_chunks;
	}

	constexpr std::array<glm::ivec3, 6> NEIGHBOR_OFFSETS = {
		glm::ivec3(1, 0, 0),
		glm::ivec3(-1, 0, 0),
		glm::ivec3(0, 1, 0),
		glm::ivec3(0, -1, 0),
		glm::ivec3(0, 0, 1),
		glm::ivec3(0, 0, -1)
	};

	std::queue<glm::ivec3> light_source_queue;
	std::queue<glm::ivec3> start_light_queue;
	std::unordered_set<glm::ivec3> visited_blocks;
	start_light_queue.push(start_block_world_pos);

	while (!start_light_queue.empty())
	{
		if (start_light_queue.size() > BLOCKS_PER_CHUNK)
		{
			LOG_WARNING("World::updateBlockLight: first loop: start_light_queue size > BLOCKS_PER_CHUNK (" << BLOCKS_PER_CHUNK << ")");
			break;
		}

		const glm::ivec3 current_block_world_pos = start_light_queue.front();
		start_light_queue.pop();

		const glm::ivec3 current_chunk_pos = getChunkPos(current_block_world_pos);
		const glm::ivec3 current_block_chunk_pos = getBlockChunkPos(current_block_world_pos);
		const std::shared_ptr<Chunk> current_chunk = getChunk(current_chunk_pos);
		if (current_chunk == nullptr)
		{
			LOG_WARNING("World::updateBlockLight: first loop: visited chunk is nullptr.");
			continue;
		}

		current_chunk->status.lock_shared();
		const BlockInfo::Type current_id = current_chunk->getBlock(current_block_chunk_pos);
		const int current_absorb_light = g_blocks_info.get(current_id).absorb_light;
		const int current_emit_light = g_blocks_info.get(current_id).emit_light;
		int current_light = current_emit_light;
		current_chunk->setMeshed(false);
		current_chunk->status.unlock_shared();
		modified_chunks.insert(current_chunk_pos);

		for (int i = 0; i < 6; i++)
		{
			const glm::ivec3 neighbor_world_pos = current_block_world_pos + NEIGHBOR_OFFSETS[i];
			const glm::ivec3 neighbor_chunk_pos = getChunkPos(neighbor_world_pos);
			const glm::ivec3 neighbor_block_chunk_pos = getBlockChunkPos(neighbor_world_pos);

			if (neighbor_chunk_pos.y != 0)
				continue;

			std::shared_ptr<Chunk> neighbor_chunk = getChunk(neighbor_chunk_pos);
			if (neighbor_chunk == nullptr)
			{
				LOG_WARNING("World::updateBlockLight: first loop: neighbor_chunk is nullptr.");
				continue;
			}

			neighbor_chunk->status.lock_shared();
			const BlockInfo::Type neighbor_id = neighbor_chunk->getBlock(neighbor_block_chunk_pos);
			const int neighbor_light = neighbor_chunk->getBlockLight(neighbor_block_chunk_pos);
			neighbor_chunk->status.unlock_shared();
			const BlockProperties neighbor_properties = g_blocks_info.get(neighbor_id).properties;

			if ((!(neighbor_properties & BLOCK_PROPERTY_OPAQUE) || (neighbor_properties & BLOCK_PROPERTY_LIGHT))
				&& visited_blocks.find(neighbor_world_pos) == visited_blocks.end())
			{
				if (insideBlockLightInfluenceZone(start_block_world_pos, neighbor_world_pos))
				{
					start_light_queue.push(neighbor_world_pos);
				}
				else
				{
					const int current_new_light = neighbor_light - current_absorb_light - 1;
					if (current_new_light > current_light)
					{
						current_light = std::max(current_new_light, 0);
					}
				}
				visited_blocks.insert(neighbor_world_pos);
			}
		}

		current_chunk->status.lock();
		current_chunk->setBlockLight(current_block_chunk_pos, current_light);
		current_chunk->status.unlock();
		if (current_light > 0)
		{
			light_source_queue.push(current_block_world_pos);
		}
	}

	while (!light_source_queue.empty())
	{
		if (light_source_queue.size() > BLOCKS_PER_CHUNK)
		{
			LOG_WARNING("World::updateBlockLight: second loop: light_source_queue size > BLOCKS_PER_CHUNK (" << BLOCKS_PER_CHUNK << ")");
			break;
		}

		const glm::ivec3 current_block_world_pos = light_source_queue.front();
		light_source_queue.pop();

		const glm::ivec3 chunk_pos = getChunkPos(current_block_world_pos);
		const glm::ivec3 block_chunk_pos = getBlockChunkPos(current_block_world_pos);
		const std::shared_ptr<Chunk> chunk = getChunk(chunk_pos);
		if (chunk == nullptr)
		{
			LOG_WARNING("World::updateBlockLight: second loop: current_chunk is nullptr.");
			continue;
		}

		chunk->status.lock_shared();
		const BlockInfo::Type current_id = chunk->getBlock(block_chunk_pos);
		// not const because it can change when looking at neighbors
		int current_light = chunk->getBlockLight(block_chunk_pos);
		chunk->status.unlock_shared();
		const int current_absorb_light = g_blocks_info.get(current_id).absorb_light;

		for (int i = 0; i < 6; i++)
		{
			const glm::ivec3 neighbor_world_pos = current_block_world_pos + NEIGHBOR_OFFSETS[i];
			const glm::ivec3 neighbor_chunk_pos = getChunkPos(neighbor_world_pos);
			const glm::ivec3 neighbor_block_chunk_pos = getBlockChunkPos(neighbor_world_pos);

			if (neighbor_chunk_pos.y != 0)
				continue;

			std::shared_ptr<Chunk> neighbor_chunk = getChunk(neighbor_chunk_pos);
			if (neighbor_chunk == nullptr)
			{
				LOG_WARNING("World::updateBlockLight: second loop: neighbor_chunk is nullptr.");
				continue;
			}

			neighbor_chunk->status.lock();
			const BlockInfo::Type neighbor_id = neighbor_chunk->getBlock(neighbor_block_chunk_pos);
			const int neighbor_light = neighbor_chunk->getBlockLight(neighbor_block_chunk_pos);
			neighbor_chunk->status.unlock();

			if (!g_blocks_info.hasProperty(neighbor_id, BLOCK_PROPERTY_OPAQUE))
			{
				const int neighbor_absorbed_light = g_blocks_info.get(neighbor_id).absorb_light;
				const int neighbor_new_light = current_light - neighbor_absorbed_light - 1;
				const int current_new_light = neighbor_light - current_absorb_light - 1;

				if (neighbor_new_light > neighbor_light)
				{
					neighbor_chunk->status.lock();
					neighbor_chunk->setBlockLight(neighbor_block_chunk_pos, neighbor_new_light);
					neighbor_chunk->status.unlock();
					light_source_queue.push(neighbor_world_pos);
				}
				else if (current_new_light > current_light)
				{
					chunk->status.lock();
					chunk->setBlockLight(block_chunk_pos, current_new_light);
					chunk->status.unlock();
					light_source_queue.push(current_block_world_pos);
					current_light = current_new_light;
				}
			}
		}
	}
	return modified_chunks;
}
