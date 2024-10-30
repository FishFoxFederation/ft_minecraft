// #define GLM_FORCE_XYZW_ONLY
#include "WorldGenerator.hpp"
#include "World.hpp"
#include "VulkanAPI.hpp"

#include "math_utils.hpp"
#include <cmath>
#include <queue>

World::WorldGenerator::WorldGenerator(World & world)
: m_relief_perlin(1, 7, 1, 0.35, 2),
  m_cave_perlin(1, 4, 1, 0.5, 2),
  m_continentalness_perlin(10, 4, 0.003, 0.4, 2),
  m_erosion_perlin(5, 2, 0.005, 0.5, 2),
  m_weirdness_perlin(42, 2, 0.004, 0.5, 1.5),
  m_temperature_perlin(7, 1, 0.005, 0.5, 2),
  m_humidity_perlin(17, 1, 0.005, 0.5, 2),
  m_world(world)
{
	// for(size_t i = 0; i + 1 < ZONE_SIZES.size(); i++)
	// {
	// 	if (ZONE_SIZES[i + 1].x % ZONE_SIZES[i].x != 0)
	// 		throw std::logic_error("World::WorldGenerator: constructor: ZONE_SIZES must be multiples of the previous zone size");
	// }
}

World::WorldGenerator::~WorldGenerator()
{
}

/**
 * @brief
 *
 * @param position
 * @return float [-1, 1]
 */
float World::WorldGenerator::generateReliefValue(glm::ivec2 position)
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

BlockInfo::Type World::WorldGenerator::generateCaveBlock(glm::ivec3 position)
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

	std::queue<glm::ivec3> light_source_queue;
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
				light_source_queue.push(block_world_pos);
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

	while (!light_source_queue.empty())
	{
		if (light_source_queue.size() > BLOCKS_PER_CHUNK)
		{
			LOG_WARNING("WorldGenerator: setSkyLight: light_source_queue size > BLOCKS_PER_CHUNK (" << BLOCKS_PER_CHUNK << ")");
			break;
		}

		const glm::ivec3 current_block_world_pos = light_source_queue.front();
		light_source_queue.pop();

		const glm::ivec3 chunk_pos = getChunkPos(current_block_world_pos);
		const glm::ivec3 block_chunk_pos = getBlockChunkPos(current_block_world_pos);
		const std::shared_ptr<Chunk> chunk = chunks.at(chunk_pos);

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
				const std::shared_ptr<Chunk> neighbor_chunk = chunks.at(neighbor_chunk_pos);
				const BlockInfo::Type neighbor_id = neighbor_chunk->getBlock(neighbor_block_chunk_pos);
				const int neighbor_light = neighbor_chunk->getSkyLight(neighbor_block_chunk_pos);

				if (!g_blocks_info.hasProperty(neighbor_id, BLOCK_PROPERTY_OPAQUE))
				{
					const int absorbed_light = g_blocks_info.get(neighbor_id).absorb_light;
					const int new_light = current_light - absorbed_light
						- (i != 3 || current_light != 15) * 1; // i == 3 is when the current light spreads down
					if (neighbor_light < new_light)
					{
						neighbor_chunk->setSkyLight(neighbor_block_chunk_pos, new_light);
						light_source_queue.push(neighbor_world_pos);
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

	std::queue<glm::ivec3> light_source_queue;
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
					light_source_queue.push(block_world_pos);
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

	while (!light_source_queue.empty())
	{
		const glm::ivec3 current_block_world_pos = light_source_queue.front();
		light_source_queue.pop();

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
						light_source_queue.push(neighbor_world_pos);
					}
				}
			}
		}
	}
}

