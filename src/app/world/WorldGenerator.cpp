#include "WorldGenerator.hpp"

#include "math_utils.hpp"
#include <cmath>
#include <queue>

Chunk::genLevel WorldGenerator::ticketToGenLevel(int ticket_level)
{
	return LIGHT;
	if (ticket_level <= TICKET_LEVEL_INACTIVE)
		return LIGHT;
	switch (ticket_level)
	{
		case TICKET_LEVEL_INACTIVE:
			return LIGHT;
		case TICKET_LEVEL_INACTIVE + 1:
			return CAVE;
		case TICKET_LEVEL_INACTIVE + 2:
			return RELIEF;
		default:
			throw std::invalid_argument("WorldGenerator::ticketToGenLevel: Invalid ticket level");
	}
}

WorldGenerator::genInfo WorldGenerator::getGenInfo(Chunk::genLevel desired_gen_level, Chunk::genLevel old_gen_level, glm::ivec3 chunkPos3D)
{
	genInfo info;

	info.oldLevel = old_gen_level;

	switch (desired_gen_level)
	{
		case LIGHT:
		{
			info.level = LIGHT;
			info.zoneSize = ZONE_SIZES[static_cast<int>(LIGHT)];
			break;
		}
		case CAVE:
		{
			info.level = CAVE;
			info.zoneSize = ZONE_SIZES[static_cast<int>(CAVE)];
			break;
		}
		case RELIEF:
		{
			info.level = RELIEF;
			info.zoneSize = ZONE_SIZES[static_cast<int>(RELIEF)];
			break;
		}
		case EMPTY:
		{
			throw std::invalid_argument("Invalid desired gen level");
		}
	}

	/*
		gen level 			zone size
		CAVE 				5
		RELIEF				10
		EMPTY				NULL

		if chunk is empty and we need cave level generation,
		we need to do it on a relief sized zone because
		if a chunk has a relief level generation then we have the garantee
		that all the chunks in its zone have at least the same gen level
	*/

	//if the level difference is more than 1, then we need to change the zone size
	if (static_cast<int>(info.oldLevel) - static_cast<int>(info.level) > 1)
		info.zoneSize = ZONE_SIZES[static_cast<int>(info.oldLevel) - 1];

	info.zoneStart = chunkPos3D;
	// LOG_INFO("CHUNKPOS: " << chunkPos3D.x << " " << chunkPos3D.z << " SIZE: " << info.zoneSize.x);
	//get the start position of the zone the chunk is in
	info.zoneStart.y = 0;

	if (info.zoneStart.x < 0 && info.zoneSize.x > 1)
		info.zoneStart.x -= info.zoneSize.x;
	info.zoneStart.x -= info.zoneStart.x % info.zoneSize.x;
	if (info.zoneStart.z < 0 && info.zoneSize.z > 1)
		info.zoneStart.z -= info.zoneSize.z;
	info.zoneStart.z -= info.zoneStart.z % info.zoneSize.z;

	// LOG_INFO("ZONE START: " << info.zoneStart.x << " " << info.zoneStart.z);

	return info;
}

WorldGenerator::WorldGenerator()
: m_relief_perlin(1, 7, 1, 0.35, 2),
  m_cave_perlin(1, 4, 1, 0.5, 2),
  m_continentalness_perlin(10, 7, 1, 0.4, 2)
{
	for(size_t i = 0; i + 1 < ZONE_SIZES.size(); i++)
	{
		if (ZONE_SIZES[i + 1].x % ZONE_SIZES[i].x != 0)
			throw std::logic_error("WorldGenerator: constructor: ZONE_SIZES must be multiples of the previous zone size");
	}
}

WorldGenerator::~WorldGenerator()
{

}

std::shared_ptr<Chunk> WorldGenerator::generateFullChunk(const int & x, const int & y, const int & z)
{
	return generateFullChunk(x, y, z, nullptr);
}

