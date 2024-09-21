#include "WorldGenerator.hpp"

#include <cmath>

Chunk::genLevel WorldGenerator::ticketToGenLevel(int ticket_level)
{
	if (ticket_level <= TICKET_LEVEL_INACTIVE)
		return Chunk::genLevel::CAVE;
	switch (ticket_level)
	{
		case TICKET_LEVEL_INACTIVE:
			return Chunk::genLevel::CAVE;
		case TICKET_LEVEL_INACTIVE + 1:
			return Chunk::genLevel::RELIEF;
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
		default:
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

	//get the start position of the zone the chunk is in
	info.zoneStart.x -= info.zoneStart.x % info.zoneSize.x;
	info.zoneStart.y = 0;
	info.zoneStart.z -= info.zoneStart.z % info.zoneSize.z;

	return info;
}

WorldGenerator::WorldGenerator()
: m_relief_perlin(1, 7, 1, 0.35, 2),
  m_cave_perlin(1, 4, 1, 0.5, 2)
{

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
				chunk->setBlock(blockX, blockY, blockZ, BlockID::Stone);
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
				BlockID to_set;

				{
					//check to see wether above or below the relief value
					if (reliefValue > position.y)
						to_set = BlockID::Stone;
					else if (reliefValue + 5 > position.y)
					{
						if (riverValue < 0.05f)
							to_set = BlockID::Water;
						else 
							to_set = BlockID::Grass;
					}
					else
						to_set = BlockID::Air;
				}
				
				// //if above relief value and below 200 place water
				// if (to_set == BlockID::Air && position.y < 200)
				// 	to_set = BlockID::Water;

				//if below relief value try to carve a cave
				if (to_set != BlockID::Air && to_set != BlockID::Water && generateCaveBlock(position) == BlockID::Air)
					to_set = BlockID::Air;
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
				BlockID to_set;

				// if( position.y == 128)
				// 	to_set = Block::Air;
				// else
				// {
					to_set = generateReliefBlock(position);
					if (to_set != BlockID::Air && generateCaveBlock(position) == BlockID::Air)
						to_set = BlockID::Air;
				// }
				// if (to_set != Block::Air && position.y > 128)
				// {
				// 	to_set = Block::Grass;
				// }
				chunk->setBlock(blockX, blockY, blockZ, to_set);
			}
		}
	}
	return chunk;
}

BlockID WorldGenerator::generateReliefBlock(glm::ivec3 position)
{
	float value = generateReliefValue(glm::ivec2(position.x, position.z));

	value *= CHUNK_Y_SIZE;

	if (value > position.y)
		return BlockID::Stone;
	// else if (value > -0.05f && value < 0.05f)
		// chunk.setBlock(blockX, blockY, blockZ, Block::Air);
	else if (value + 3 > position.y)
		return BlockID::Grass;
	else
		return BlockID::Air;
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

BlockID WorldGenerator::generateCaveBlock(glm::ivec3 position)
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
		return BlockID::Air;

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
		return BlockID::Air;
	else
		return BlockID::Stone;

	return BlockID::Stone;
}

void WorldGenerator::generate(genInfo info, ChunkMap & chunks)
{
	using enum Chunk::genLevel;
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
						//generate the relief value for the whole chunk
						float reliefValue = generateReliefValue(glm::ivec2(
							blockX + chunkPos3D.x * CHUNK_X_SIZE,
							blockZ + chunkPos3D.z * CHUNK_Z_SIZE
						));

						float riverValue = std::abs(reliefValue);

						reliefValue = (reliefValue + 1) / 2;
						reliefValue = pow(2, 10 * reliefValue - 10);
						reliefValue *= (CHUNK_Y_SIZE - 100);
						reliefValue += 100;
						for(int blockY = 0; blockY < CHUNK_Y_SIZE; blockY++)
						{

							glm::ivec3 position = glm::ivec3(
								blockX + chunkPos3D.x * CHUNK_X_SIZE,
								blockY,
								blockZ + chunkPos3D.z * CHUNK_Z_SIZE
							);
							BlockID to_set;

							{
								//check to see wether above or below the relief value
								if (reliefValue > position.y)
									to_set = BlockID::Stone;
								else if (reliefValue + 5 > position.y)
								{
									if (riverValue < 0.05f)
										to_set = BlockID::Water;
									else 
										to_set = BlockID::Grass;
								}
								else
									to_set = BlockID::Air;
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
		//generate caves
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
							BlockID current_block = chunk->getBlock(blockX, blockY, blockZ);
							
							if (current_block == BlockID::Stone && generateCaveBlock(position) == BlockID::Air)
								chunk->setBlock(blockX, blockY, blockZ, BlockID::Stone);
						}
					}
				}
			}
		}
	}
}