void World::WorldGenerator::setupPass(genStruct & genData, const glm::ivec3 & chunk_pos, Chunk::genLevel gen_level)
{
	std::shared_ptr<Chunk> chunk = m_world.getChunkNoLock(chunk_pos);
	if (chunk == nullptr)
	{
		chunk = std::make_shared<Chunk>(chunk_pos);
		m_world.insertChunkNoLock(chunk_pos, chunk);
	}

	if (chunk->getGenLevel() <= gen_level || genData.generated_chunk.contains({chunk_pos, gen_level}))
		return;
	genData.generated_chunk.insert({chunk_pos, gen_level});

	switch(gen_level)
	{
	case LIGHT:
	{
		if (chunk->getGenLevel() <= LIGHT)
			return;
		//to do lights we need to garantee that the current chunk has full formed relief chunks around it
		for(int x = -1; x <= 1; x++)
		{
			for(int z = -1; z <= 1; z++)
			{
				glm::ivec3 current_pos = chunk_pos + glm::ivec3(x, 0, z);
				setupPass(genData, current_pos, DECORATE);
			}
		}

		genData.light_graph->emplace([this, chunk_pos]{
			lightPass(chunk_pos);
		});
		break;
	}
	case DECORATE:
	{
		if (chunk->getGenLevel() <= DECORATE)
			return;
		
		//to do decorations we need to garantee that the current chunk has full formed relief/cave chunks around it		
		for(int x = -1; x <= 1; x++)
		{
			for(int z = -1; z <= 1; z++)
			{
				glm::ivec3 current_pos = chunk_pos + glm::ivec3(x, 0, z);
				setupPass(genData, current_pos, CAVE);
			}
		}
		
		genData.decorate_graph->emplace([this, chunk_pos]{
			decoratePass(chunk_pos);
		});
		break;
	}
	case CAVE:
	{
		if (chunk->getGenLevel() <= CAVE)
			return;
		genData.relief_graph->emplace([this, chunk_pos]{
			// reliefPass(chunk_pos);
			newReliefPass(chunk_pos);
		});
		break;
	}
	default:
		break;
	}
}

std::shared_ptr<task::TaskGraph> World::WorldGenerator::getGenerationGraph(ChunkGenList & chunks_to_gen)
{
	genStruct genData;

	for (auto position : chunks_to_gen)
		setupPass(genData, position, LIGHT);
	return genData.graph;
}

void World::WorldGenerator::lightPass(const glm::ivec3 & chunkPos3D)
{
	ChunkMap chunkGrid;
	std::shared_ptr<Chunk> current_chunk = m_world.getChunkNoLock(chunkPos3D);
	{
		std::lock_guard lock(current_chunk->status);
		current_chunk->setGenLevel(LIGHT);
	}
	for (int x = -1; x <= 1; x++)
	{
		for (int z = -1; z <= 1; z++)
		{
			glm::ivec3 chunk_pos = chunkPos3D + glm::ivec3(x, 0, z);
			std::shared_ptr<Chunk> chunk = m_world.getChunkNoLock(chunk_pos);
			if (chunk == nullptr)
				continue;
			std::lock_guard lock(chunk->status);
			if (chunk->getGenLevel() != DECORATE && chunk->getGenLevel() != LIGHT)
				throw std::runtime_error("chunk not generated :" + std::to_string(chunk_pos.x) + " " + std::to_string(chunk_pos.z));
			chunkGrid.insert({chunk_pos, chunk});
		}
	}

	std::scoped_lock lock(
		chunkGrid.at(chunkPos3D)->status,
		chunkGrid.at(chunkPos3D + glm::ivec3{0, 0, -1})->status,
		chunkGrid.at(chunkPos3D + glm::ivec3{0, 0, 1})->status,
		chunkGrid.at(chunkPos3D + glm::ivec3{1, 0, -1})->status,
		chunkGrid.at(chunkPos3D + glm::ivec3{1, 0, 0})->status,
		chunkGrid.at(chunkPos3D + glm::ivec3{1, 0, 1})->status,
		chunkGrid.at(chunkPos3D + glm::ivec3{-1, 0, -1})->status,
		chunkGrid.at(chunkPos3D + glm::ivec3{-1, 0, 0})->status,
		chunkGrid.at(chunkPos3D + glm::ivec3{-1, 0, 1})->status
	);

	setSkyLight(chunkGrid, chunkPos3D);
	setBlockLight(chunkGrid, chunkPos3D);
}

