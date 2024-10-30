#include "Chunk.hpp"
#include "logger.hpp"
#include <cstring>

Chunk::Chunk(glm::ivec3 position)
: position(position), m_mesh_id(0)
{
	(void)position;
	// LOG_INFO("Chunk created at position: " << position.x << " " << position.y << " " << position.z);
	for(int i = 0; i < BLOCKS_PER_CHUNK; i++)
		m_blocks[i] = BlockInfo::Type::Air;

	memset(m_light.data(), 0, BLOCKS_PER_CHUNK);
	for(auto & biome : m_biomes)
		biome = biomeInfo();
}

// Chunk::Chunk(const glm::ivec3 & position, const BlockArray & blocks)
// : position(position), m_mesh_id(0), m_blocks(blocks)
// {
// 	memset(m_light.data(), 0, BLOCKS_PER_CHUNK);
// }

Chunk::Chunk(const glm::ivec3 & position,
	const BlockArray & blocks,
	const LightArray & light,
	const BiomeArray & biomes)
:	position(position),
	m_mesh_id(0),
	m_blocks(blocks),
	m_light(light),
	m_biomes(biomes)
{
}

// Chunk::Chunk(const Chunk & other)
// :	position(other.position),
// 	m_mesh_id(other.m_mesh_id),
// 	m_blocks(other.m_blocks),
// 	m_light(other.m_light)
// {
// }

// Chunk::Chunk(Chunk && other)
// :
// 	// status(other.status),
// 	position(other.position),
// 	m_mesh_id(other.m_mesh_id),
// 	m_blocks(std::move(other.m_blocks)),
// 	m_light(std::move(other.m_light))
// {
// }

// Chunk & Chunk::operator=(const Chunk & other)
// {
// 	position = other.position;
// 	m_mesh_id = other.m_mesh_id;
// 	m_blocks = other.m_blocks;
// 	m_light = other.m_light;
// 	// status = other.status;
// 	return *this;
// }

// Chunk & Chunk::operator=(const Chunk && other)
// {
// 	position = other.position;
// 	m_mesh_id = other.m_mesh_id;
// 	m_blocks = std::move(other.m_blocks);
// 	m_light = std::move(other.m_light);
// 	// status = other.status;
// 	return *this;
// }

Chunk::~Chunk()
{
	status.try_lock();
}

Chunk::BlockArray & Chunk::getBlocks()
{
	return m_blocks;
}

const Chunk::BlockArray & Chunk::getBlocks() const
{
	return m_blocks;
}

BlockInfo::Type Chunk::getBlock(const int & x, const int & y, const int & z) const
{
	int index = toIndex(x, y, z);

	return m_blocks[index];
}

BlockInfo::Type Chunk::getBlock(const glm::ivec3 & position) const
{
	return getBlock(position.x, position.y, position.z);
}

void Chunk::setBlock(const int & x, const int & y, const int & z, BlockInfo::Type block)
{
	int index = toIndex(x, y, z);

	m_blocks[index] = block;
	//REGENERATE MESH HERE
}

void Chunk::setBlock(const glm::ivec3 & position, BlockInfo::Type block)
{
	setBlock(position.x, position.y, position.z, block);
}

void Chunk::setBlockColumn(const int & x, const int & z, const std::array<BlockType, CHUNK_Y_SIZE> & column)
{
	for (int y = 0; y < CHUNK_Y_SIZE; y++)
		setBlock(x, y, z, column[y]);
}

void Chunk::setBlockColumn(const glm::ivec2 & pos, const std::array<BlockType, CHUNK_Y_SIZE> & column)
{
	setBlockColumn(pos.x, pos.y, column);
}


Chunk::LightArray & Chunk::getLight()
{
	return m_light;
}

const Chunk::LightArray & Chunk::getLight() const
{
	return m_light;
}

uint8_t Chunk::getLight(const int & x, const int & y, const int & z) const
{
	int index = toIndex(x, y, z);

	return m_light[index];
}

uint8_t Chunk::getLight(const glm::ivec3 & position) const
{
	return getLight(position.x, position.y, position.z);
}

