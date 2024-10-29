#pragma once

#include "List.hpp"
#include "Player.hpp"
#include "WorldGenerator.hpp"
#include "logger.hpp"
#include "ThreadPoolAccessor.hpp"
#include "tasks.hpp"
#include "Mob.hpp"
#include "hashes.hpp"
#include "Tracy.hpp"
#include "Structures.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>


#include <unordered_map>
#include <shared_mutex>
#include <unordered_set>

class World
{
public:
	class WorldGenerator
	{
	using enum Chunk::genLevel;
	public:
		typedef std::unordered_set<glm::ivec3> ChunkGenList;
		// struct genInfo
		// {
		// 	struct zone
		// 	{
		// 		Chunk::genLevel level;
		// 		Chunk::genLevel oldLevel;
		// 		glm::ivec3 size;
		// 		glm::ivec3 start;
		// 	};
		// 	std::vector<zone> zones;
		// };

		/**
		 * @warning the zone sizes must be in the same order as the genLevel enum
		 * @warning the zone sizes must be multiples of the zone sizes of the previous gen level (ex 5x5 and 10x10)
		 */
		// constexpr static std::array<glm::ivec3, 3> ZONE_SIZES = {
		// 	glm::ivec3(1, 0, 1),
		// 	glm::ivec3(1, 0, 1),
		// 	glm::ivec3(1, 0, 1)
		// };
		// constexpr static int MAX_TICKET_LEVEL = TICKET_LEVEL_INACTIVE + 2;

		//gen level 0 if full chunkGeneration


		WorldGenerator(World & world);

		~WorldGenerator();

		/**
		 * @brief get the generation info for the given gen level and chunk position
		 *
		 * @param gen_level
		 * @param old_level
		 * @param chunkPos3D
		 * @return a genInfo struct
		 */
		// static genInfo getGenInfo(Chunk::genLevel gen_level, Chunk::genLevel old_level, glm::ivec3 chunkPos3D);

		// static Chunk::genLevel ticketToGenLevel(int ticket_level);

		/**
		 * @brief will do a generation pass to the chunks
		 *
		 * @param info the struct returned by the getGenInfo function
		 * @param chunkGenGrid the chunks to generate
		 *
		 * @warning the chunks MUST be locked before calling and unlocked after
		 */
		// void 					generate(genInfo::zone info, ChunkMap & chunkGenGrid);

		/**
		 * @brief creates a task graph that when runned will generate every chunk in the list
		 * 
		 * @param chunks_to_gen 
		 * @return task::TaskGraph&& 
		 */
		std::shared_ptr<task::TaskGraph>	getGenerationGraph(ChunkGenList & chunks_to_gen);


		// double	m_avg = 0;
		// int		m_called = 0;
		// float	m_max = -1;
		// float 	m_min = 1;
	private:
		uint64_t seed = 0;
		struct genStruct
		{
			genStruct()
			:
			graph(task::TaskGraph::create()),
			light_graph(task::TaskGraph::create()),
			relief_graph(task::TaskGraph::create()),
			decorate_graph(task::TaskGraph::create())
			{
				task::Task light_task = graph->emplace(light_graph).Name("Light passes");
				task::Task relief_task = graph->emplace(relief_graph).Name("Relief passes");
				task::Task decorate_task = graph->emplace(decorate_graph).Name("Decoration passes");
				relief_task.precede(decorate_task);
				decorate_task.precede(light_task);
				light_graph->emplace([]{}); // dummy task
				relief_graph->emplace([]{}); // dummy task
				decorate_graph->emplace([]{}); // dummy task
				graph->emplace([]{});
			}
			std::shared_ptr<task::TaskGraph> graph;
			std::shared_ptr<task::TaskGraph> light_graph;
			std::shared_ptr<task::TaskGraph> relief_graph;
			std::shared_ptr<task::TaskGraph> decorate_graph;
			std::unordered_set<std::pair<glm::ivec3, Chunk::genLevel>> generated_chunk;
		};
		/*
		* PERLINS
		*/
		Perlin	m_relief_perlin;
		Perlin	m_cave_perlin;

		
		// BIOMES PERLINS
		Perlin	m_continentalness_perlin;
		Perlin  m_erosion_perlin;
		Perlin  m_weirdness_perlin;
		Perlin  m_temperature_perlin;
		Perlin  m_humidity_perlin;

		World & m_world;