void World::WorldGenerator::reliefPass(const glm::ivec3 & chunkPos3D)
{
	std::shared_ptr<Chunk> chunk = m_world.getChunkNoLock(chunkPos3D);
	std::lock_guard(chunk->status);
	chunk->setGenLevel(CAVE);
	for(int blockX = 0; blockX < CHUNK_X_SIZE; blockX++)
	{
		for(int blockZ = 0; blockZ < CHUNK_Z_SIZE; blockZ++)
		{
			//generate or find current biome values
			float continentalness = 10000.0f;
			bool isLand = false;
			bool isOcean = false;
			bool isCoast = false;
			Chunk::biomeInfo biome;
			continentalness = m_continentalness_perlin.noise(glm::vec2(
				(blockX + chunkPos3D.x * CHUNK_X_SIZE) * 0.00090f,
				(blockZ + chunkPos3D.z * CHUNK_Z_SIZE) * 0.00090f
			));
			isLand = continentalness > 0.2f;
			isOcean = continentalness < 0.00f;
			isCoast = !isLand && !isOcean;
			biome.continentalness = continentalness;
			biome.isLand = isLand;
			biome.isOcean = isOcean;
			biome.isCoast = isCoast;
			chunk->setBiome(blockX, blockZ, biome);
			// if (blockZ % 2 == 0 && blockX % 2 == 0)
			// {
			// 	if (blockX + 1 < CHUNK_X_SIZE)
			// 		chunk->setBiome(blockX + 1, blockZ, biome);
			// 	if (blockZ + 1 < CHUNK_Z_SIZE)
			// 		chunk->setBiome(blockX, blockZ + 1, biome);
			// 	if (blockX + 1 < CHUNK_X_SIZE && blockZ + 1 < CHUNK_Z_SIZE)
			// 		chunk->setBiome(blockX + 1, blockZ + 1, biome);
			// }
			// else
			// {
			// 	biome = chunk->getBiome(blockX, blockZ);
			// 	continentalness = biome.continentalness;
			// 	isLand = biome.isLand;
			// 	isOcean = biome.isOcean;
			// 	isCoast = biome.isCoast;
			// }
			// check the continentalness of the chunk

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

			if (isOcean)
			{
				reliefValue = oceanReliefValue;
			}
			else if (isLand)
			{
				// reliefValue *= (CHUNK_Y_SIZE - 100); // map from [0, 1] to [0, CHUNK_Y_SIZE - 100]
				// reliefValue += 100; // map from [0, CHUNK_Y_SIZE - 100] to [100, CHUNK_Y_SIZE]
				reliefValue = landReliefValue;
			}
			else if (isCoast)
			{
				// reliefValue *= (CHUNK_Y_SIZE - 100); // map from [0, 1] to [0, CHUNK_Y_SIZE - 100]
				// reliefValue += 100; // map from [0, CHUNK_Y_SIZE - 100] to [100, CHUNK_Y_SIZE]
				reliefValue = std::lerp(oceanReliefValue, landReliefValue, mapRange(continentalness, 0.0f, 0.2f, 0.0f, 1.0f));
			}
			int highestBlock = 0;
			for(int blockY = 0; blockY < CHUNK_Y_SIZE; blockY++)
			{

				glm::ivec3 position = glm::ivec3(
					blockX + chunkPos3D.x * CHUNK_X_SIZE,
					blockY,
					blockZ + chunkPos3D.z * CHUNK_Z_SIZE
				);
				BlockInfo::Type to_set = BlockInfo::Type::Air;

				if (isOcean)
				{
					if (position.y < reliefValue)
						to_set = BlockInfo::Type::Stone;
					else if (position.y < 80)
						to_set = BlockInfo::Type::Water;
					else
						to_set = BlockInfo::Type::Air;
				}
				else if (isCoast)
				{
					if (position.y > 80)
					{
						if (position.y < reliefValue - 5)
							to_set = BlockInfo::Type::Stone;
						else if (position.y < reliefValue - 1 )
							to_set = BlockInfo::Type::Dirt;
						else if (position.y < reliefValue)
							to_set = BlockInfo::Type::Grass;
						else
							to_set = BlockInfo::Type::Air;
					}
					else
					{
						if (position.y < reliefValue)
							to_set = BlockInfo::Type::Stone;
						else
							to_set = BlockInfo::Type::Water;
					}
				}
				else if (isLand)
				{
					(void)riverValue;
					//check to see wether above or below the relief value
					// if (reliefValue > position.y)
						// to_set = BlockInfo::Type::Stone;
					// else if (reliefValue + 5 > position.y)
					// {
					// 	if (riverValue < 0.05f)
					// 		to_set = BlockInfo::Type::Water;
					// 	else
					// 		to_set = BlockInfo::Type::Grass;
					// }
					// else
						// to_set = BlockInfo::Type::Air;
					if (position.y < reliefValue - 5)
						to_set = BlockInfo::Type::Stone;
					else if (position.y < reliefValue - 1)
						to_set = BlockInfo::Type::Dirt;
					else if (position.y < reliefValue)
						to_set = BlockInfo::Type::Grass;
					else
						to_set = BlockInfo::Type::Air;
				}

				if (to_set != BlockInfo::Type::Air
					&& to_set != BlockInfo::Type::Water
					&& generateCaveBlock(position) == BlockInfo::Type::Air)
					to_set = BlockInfo::Type::Air;
				if (to_set != BlockInfo::Type::Air && blockY > highestBlock)
					highestBlock = blockY;
				chunk->setBlock(blockX, blockY, blockZ, to_set);
			}
			chunk->setHeight(blockX, blockZ, highestBlock);
		}
	}
}