void Chunk::setLight(const int & x, const int & y, const int & z, uint8_t light)
{
	int index = toIndex(x, y, z);

	m_light[index] = light;
}

void Chunk::setLight(const glm::ivec3 & position, uint8_t light)
{
	setLight(position.x, position.y, position.z, light);
}


uint8_t Chunk::getSkyLight(const int & x, const int & y, const int & z) const
{
	const int index = toIndex(x, y, z);
	return m_light[index] & 0b00001111;
}

uint8_t Chunk::getSkyLight(const glm::ivec3 & position) const
{
	return getSkyLight(position.x, position.y, position.z);
}

void Chunk::setSkyLight(const int & x, const int & y, const int & z, uint8_t sky_light)
{
	const int index = toIndex(x, y, z);
	m_light[index] = (m_light[index] & 0b11110000) | (sky_light & 0b00001111);
}

void Chunk::setSkyLight(const glm::ivec3 & position, uint8_t light)
{
	setSkyLight(position.x, position.y, position.z, light);
}


uint8_t Chunk::getBlockLight(const int & x, const int & y, const int & z) const
{
	const int index = toIndex(x, y, z);
	return (m_light[index] >> 4) & 0b00001111;
}

uint8_t Chunk::getBlockLight(const glm::ivec3 & position) const
{
	return getBlockLight(position.x, position.y, position.z);
}

void Chunk::setBlockLight(const int & x, const int & y, const int & z, uint8_t block_light)
{
	const int index = toIndex(x, y, z);
	m_light[index] = (m_light[index] & 0b00001111) | ((block_light & 0b00001111) << 4);
}

void Chunk::setBlockLight(const glm::ivec3 & position, uint8_t light)
{
	setBlockLight(position.x, position.y, position.z, light);
}

Chunk::BiomeArray & Chunk::getBiomes()
{
	return m_biomes;
}

const Chunk::BiomeArray & Chunk::getBiomes() const
{
	return m_biomes;
}

Chunk::biomeInfo Chunk::getBiome(const int & x, const int & z) const
{
	return m_biomes[toBiomeIndex(x, z)];
}

Chunk::biomeInfo Chunk::getBiome(const glm::ivec2 & position) const
{
	return getBiome(position.x, position.y);
}

void Chunk::setBiome(const int & x, const int & z, const biomeInfo & biome)
{
	m_biomes[toBiomeIndex(x, z)] = biome;
}

void Chunk::setBiome(const glm::ivec2 & position, const biomeInfo & biome)
{
	setBiome(position.x, position.y, biome);
}

glm::ivec2 Chunk::toBiomeCoord(int index)
{
	int x = index % (CHUNK_X_SIZE);
	int z = (index - x) / (CHUNK_Z_SIZE);

	return glm::ivec2(x, z);
}

int Chunk::toBiomeIndex(int x, int z)
{
	return x + z * (CHUNK_Z_SIZE);
}

int Chunk::toBiomeIndex(const glm::ivec2 & position)
{
	return toBiomeIndex(position.x, position.y);
}

Chunk::HeightArray & Chunk::getHeights()
{
	return m_heights;
}

const Chunk::HeightArray & Chunk::getHeights() const
{
	return m_heights;
}

uint8_t Chunk::getHeight(const int & x, const int & z) const
{
	return m_heights[toHeightIndex(x, z)];
}

uint8_t Chunk::getHeight(const glm::ivec2 & position) const
{
	return getHeight(position.x, position.y);
}

void Chunk::setHeight(const int & x, const int & z, uint8_t height)
{
	m_heights[toHeightIndex(x, z)] = height;
}

void Chunk::setHeight(const glm::ivec2 & position, uint8_t height)
{
	setHeight(position.x, position.y, height);
}

int Chunk::toHeightIndex(const int & x, const int & z)
{
	return x + z * CHUNK_X_SIZE;
}

int Chunk::toHeightIndex(const glm::ivec2 & position)
{
	return toHeightIndex(position.x, position.y);
}

const glm::ivec3 & Chunk::getPosition() const
{
	return position;
}