std::shared_ptr<Chunk> WorldGenerator::generateFullChunk(const int & x, const int & y, const int & z, std::shared_ptr<Chunk> chunk)
{
	if (chunk == nullptr)
		chunk = std::make_shared<Chunk>(glm::ivec3(x, y, z));
	(void)x, (void)y;

	for(int blockX = 0; blockX < CHUNK_X_SIZE; blockX++)
	{
		for(int blockY = 0; blockY < CHUNK_Y_SIZE; blockY++)
		{
			for(int blockZ = 0; blockZ < CHUNK_Z_SIZE; blockZ++)
			{
				chunk->setBlock(blockX, blockY, blockZ, BlockInfo::Type::Stone);
			}
		}
	}
	return chunk;
}

std::shared_ptr<Chunk> WorldGenerator::generateChunkColumn(const int & x, const int & z)
{
	return generateChunkColumn(x, z, nullptr);
}

std::shared_ptr<Chunk> WorldGenerator::generateChunkColumn(const int & x, const int & z, std::shared_ptr<Chunk> column)
{
	if (column == nullptr)
		column = std::make_shared<Chunk>(glm::ivec3(x, 0, z));
	// chunk = std::make_shared<Chunk>(glm::ivec3(x, y, z));


	for(int blockX = 0; blockX < CHUNK_X_SIZE; blockX++)
	{
		for(int blockZ = 0; blockZ < CHUNK_Z_SIZE; blockZ++)
		{
			//generate the relief value for the whole column
			float reliefValue = generateReliefValue(glm::ivec2(
				blockX + x * CHUNK_X_SIZE,
				blockZ + z * CHUNK_Z_SIZE
			));

			float riverValue = std::abs(reliefValue);

			reliefValue = (reliefValue + 1) / 2;
			reliefValue = pow(2, 10 * reliefValue - 10);
			reliefValue *= (CHUNK_Y_SIZE - 100);
			reliefValue += 100;
			for(int blockY = 0; blockY < CHUNK_Y_SIZE; blockY++)
			{

				glm::ivec3 position = glm::ivec3(
					blockX + x * CHUNK_X_SIZE,
					blockY,
					blockZ + z * CHUNK_Z_SIZE
				);
				BlockInfo::Type to_set;

				{
					//check to see wether above or below the relief value
					if (reliefValue > position.y)
						to_set = BlockInfo::Type::Stone;
					else if (reliefValue + 5 > position.y)
					{
						if (riverValue < 0.05f)
							to_set = BlockInfo::Type::Water;
						else
							to_set = BlockInfo::Type::Grass;
					}
					else
						to_set = BlockInfo::Type::Air;
				}

				// //if above relief value and below 200 place water
				// if (to_set == BlockInfo::Type::Air && position.y < 200)
				// 	to_set = BlockInfo::Type::Water;

				//if below relief value try to carve a cave
				if (to_set != BlockInfo::Type::Air && to_set != BlockInfo::Type::Water && generateCaveBlock(position) == BlockInfo::Type::Air)
					to_set = BlockInfo::Type::Air;
				// try {
				column->setBlock(blockX, blockY, blockZ, to_set);
				// } catch (std::exception & e)
				// {
					// LOG_ERROR("EXCEPTION: " << e.what());
				// }
			}
		}
	}
	return column;
}

std::shared_ptr<Chunk> WorldGenerator::generateChunk(const int & x, const int & y, const int & z, std::shared_ptr<Chunk> chunk)
{
	if (chunk == nullptr)
		chunk = std::make_shared<Chunk>(glm::ivec3(x, y, z));
	(void)x, (void)y;

	for(int blockX = 0; blockX < CHUNK_X_SIZE; blockX++)
	{
		for(int blockY = 0; blockY < CHUNK_Y_SIZE; blockY++)
		{
			for(int blockZ = 0; blockZ < CHUNK_Z_SIZE; blockZ++)
			{
				glm::ivec3 position = glm::ivec3(
					blockX + x * CHUNK_X_SIZE,
					blockY + y * CHUNK_Y_SIZE,
					blockZ + z * CHUNK_Z_SIZE
				);
				BlockInfo::Type to_set;

				// if( position.y == 128)
				// 	to_set = BlockInfo::Air;
				// else
				// {
					to_set = generateReliefBlock(position);
					if (to_set != BlockInfo::Type::Air && generateCaveBlock(position) == BlockInfo::Type::Air)
						to_set = BlockInfo::Type::Air;
				// }
				// if (to_set != BlockInfo::Air && position.y > 128)
				// {
				// 	to_set = BlockInfo::Grass;
				// }
				chunk->setBlock(blockX, blockY, blockZ, to_set);
			}
		}
	}
	return chunk;
}