void World::WorldGenerator::newReliefPass(const glm::ivec3 & chunkPos3D)
{
	std::shared_ptr<Chunk> chunk = m_world.getChunkNoLock(chunkPos3D);
	std::lock_guard(chunk->status);
	chunk->setGenLevel(CAVE);
	for(int blockX = 0; blockX < CHUNK_X_SIZE; blockX++)
	{
		for(int blockZ = 0; blockZ < CHUNK_Z_SIZE; blockZ++)
		{
			glm::vec2 current_world_pos = glm::vec2(
				blockX + chunkPos3D.x * CHUNK_X_SIZE,
				blockZ + chunkPos3D.z * CHUNK_Z_SIZE
			);
			//generate values
			float continentalness = m_continentalness_perlin.noise(current_world_pos);
			float erosion = m_erosion_perlin.noise(current_world_pos);
			float weirdness = m_weirdness_perlin.noise(current_world_pos);
			float temperature = m_temperature_perlin.noise(current_world_pos);
			float humidity = m_humidity_perlin.noise(current_world_pos);
			float PV = calculatePeaksAndValleys(weirdness);
			float relief = generateReliefValue(glm::ivec2(current_world_pos));
			// relief = (relief + 1) / 2; // map from [-1, 1] to [0, 1]
			// relief = pow(2, 10 * relief - 10); // map from [0, 1] to [0, 1] with a slope
			
			int level_continent = continentalnessLevel(continentalness);
			int level_erosion = erosionLevel(erosion);
			int level_temperature = temperatureLevel(temperature);
			int level_humidity = humidityLevel(humidity);
			int level_weirdness = weirdnessLevel(weirdness);
			int level_PV = PVLevel(PV);

			BiomeType biome = getBiomeType(level_continent, level_erosion, level_humidity, level_temperature, level_weirdness, level_PV);

			//we want continentalness to determine base height
			//the more erosion the flatter the terrain
			// positive PV means positive height bias
			// negative PV means negative height bias
			//and erosion as well as PV to determine the height bias
			float heightBias = calculateHeightBias(erosion, PV);
			int baseHeight = calculateBaseHeight(relief, continentalness, PV, erosion, biome);

			std::array<BlockType, CHUNK_Y_SIZE> blocks = getBlockColumn(baseHeight, biome);
			chunk->setBlockColumn(blockX, blockZ, blocks);
			Chunk::biomeInfo biome_info;
			biome_info.biome = biome;
			biome_info.continentalness = continentalness;
			biome_info.erosion = erosion;
			biome_info.weirdness = weirdness;
			biome_info.temperature = temperature;
			biome_info.humidity = humidity;
			biome_info.PV = PV;
			chunk->setBiome(blockX, blockZ, biome_info);
			// Chunk::biomeInfo biome_info{
			// 	false,
			// 	false,
			// 	false,
			// 	continentalness,
			// 	erosion,
			// 	weirdness,
			// 	temperature,
			// 	humidity,
			// 	biome
			// };
			// chunk->setBiome(blockX, blockZ, biome_info);
		}
	}
}