void Chunk::setPosition(const glm::ivec3 & position)
{
	this->position = position;
}

uint64_t Chunk::getMeshID() const
{
	return m_mesh_id;
}

void Chunk::setMeshID(const uint64_t & mesh_id)
{
	m_mesh_id = mesh_id;
}

bool Chunk::isMeshed() const
{
	return meshed;
}

void Chunk::setMeshed(bool meshed)
{
	this->meshed = meshed;
}

int Chunk::getLoadLevel() const
{
	return load_level;
}

int Chunk::getHighestLoadLevel() const
{
	return highest_load_level;
}
void Chunk::setLoadLevel(const int & load_level)
{
	this->load_level = load_level;
	if (this->load_level > highest_load_level)
		highest_load_level = this->load_level;
}

Chunk::genLevel Chunk::getGenLevel() const
{
	return m_gen_level;
}

void Chunk::setGenLevel(const genLevel & level)
{
	m_gen_level = level;
}

int Chunk::toIndex(const int & x, const int & y, const int & z)
{
	return x + y * CHUNK_X_SIZE + z * CHUNK_X_SIZE * CHUNK_Y_SIZE;
}

glm::ivec3	Chunk::toCoord(const int & index)
{
	int x = index % CHUNK_X_SIZE;
	int y = (index - x) % CHUNK_Y_SIZE;
	int z = (index - x - y) % CHUNK_Z_SIZE;
	return glm::ivec3(x, y, z);
}

glm::ivec3 getChunkPos(const glm::ivec3 & block_pos)
{
	// (block_pos / 16) works for positive numbers
	// but for negative numbers there is two problems:
	// 1) -1 / 16 = 0 and not -1: the first negative chunk has a coordinate of -1, but the result of the division is 0.
	//    It means that every negative chunk will give a result higher by 1 than it should be. So we need to subtract 1.
	//    We can use the following formula: (block_pos / 16) + (block_pos >> 31)
	//    If block_pos is negative, it's sign bit will be 1, so block_pos >> 31 will be 0xFFFFFFFF, and 0xFFFFFFFF = -1.
	//    If block_pos is positive, it's sign bit will be 0, so block_pos >> 31 will be 0x00000000, and 0x00000000 = 0.
	// 2) The block positions for the first negative chunk are in the range [-16, -1] and not [-15, 0].
	//    It means that whend you divide -16 by 16 you get -1 and not 0. So we need to add 1 to the initial block_pos.
	//    We can use the following formula: ((block_pos - (block_pos >> 31)) / 16) + (block_pos >> 31)
	//    See the explanation of the first problem to understand why we need to subtract (block_pos >> 31).

	constexpr int shift = sizeof(int) * 8 - 1;
	const glm::ivec3 shifted = block_pos >> shift;
	return ((block_pos - shifted) / CHUNK_SIZE_IVEC3) + shifted;
}

glm::ivec3 getBlockChunkPos(const glm::ivec3 & block_pos)
{
	// (block_pos % 16) works for positive numbers
	// but for negative numbers there is a problem:
	// The block positions for the first negative chunk are in the range [-16, -1] and not [0, 15].
	// So we need to add 16 to the result of the modulo operation. Execpt for -16, because -16 % 16 = 0.
	// We can use the following formula: (block_pos % 16) + (16 & ((block_pos % 16) >> 31))
	// If block_pos is negative, it's sign bit will be 1, so block_pos >> 31 will be 0xFFFFFFFF, and 16 & 0xFFFFFFFF = 16.
	// If block_pos is positive, it's sign bit will be 0, so block_pos >> 31 will be 0x00000000, and 16 & 0x00000000 = 0.
	// Note that we do ((block_pos % 16) >> 31) and not (block_pos >> 31) so that when block_pos is 16, the result will be 0 and not considered as negative.

	constexpr int shift = sizeof(int) * 8 - 1;
	const glm::ivec3 mod = block_pos % CHUNK_SIZE_IVEC3;
	return mod + (CHUNK_SIZE_IVEC3 & (mod >> shift));
}