BlockInfo::Type WorldGenerator::generateReliefBlock(glm::ivec3 position)
{
	float value = generateReliefValue(glm::ivec2(position.x, position.z));

	value *= CHUNK_Y_SIZE;

	if (value > position.y)
		return BlockInfo::Type::Stone;
	// else if (value > -0.05f && value < 0.05f)
		// chunk.setBlock(blockX, blockY, blockZ, BlockInfo::Air);
	else if (value + 3 > position.y)
		return BlockInfo::Type::Grass;
	else
		return BlockInfo::Type::Air;
}

/**
 * @brief
 *
 * @param position
 * @return float [-1, 1]
 */
float WorldGenerator::generateReliefValue(glm::ivec2 position)
{
	float value = m_relief_perlin.noise(glm::vec2(
		position.x * 0.0080f,
		position.y * 0.0080f
	));


	// value = (value + 1) / 2;

	// value = pow(2, 10 * value - 10);
	// value = value < 0.5 ? 4 * value * value * value : 1 - pow(-2 * value + 2, 3) / 2;

	// value = pow(2, value);

	// constexpr double slope = 1.0 * (1 - 0) / (2 - 0.5);
	// value = slope * (value - 0.5);
	//value is back to [0, 1]


	return value;
}

BlockInfo::Type WorldGenerator::generateCaveBlock(glm::ivec3 position)
{


	//spaghetti caves
	float valueA = m_cave_perlin.noise(glm::vec3(
		position.x * 0.01f,
		position.y * 0.016f,
		position.z * 0.01f
	));

	float valueB = m_cave_perlin.noise(glm::vec3(
		position.x * 0.01f,
		(position.y + 42) * 0.016f,
		position.z * 0.01f
	));

	float threshold = 0.002f;
	float value = valueA * valueA + valueB * valueB;
	// value /= threshold;

	// m_avg += value;
	// if (value > m_max)
	// 	m_max = value;
	// if (value < m_min)
	// 	m_min = value;
	// m_called++;

	if (value < threshold)
		return BlockInfo::Type::Air;

	// else try to create cheese cave

	valueA = (valueA + 1) / 2;
	valueA += 0.01f;

	//bias value to have less chance of a cave the higher the y value
	//for now the transition layer will be between 110 and 128

	float edge = 0.05f;

	if (position.y > 100 && position.y < 140)
		edge = 0.025f;
	else if (position.y >= 140 && position.y < 141)
		edge = 0.02f;
	else if (position.y >= 141 && position.y < 142)
		edge = 0.015f;
	else if (position.y >= 142 && position.y < 143)
		edge = 0.01f;
	else if (position.y >= 143)
		edge = 0;

	if (valueA > 0 && valueA < edge)
		return BlockInfo::Type::Air;
	else
		return BlockInfo::Type::Stone;

	return BlockInfo::Type::Stone;
}