void World::WorldGenerator::decoratePass(const glm::ivec3 & chunkPos3D)
{
	ChunkMap chunkGrid;
	std::shared_ptr<Chunk> center_chunk = m_world.getChunkNoLock(chunkPos3D);
	{
		std::lock_guard lock(center_chunk->status);
		center_chunk->setGenLevel(DECORATE);
	}
	for (int x = -1; x <= 1; x++)
	{
		for (int z = -1; z <= 1; z++)
		{
			glm::ivec3 chunk_pos = chunkPos3D + glm::ivec3(x, 0, z);
			std::shared_ptr<Chunk> chunk = m_world.getChunkNoLock(chunk_pos);
			if (chunk == nullptr)
				continue;
			std::lock_guard lock(chunk->status);
			if (chunk->getGenLevel() != CAVE && chunk->getGenLevel() != DECORATE)
				throw std::runtime_error("chunk not generated :" + std::to_string(chunk_pos.x) + " " + std::to_string(chunk_pos.z));
			chunkGrid.insert({chunk_pos, chunk});
		}
	}

	std::scoped_lock lock(
		chunkGrid.at(chunkPos3D)->status,
		chunkGrid.at(chunkPos3D + glm::ivec3{0, 0, -1})->status,
		chunkGrid.at(chunkPos3D + glm::ivec3{0, 0, 1})->status,
		chunkGrid.at(chunkPos3D + glm::ivec3{1, 0, -1})->status,
		chunkGrid.at(chunkPos3D + glm::ivec3{1, 0, 0})->status,
		chunkGrid.at(chunkPos3D + glm::ivec3{1, 0, 1})->status,
		chunkGrid.at(chunkPos3D + glm::ivec3{-1, 0, -1})->status,
		chunkGrid.at(chunkPos3D + glm::ivec3{-1, 0, 0})->status,
		chunkGrid.at(chunkPos3D + glm::ivec3{-1, 0, 1})->status
	);

	std::unordered_set<glm::ivec2> tree_positions;
	//iterate over every possible tree placement and add it to a list
	const Chunk::BiomeArray & biomes = center_chunk->getBiomes();
	for(size_t i = 0; i < biomes.size(); i++)
	{
		const Chunk::biomeInfo & biome = biomes[i];
		if (biome.isLand)
		{
			glm::ivec2 chunk_pos = Chunk::toBiomeCoord(i);
			tree_positions.insert(chunk_pos);
		}
	}

	std::srand(this->seed);

	const StructureInfo & tree_info = g_structures_info.get(StructureInfo::Type::Tree);
	while (!tree_positions.empty())
	{
		glm::ivec2 tree_pos = *tree_positions.begin();
		tree_positions.erase(tree_pos);

		//test if the tree can be placed
		if (std::rand() % 15 != 0)
			continue;


		glm::ivec3 tree_start = glm::ivec3(
			tree_pos.x,
			center_chunk->getHeight(tree_pos),
			tree_pos.y
		);

		tree_start += chunkPos3D * CHUNK_SIZE_IVEC3;

		// if ((std::hash<glm::ivec2>{}(tree_start)) % 10 != 0)
		// 	continue;


		//erase neighbor positions from the list
		for(int x = 0; x < tree_info.size.x; x++)
		{
			for(int z = 0; z < tree_info.size.z; z++)
				tree_positions.erase(tree_pos + glm::ivec2(x - 1, z - 1));
		}
		//find highest block that isnt air
		
		//convert tree start to world pos


		placeStructure(chunkGrid, tree_start, tree_info);
	}

}

void World::WorldGenerator::placeStructure(ChunkMap & chunkGrid, const glm::ivec3 & start_pos, const StructureInfo & structure)
{
	for(int x = 0; x < structure.size.x; x++)
	{
		for(int z = 0; z < structure.size.z; z++)
		{
			for(int y = 0; y < structure.size.y; y++)
			{
				glm::ivec3 current_world_position = start_pos + glm::ivec3(x, y, z);
				if (current_world_position.y >= CHUNK_Y_SIZE)
					break;

				glm::ivec3 current_chunk_pos = getChunkPos(current_world_position);
				current_chunk_pos.y = 0;
				std::shared_ptr<Chunk> current_chunk = nullptr;
				auto it = chunkGrid.find(current_chunk_pos);
				if (it == chunkGrid.end())
					continue;
				current_chunk = it->second;

				glm::ivec3 current_chunk_position = getBlockChunkPos(current_world_position);
				if (current_chunk->getBlock(current_chunk_position) == BlockInfo::Type::Air)
					current_chunk->setBlock(current_chunk_position, structure.getBlock({x, y ,z}));
			}
		}
	}
}