		/*********************\
		* NOISE MANIPULATION *
		\*********************/
		float calculatePeaksAndValleys(const float & weirdness);
		float calculateHeightBias(const float & erosion, const float & PV);
		float calculateBaseHeight(const float & continentalness, const float & erosion, const float & PV);
		int   temperatureLevel(const float & temperature);
		int   humidityLevel(const float & humidity);

		BiomeType 							getBiomeType(
			const float & continentalness,
			const float & erosion,
			const float & humidity,
			const float & temperature,
			const float & weirdness,
			const float & PV);
		std::array<BlockType, CHUNK_Y_SIZE> getBlockColumn(int baseHeight, BiomeType biome);





		/****************************************\
		*
		* GENERATION PASSES
		*
		\****************************************/
		void	setupPass(genStruct & genData,const glm::ivec3 & chunk_pos, Chunk::genLevel gen_level);
		void	addPassToGraph(genStruct & genData, glm::ivec3 chunk_pos, Chunk::genLevel gen_level);
		
		void	lightPass(const glm::ivec3 & pos);

		void	reliefPass(const glm::ivec3 & pos);

		void	newReliefPass(const glm::ivec3 & pos);
		/********************\
		* RELIEF PASS UTILS
		\********************/
		BlockInfo::Type generateCaveBlock(glm::ivec3 position);
		float	generateReliefValue(glm::ivec2 position);

		void 	decoratePass(const glm::ivec3 & pos);
		/********************\
		* DECORATE PASS UTILS
		\********************/
		void	placeStructure(ChunkMap & chunkGrid, const glm::ivec3 & start_pos, const StructureInfo & structure);
		/*
		* BLOCK PLACING UTILS
		*/
	};

	World();
	~World();

	World(World & other) = delete;
	World(World && other) = delete;
	World & operator=(World & other) = delete;
	World & operator=(World && other) = delete;

	/**
	 * @brief will return the block position relative to the chunk
	 *
	 * @param position the position of the block in the world
	 * @return glm::vec3
	 */
	static glm::vec3 	getBlockChunkPosition(const glm::vec3 & position);

	/**
	 * @brief will return the chunk position containing the block
	 *
	 * @param position the position of the block in the world
	 * @return glm::vec3
	 */
	static glm::vec3 	getChunkPosition(const glm::vec3 & position);

	/**
	 * @brief Get the Chunk object
	 *
	 * @param position
	 * @retval std::shared_ptr<Chunk> if the chunk exists
	 * @retval nullptr if the chunk does not exist
	 */
	std::shared_ptr<Chunk>	getChunk(const glm::ivec3 & position) const;
	void 					insertChunk(const glm::ivec3 & position, std::shared_ptr<Chunk> chunk);

protected:

	std::unordered_map<uint64_t, std::shared_ptr<Player>>	m_players;
	TracyLockableN											(std::mutex, m_players_mutex, "Players");

	std::unordered_map<uint64_t, std::shared_ptr<Mob>>		m_mobs;
	TracyLockableN											(std::mutex, m_mobs_mutex, "Mobs");
	uint64_t												m_mob_id = 0;

	std::unordered_map<glm::ivec3, std::shared_ptr<Chunk>>	m_chunks;
	mutable TracyLockableN									(std::mutex,	m_chunks_mutex, "Chunks");

	ThreadPoolAccessor 									m_threadPool;


	WorldGenerator										m_world_generator;

	/**
	 * @brief Get a chunk ptr without locking the chunks mutex
	 *
	 * @warning you MUST lock the chunks mutex before calling this function
	 * @param position
	 * @retval std::shared_ptr<Chunk> if the chunk exists
	 * @retval nullptr if the chunk does not exist
	 */
	std::shared_ptr<Chunk>	getChunkNoLock(const glm::ivec3 & position) const;


	void					insertChunkNoLock(const glm::ivec3 & position, std::shared_ptr<Chunk> chunk);
	/*************************************
	 *  FUTURES
	 *************************************/
	// void	waitForFinishedFutures();
	// void	waitForFutures();

	/*************************************
	 *  LIGHTS
	 *************************************/

	/**
	 * @brief Update the sky light of the chunk containing the block
	 *
	 * @param block_position the position of the block in the world
	 * 
	 * @return a set of chunks that have been modified ( and might need remeshing )
	 */
	std::unordered_set<glm::ivec3> updateSkyLight(const glm::ivec3 & block_position);

	/**
	 * @brief Update the block light of the chunk containing the block
	 *
	 * @param block_position the position of the block in the world
	 * 
	 * @return a set of chunks that have been modified ( and might need remeshing )
	 */
	std::unordered_set<glm::ivec3> updateBlockLight(const glm::ivec3 & block_position);
};