static void setSkyLight(
	ChunkMap & chunks,
	const glm::ivec3 & chunk_pos
)
{
	std::shared_ptr<Chunk> start_chunk = chunks.at(chunk_pos);

	std::queue<glm::ivec3> light_queue;
	for (int x = 0; x < CHUNK_X_SIZE; x++)
	{
		for (int z = 0; z < CHUNK_Z_SIZE; z++)
		{
			const glm::ivec3 block_chunk_pos = glm::ivec3(x, CHUNK_Y_SIZE - 1, z);
			const glm::ivec3 block_world_pos = chunk_pos * CHUNK_SIZE_IVEC3 + block_chunk_pos;
			const BlockInfo::Type block_id = start_chunk->getBlock(block_chunk_pos);
			if (!g_blocks_info.hasProperty(block_id, BLOCK_PROPERTY_OPAQUE))
			{
				const int absorbed_light = g_blocks_info.get(block_id).absorb_light;
				const int new_light = 15 - absorbed_light;
				start_chunk->setSkyLight(block_chunk_pos, new_light);
				light_queue.push(block_world_pos);
			}
		}
	}

	constexpr std::array<glm::ivec3, 6> NEIGHBOR_OFFSETS = {
		glm::ivec3(1, 0, 0),
		glm::ivec3(-1, 0, 0),
		glm::ivec3(0, 1, 0),
		glm::ivec3(0, -1, 0),
		glm::ivec3(0, 0, 1),
		glm::ivec3(0, 0, -1)
	};

	while (!light_queue.empty())
	{
		const glm::ivec3 current_block_world_pos = light_queue.front();
		light_queue.pop();

		const glm::ivec3 chunk_pos = getChunkPos(current_block_world_pos);
		const glm::ivec3 block_chunk_pos = getBlockChunkPos(current_block_world_pos);
		std::shared_ptr<Chunk> chunk = chunks.at(chunk_pos);

		// const BlockInfo::Type current_id = chunk->getBlock(block_chunk_pos);
		const int current_light = chunk->getSkyLight(block_chunk_pos);

		if (current_light == 0)
			continue;

		for (int i = 0; i < 6; i++)
		{
			const glm::ivec3 neighbor_world_pos = current_block_world_pos + NEIGHBOR_OFFSETS[i];
			const glm::ivec3 neighbor_chunk_pos = getChunkPos(neighbor_world_pos);
			const glm::ivec3 neighbor_block_chunk_pos = getBlockChunkPos(neighbor_world_pos);

			if (chunks.contains(neighbor_chunk_pos))
			{
				std::shared_ptr<Chunk> neighbor_chunk = chunks.at(neighbor_chunk_pos);
				const BlockInfo::Type neighbor_id = neighbor_chunk->getBlock(neighbor_block_chunk_pos);
				const int neighbor_light = neighbor_chunk->getSkyLight(neighbor_block_chunk_pos);

				if (!g_blocks_info.hasProperty(neighbor_id, BLOCK_PROPERTY_OPAQUE))
				{
					const int absorbed_light = g_blocks_info.get(neighbor_id).absorb_light;
					const int new_light = current_light - absorbed_light - (i == 3 ? 0 : 1); // -1 if not below
					if (neighbor_light < new_light)
					{
						neighbor_chunk->setSkyLight(neighbor_block_chunk_pos, new_light);
						light_queue.push(neighbor_world_pos);
					}
				}
			}
		}
	}
}

static void setBlockLight(
	ChunkMap & chunks,
	const glm::ivec3 & chunk_pos
)
{
	std::shared_ptr<Chunk> start_chunk = chunks.at(chunk_pos);

	std::queue<glm::ivec3> light_queue;
	for (int x = 0; x < CHUNK_X_SIZE; x++)
	{
		for (int y = 0; y < CHUNK_Y_SIZE; y++)
		{
			for (int z = 0; z < CHUNK_Z_SIZE; z++)
			{
				const glm::ivec3 block_chunk_pos = glm::ivec3(x, y, z);
				const glm::ivec3 block_world_pos = chunk_pos * CHUNK_SIZE_IVEC3 + block_chunk_pos;
				const BlockInfo::Type block_id = start_chunk->getBlock(block_chunk_pos);
				if (g_blocks_info.hasProperty(block_id, BLOCK_PROPERTY_LIGHT))
				{
					const int emit_light = g_blocks_info.get(block_id).emit_light;
					start_chunk->setBlockLight(block_chunk_pos, emit_light);
					light_queue.push(block_world_pos);
				}
			}
		}
	}

	constexpr std::array<glm::ivec3, 6> NEIGHBOR_OFFSETS = {
		glm::ivec3(1, 0, 0),
		glm::ivec3(-1, 0, 0),
		glm::ivec3(0, 1, 0),
		glm::ivec3(0, -1, 0),
		glm::ivec3(0, 0, 1),
		glm::ivec3(0, 0, -1)
	};

	while (!light_queue.empty())
	{
		const glm::ivec3 current_block_world_pos = light_queue.front();
		light_queue.pop();

		const glm::ivec3 chunk_pos = getChunkPos(current_block_world_pos);
		const glm::ivec3 block_chunk_pos = getBlockChunkPos(current_block_world_pos);
		std::shared_ptr<Chunk> chunk = chunks.at(chunk_pos);

		// const BlockInfo::Type current_id = chunk->getBlock(block_chunk_pos);
		const int current_light = chunk->getBlockLight(block_chunk_pos);

		if (current_light == 0)
			continue;

		for (int i = 0; i < 6; i++)
		{
			const glm::ivec3 neighbor_world_pos = current_block_world_pos + NEIGHBOR_OFFSETS[i];
			const glm::ivec3 neighbor_chunk_pos = getChunkPos(neighbor_world_pos);
			const glm::ivec3 neighbor_block_chunk_pos = getBlockChunkPos(neighbor_world_pos);

			if (chunks.contains(neighbor_chunk_pos))
			{
				std::shared_ptr<Chunk> neighbor_chunk = chunks.at(neighbor_chunk_pos);
				const BlockInfo::Type neighbor_id = neighbor_chunk->getBlock(neighbor_block_chunk_pos);
				const int neighbor_light = neighbor_chunk->getBlockLight(neighbor_block_chunk_pos);

				if (!g_blocks_info.hasProperty(neighbor_id, BLOCK_PROPERTY_OPAQUE))
				{
					const int absorbed_light = g_blocks_info.get(neighbor_id).absorb_light;
					const int new_light = current_light - absorbed_light - 1;
					if (neighbor_light < new_light)
					{
						neighbor_chunk->setBlockLight(neighbor_block_chunk_pos, new_light);
						light_queue.push(neighbor_world_pos);
					}
				}
			}
		}
	}
}