/*****************************\
 * NOISE MANIPULATION
\*****************************/

float World::WorldGenerator::calculatePeaksAndValleys(const float & weirdness)
{
	//1−|(3|x|)−2|
	return (1 - std::abs((3 * std::abs(weirdness)) - 2));
}

//return a value between -1 1
float World::WorldGenerator::calculateHeightBias(const float & erosion, const float & PV)
{
	float ret = PV;

	float erosionBias = mapRange(erosion, -1.0f, 1.0f, 1.0f, 0.0f);

	//higher the erosion the flatter the terrain
	ret *= erosionBias;

	return ret;
}

float World::WorldGenerator::calculateBaseHeight(const float & relief, const float & continentalness, const float & pv, const float & erosion, BiomeType biome) 
{
	float temp_relief = relief;
	float ret = 0.0f;
	temp_relief = (temp_relief + 1) / 2; // map from [-1, 1] to [0, 1]
	float oceanRelief = mapRange(temp_relief, 0.0f, 1.0f, 40.0f, 60.0f);
	float riverRelief = mapRange(temp_relief, 0.0f, 1.0f, 70.0f, 80.0f);
	temp_relief = pow(2, 10 * temp_relief - 10); // map from [0, 1] to [0, 1] with a slope
	float landRelief = mapRange(temp_relief, 0.0f, 1.0f, 80.0f, 300.0f);
	// float mountainRelief = mapRange(temp_relief, 0.0f, 1.0f, 100.0f, 150.0f);

	//lerp between ocean and land with heavy favoring of ocean

	//create smooth transition between mountains and land and rivers
	if (pv < -0.8f)
		landRelief = std::lerp(riverRelief, landRelief, mapRange(pv, -1.0f, -0.8f, 0.0f, 1.0f));
	else if (pv < -0.6f)
		landRelief = landRelief;
	else if (pv < 0.2f)
		landRelief = landRelief;
	// else if (pv <= 0.7f)
		// landRelief = std::lerp(landRelief, mountainRelief, mapRange(pv, 0.2f, 0.7f, 0.0f, 1.0f));
	else
		landRelief = landRelief;
		// landRelief = mountainRelief;
		// landRelief = std::lerp(landRelief, mountainRelief, mapRange(pv, 0.7f, 1.0f, 0.0f, 1.0f));
	// else
		// landRelief = std::lerp(landRelief, riverRelief, mapRange(pv, -0.8f, 0.2f, 0.0f, 1.0f));

	//lerp ocean and land
	if( continentalness > -0.1f)
		ret = landRelief;
	else if (continentalness < -0.2f)
		ret = oceanRelief;
	else
		ret = std::lerp(oceanRelief, landRelief, mapRange(continentalness, -0.2f, -0.1f, 0.0f, 1.0f));

	return ret;
}

int World::WorldGenerator::temperatureLevel(const float & temperature)
{
	if (temperature < -0.45f) return 0;
	if (temperature < -0.15f) return 1;
	if (temperature < 0.2f) return 2;
	if (temperature < 0.5f) return 3;
	return 4;
}

int World::WorldGenerator::humidityLevel(const float & humidity)
{
	if (humidity < -0.45f) return 0;
	if (humidity < -0.15f) return 1;
	if (humidity < 0.2f) return 2;
	if (humidity < 0.5f) return 3;
	return 4;
}

int World::WorldGenerator::weirdnessLevel(const float & weirdness)
{
	if( weirdness > 0) return 1;
	return 0;
}

int World::WorldGenerator::erosionLevel(const float & erosion)
{
	if (erosion < -0.80) return 0;
	if (erosion < -0.35) return 1;
	if (erosion < -0.20) return 2;
	if (erosion < 0.05) return 3;
	if (erosion < 0.45) return 4;
	if (erosion < 0.20) return 5;
	return 6;
}

int World::WorldGenerator::continentalnessLevel(const float & continentalness)
{
	if (continentalness < -0.2f) return 0;
	if (continentalness < -0.1f) return 1;
	return 2;
}

int World::WorldGenerator::PVLevel(const float & PV)
{
	if (PV < -0.8f) return 0;
	if (PV < -0.6f) return 1;
	if (PV < 0.2f) return 2;
	if (PV < 0.7f) return 3;
	return 4;
}

BiomeType World::WorldGenerator::getBiomeType(
	const int & continentalness,
	const int & erosion,
	const int & humidity,
	const int & temperature,
	const int & weirdness,
	const int & PV)
{
	if (continentalness == 0)
		return BiomeType::OCEAN;
	else {
		if (PV == 0)
			return BiomeType::RIVER;
		if (PV == 4 && erosion < 3)
			return BiomeType::MOUNTAIN;
		if (continentalness == 1)
		{
			return BiomeType::COAST;
		} else //inland biomes
		{
			if (temperature >= 3)
				return BiomeType::DESERT;
			if (humidity > 1)
				return BiomeType::FOREST;
		}
	}
	return BiomeType::PLAIN;
}

std::array<BlockType, CHUNK_Y_SIZE> World::WorldGenerator::getBlockColumn(int baseHeight, BiomeType biome)
{
	std::array<BlockType, CHUNK_Y_SIZE> blocks;
	// switch(biome)
	// {
	// case BiomeType::OCEAN:
	// {
	// 	for(int i = 0; i < CHUNK_Y_SIZE; i++)
	// 	{
	// 		if (i < baseHeight)
	// 			blocks[i] = BlockType::Stone;
	// 		else if (i < 80)
	// 			blocks[i] = BlockType::Water;
	// 		else
	// 			blocks[i] = BlockType::Air;
	// 	}
	// 	break;
	// }
	// case BiomeType::RIVER:
	// {
	// 	for(int i = 0; i < CHUNK_Y_SIZE; i++)
	// 	{
	// 		if (i < baseHeight)
	// 			blocks[i] = BlockType::Stone;
	// 		else if (i < 80)
	// 			blocks[i] = BlockType::Water;
	// 		else
	// 			blocks[i] = BlockType::Air;
	// 	}
	// 	break;
	// }
	// case BiomeType::MOUNTAIN:
	// {
	// 	for(int i = 0; i < CHUNK_Y_SIZE; i++)
	// 	{
	// 		if (i < baseHeight)
	// 			blocks[i] = BlockType::Stone;
	// 		else
	// 			blocks[i] = BlockType::Air;
	// 	}
	// 	break;
	// }
	// case BiomeType::COAST:
	// {
	// 	for(int i = 0; i < CHUNK_Y_SIZE; i++)
	// 	{
	// 		if (i < baseHeight - 5)
	// 			blocks[i] = BlockType::Stone;
	// 		else if (i < baseHeight)
	// 			blocks[i] = BlockType::Sand;
	// 		else if (i < 80 - 5)
	// 			blocks[i] = BlockType::Water;
	// 		else
	// 			blocks[i] = BlockType::Air;
	// 	}
	// 	break;
	// }
	// case BiomeType::DESERT:
	// {
	// 	for(int i = 0; i < CHUNK_Y_SIZE; i++)
	// 	{
	// 		if (i < baseHeight - 5)
	// 			blocks[i] = BlockType::Stone;
	// 		else if (i < baseHeight)
	// 			blocks[i] = BlockType::Sand;
	// 		else
	// 			blocks[i] = BlockType::Air;
	// 	}
	// 	break;
	// }
	// case BiomeType::PLAIN:
	// 	[[fallthrough]];
	// case BiomeType::FOREST:
	// {
	// 	for(int i = 0; i < CHUNK_Y_SIZE; i++)
	// 	{
	// 		if (i < baseHeight - 5)
	// 			blocks[i] = BlockType::Stone;
	// 		else if (i < baseHeight - 1)
	// 			blocks[i] = BlockType::Dirt;
	// 		else if (i < baseHeight)
	// 			blocks[i] = BlockType::Grass;
	// 		else
	// 			blocks[i] = BlockType::Air;
	// 	}
	// 	break;
	// }
	// case BiomeType::NONE:
	// {
	// 	for(int i = 0; i < CHUNK_Y_SIZE; i++)
	// 		blocks[i] = BlockType::Air;
	// 	break;
	// }
	// }
	for(int i = 0; i < CHUNK_Y_SIZE; i++)
	{
		if (i < baseHeight)
			blocks[i] = BlockType::Stone;
		else if (i < 80)
			blocks[i] = BlockType::Water;
		else
			blocks[i] = BlockType::Air;
	}
	return blocks;
}