void WorldGenerator::generate(genInfo info, ChunkMap & chunks)
{
	if (info.level <= RELIEF && info.oldLevel > RELIEF)
	{
		for(int x = 0; x < info.zoneSize.x; x++)
		{
			for(int z = 0; z < info.zoneSize.z; z++)
			{
				glm::ivec3 chunkPos3D = info.zoneStart + glm::ivec3(x, 0, z);
				std::shared_ptr<Chunk> chunk = chunks.at(chunkPos3D);
				chunk->setGenLevel(RELIEF);
				for(int blockX = 0; blockX < CHUNK_X_SIZE; blockX++)
				{
					for(int blockZ = 0; blockZ < CHUNK_Z_SIZE; blockZ++)
					{
						// check the continentalness of the chunk
						float continentalness = m_continentalness_perlin.noise(glm::vec2(
							(blockX + chunkPos3D.x * CHUNK_X_SIZE) * 0.00090f,
							(blockZ + chunkPos3D.z * CHUNK_Z_SIZE) * 0.00090f
						));

						// continentalness *= 4; // map from [-1, 1] to [-4, 4]

						//generate the relief value for the whole chunk
						float reliefValue = generateReliefValue(glm::ivec2(
							blockX + chunkPos3D.x * CHUNK_X_SIZE,
							blockZ + chunkPos3D.z * CHUNK_Z_SIZE
						));

						float riverValue = std::abs(reliefValue);

						reliefValue = (reliefValue + 1) / 2; // map from [-1, 1] to [0, 1]
						reliefValue = pow(2, 10 * reliefValue - 10); // map from [0, 1] to [0, 1] with a slope
						
						float oceanReliefValue = (reliefValue * -60) + 60; // map from [0, 1] to [60, 0]
						float landReliefValue = (reliefValue * (CHUNK_Y_SIZE - 80)) + 80; // map from [0, 1] to [80, CHUNK_Y_SIZE]

						bool isLand = continentalness > 0.2f;
						bool isOcean = continentalness < 0.00f;
						bool isCoast = !isLand && !isOcean;

						if (isOcean)
						{
							reliefValue = oceanReliefValue;
						}
						else if (isCoast)
						{
							// reliefValue *= (CHUNK_Y_SIZE - 100); // map from [0, 1] to [0, CHUNK_Y_SIZE - 100]
							// reliefValue += 100; // map from [0, CHUNK_Y_SIZE - 100] to [100, CHUNK_Y_SIZE]
							reliefValue = std::lerp(oceanReliefValue, landReliefValue, mapRange(continentalness, 0.0f, 0.2f, 0.0f, 1.0f));
						}
						else if (isLand)
						{
							// reliefValue *= (CHUNK_Y_SIZE - 100); // map from [0, 1] to [0, CHUNK_Y_SIZE - 100]
							// reliefValue += 100; // map from [0, CHUNK_Y_SIZE - 100] to [100, CHUNK_Y_SIZE]
							reliefValue = landReliefValue;
						}

						for(int blockY = 0; blockY < CHUNK_Y_SIZE; blockY++)
						{

							glm::ivec3 position = glm::ivec3(
								blockX + chunkPos3D.x * CHUNK_X_SIZE,
								blockY,
								blockZ + chunkPos3D.z * CHUNK_Z_SIZE
							);
							BlockID to_set = BlockID::Air;

							{
								if (isOcean)
								{
									if (position.y < reliefValue)
										to_set = BlockID::Stone;
									else if (position.y < 80)
										to_set = BlockID::Water;
									else
										to_set = BlockID::Air;
								}
								else if (isCoast)
								{
									if (position.y > 80)
									{
										if (position.y < reliefValue - 5)
											to_set = BlockID::Stone;
										else if (position.y < reliefValue - 1 )
											to_set = BlockID::Dirt;
										else if (position.y < reliefValue)
											to_set = BlockID::Grass;
										else
											to_set = BlockID::Air;
									}
									else
									{
										if (position.y < reliefValue)
											to_set = BlockID::Stone;
										else
											to_set = BlockID::Water;
									}
								}
								else if (isLand)
								{
									(void)riverValue;
									//check to see wether above or below the relief value
									// if (reliefValue > position.y)
										// to_set = BlockID::Stone;
									// else if (reliefValue + 5 > position.y)
									// {
									// 	if (riverValue < 0.05f)
									// 		to_set = BlockID::Water;
									// 	else
									// 		to_set = BlockID::Grass;
									// }
									// else
										// to_set = BlockID::Air;
									if (position.y < reliefValue - 5)
										to_set = BlockID::Stone;
									else if (position.y < reliefValue - 1)
										to_set = BlockID::Dirt;
									else if (position.y < reliefValue)
										to_set = BlockID::Grass;
									else
										to_set = BlockID::Air;
								}
								// if (isCoast)
								// 	to_set = BlockID::Glass;
							}
							chunk->setBlock(blockX, blockY, blockZ, to_set);
						}
					}
				}
			}
		}
	}
	if (info.level <= CAVE && info.oldLevel > CAVE)
	{
		for(int x = 0; x < info.zoneSize.x; x++)
		{
			for(int z = 0; z < info.zoneSize.z; z++)
			{
				glm::ivec3 chunkPos3D = info.zoneStart + glm::ivec3(x, 0, z);
				std::shared_ptr<Chunk> chunk = chunks.at(chunkPos3D);
				chunk->setGenLevel(CAVE);

				for(int blockX = 0; blockX < CHUNK_X_SIZE; blockX++)
				{
					for(int blockZ = 0; blockZ < CHUNK_Z_SIZE; blockZ++)
					{
						for(int blockY = 0; blockY < CHUNK_Y_SIZE; blockY++)
						{

							glm::ivec3 position = glm::ivec3(
								blockX + chunkPos3D.x * CHUNK_X_SIZE,
								blockY,
								blockZ + chunkPos3D.z * CHUNK_Z_SIZE
							);
							BlockInfo::Type current_block = chunk->getBlock(blockX, blockY, blockZ);

							if (current_block != BlockInfo::Type::Air && generateCaveBlock(position) == BlockInfo::Type::Air)
								chunk->setBlock(blockX, blockY, blockZ, BlockInfo::Type::Air);
						}
					}
				}
			}
		}
	}
	if (info.level <= LIGHT && info.oldLevel > LIGHT)
	{
		for(int x = 0; x < info.zoneSize.x; x++)
		{
			for(int z = 0; z < info.zoneSize.z; z++)
			{
				glm::ivec3 chunk_pos_3D = info.zoneStart + glm::ivec3(x, 0, z);
				std::shared_ptr<Chunk> chunk = chunks.at(chunk_pos_3D);
				chunk->setGenLevel(LIGHT);

				setSkyLight(chunks, chunk_pos_3D);
				setBlockLight(chunks, chunk_pos_3D);
			}
		}
	}
}