void World::WorldGenerator::drawNoises(VulkanAPI & vk)
{
	for (int x = 0; x < DebugGui::NOISE_SIZE; x++)
	{
		for (int z = 0; z < DebugGui::NOISE_SIZE; z++)
		{
			glm::vec2 current_world_pos = glm::vec2(x,z) * 5.0f;
			//generate values
			float continentalness = m_continentalness_perlin.noise(current_world_pos);
			float erosion = m_erosion_perlin.noise(current_world_pos);
			float weirdness = m_weirdness_perlin.noise(current_world_pos);
			float temperature = m_temperature_perlin.noise(current_world_pos);
			float humidity = m_humidity_perlin.noise(current_world_pos);
			float PV = calculatePeaksAndValleys(weirdness);
			
			int level_continent = continentalnessLevel(continentalness);
			int level_erosion = erosionLevel(erosion);
			int level_temperature = temperatureLevel(temperature);
			int level_humidity = humidityLevel(humidity);
			int level_weirdness = weirdnessLevel(weirdness);
			int level_PV = PVLevel(PV);

			BiomeType biome = getBiomeType(level_continent, level_erosion, level_humidity, level_temperature, level_weirdness, level_PV);

			uint8_t rgb_continent = mapRange(continentalness, -1.0f, 1.0f, 0.0f, 255.0f);
			uint8_t rgb_erosion = mapRange(erosion, -1.0f, 1.0f, 0.0f, 255.0f);
			uint8_t rgb_temperature = mapRange(temperature, -1.0f, 1.0f, 0.0f, 255.0f);
			uint8_t rgb_humidity = mapRange(humidity, -1.0f, 1.0f, 0.0f, 255.0f);
			uint8_t rgb_weirdness = mapRange(weirdness, -1.0f, 1.0f, 0.0f, 255.0f);
			uint8_t rgb_PV = mapRange(PV, -1.0f, 1.0f, 0.0f, 255.0f);

			vk.ImGuiTexturePutPixel(DebugGui::continentalness_texture_id, x, z, rgb_continent, rgb_continent, rgb_continent);
			vk.ImGuiTexturePutPixel(DebugGui::erosion_texture_id, x, z, rgb_erosion, rgb_erosion, rgb_erosion);
			vk.ImGuiTexturePutPixel(DebugGui::temperature_texture_id, x, z, rgb_temperature, rgb_temperature, rgb_temperature);
			vk.ImGuiTexturePutPixel(DebugGui::humidity_texture_id, x, z, rgb_humidity, rgb_humidity, rgb_humidity);
			vk.ImGuiTexturePutPixel(DebugGui::weirdness_texture_id, x, z, rgb_weirdness, rgb_weirdness, rgb_weirdness);
			vk.ImGuiTexturePutPixel(DebugGui::PV_texture_id, x, z, rgb_PV, rgb_PV, rgb_PV);

			glm::ivec3 biome_rgb;

			switch (biome)
			{
			case BiomeType::OCEAN:
				biome_rgb = {0, 0, 200};
				break;
			case BiomeType::RIVER:
				biome_rgb = {0, 255, 255};
				break;
			case BiomeType::MOUNTAIN:
				biome_rgb = {200, 200, 200};
				break;
			case BiomeType::COAST:
				biome_rgb = {255, 255, 0};
				break;
			case BiomeType::DESERT:
				biome_rgb = {200, 200, 0};
				break;
			case BiomeType::FOREST:
				biome_rgb = {0, 180, 0};
				break;
			case BiomeType::PLAIN:
				biome_rgb = {0, 255, 0};
				break;
			case BiomeType::NONE:
				biome_rgb = {0, 0, 0};
				break;
			};
			vk.ImGuiTexturePutPixel(DebugGui::biome_texture_id, x, z, biome_rgb.r, biome_rgb.g, biome_rgb.b);
		}
	}
}
