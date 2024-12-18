#pragma once

#include "define.hpp"
#include "vk_define.hpp"
#include "DebugGui.hpp"
#include "Timer.hpp"
#include "logger.hpp"
#include "ObjLoader.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include "Chunk.hpp"
#include "hashes.hpp"

#include <unordered_map>

// vec3 pos: 96
// vec3 normal: 96
// vec2 texCoord: 64
// uint32_t texLayer: 32
// uint8_t ao: 8
// uint8_t light: 8
// Total: 304 bits

// pos: 5+5+10 = 20 (x: [0..16], y: [0..512], z: [0..16])
// normal: 3 (only 6 different normals, mapped to 6 values)
// texCoord: 5+5 = 10 (x: [0..16], y: [0..16])
// texLayer: 32
// ao: 2 [0..3]
// light: 8
// Total: 75 bits

// pos x          pos z   tex xy            ao
// -----          -----   ----------        --
// 0000000000000000000000000000000000000000000000000000000000000000
//      ----------     ---          --------
//      pos y        normal          light

//  texLayer
// --------------------------------
// 0000000000000000000000000000000000000000000000000000000000000000

struct BlockVertex
{
	uint64_t data[2];

	BlockVertex(
		const glm::ivec3 & pos,
		const uint32_t normal,
		const glm::ivec2 & texCoord,
		const uint32_t texLayer,
		const uint8_t ao,
		const uint8_t light
	)
	{
		bzero(data, sizeof(data));
		data[0] |= static_cast<uint64_t>(pos.x) & 0b11111;
		data[0] |= (static_cast<uint64_t>(pos.y) & 0b1111111111) << 5;
		data[0] |= (static_cast<uint64_t>(pos.z) & 0b11111) << 15;
		data[0] |= (static_cast<uint64_t>(normal) & 0b111) << 20;
		data[0] |= (static_cast<uint64_t>(texCoord.x) & 0b11111) << 23;
		data[0] |= (static_cast<uint64_t>(texCoord.y) & 0b11111) << 28;
		data[0] |= (static_cast<uint64_t>(light) & 0xFF) << 33;
		data[0] |= (static_cast<uint64_t>(ao) & 0b11) << 41;

		data[1] |= (static_cast<uint64_t>(texLayer) & 0xFFFFFFFF);
	}

	static constexpr uint32_t getMaxTextureCoord()
	{
		return 16;
	}

	static VkVertexInputBindingDescription getBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(BlockVertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions()
	{
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions(1);

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R64G64_UINT;
		attributeDescriptions[0].offset = offsetof(BlockVertex, data);

		return attributeDescriptions;
	}
};

// struct BlockVertex
// {
// 	glm::vec3 pos;
// 	glm::vec3 normal;
// 	glm::vec2 texCoord;
// 	uint32_t texLayer;
// 	uint8_t ao;
// 	uint8_t light;

// 	static VkVertexInputBindingDescription getBindingDescription()
// 	{
// 		VkVertexInputBindingDescription bindingDescription{};
// 		bindingDescription.binding = 0;
// 		bindingDescription.stride = sizeof(BlockVertex);
// 		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

// 		return bindingDescription;
// 	}

// 	static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions()
// 	{
// 		std::vector<VkVertexInputAttributeDescription> attributeDescriptions(6);

// 		attributeDescriptions[0].binding = 0;
// 		attributeDescriptions[0].location = 0;
// 		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
// 		attributeDescriptions[0].offset = offsetof(BlockVertex, pos);

// 		attributeDescriptions[1].binding = 0;
// 		attributeDescriptions[1].location = 1;
// 		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
// 		attributeDescriptions[1].offset = offsetof(BlockVertex, normal);

// 		attributeDescriptions[2].binding = 0;
// 		attributeDescriptions[2].location = 2;
// 		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
// 		attributeDescriptions[2].offset = offsetof(BlockVertex, texCoord);

// 		attributeDescriptions[3].binding = 0;
// 		attributeDescriptions[3].location = 3;
// 		attributeDescriptions[3].format = VK_FORMAT_R32_UINT;
// 		attributeDescriptions[3].offset = offsetof(BlockVertex, texLayer);

// 		attributeDescriptions[4].binding = 0;
// 		attributeDescriptions[4].location = 4;
// 		attributeDescriptions[4].format = VK_FORMAT_R8_UINT;
// 		attributeDescriptions[4].offset = offsetof(BlockVertex, ao);

// 		attributeDescriptions[5].binding = 0;
// 		attributeDescriptions[5].location = 5;
// 		attributeDescriptions[5].format = VK_FORMAT_R8_UINT;
// 		attributeDescriptions[5].offset = offsetof(BlockVertex, light);

// 		return attributeDescriptions;
// 	}

// 	bool operator==(const BlockVertex& other) const
// 	{
// 		return pos == other.pos
// 			&& normal == other.normal
// 			&& texCoord == other.texCoord
// 			&& texLayer == other.texLayer
// 			&& ao == other.ao
// 			&& light == other.light;
// 	}
// };

// namespace std
// {
// 	template<> struct hash<BlockVertex>
// 	{
// 		size_t operator()(const BlockVertex & vertex) const
// 		{
// 			return ((hash<glm::vec3>()(vertex.pos) ^
// 				(hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
// 				(hash<glm::vec2>()(vertex.texCoord) << 1);
// 		}
// 	};
// }

class CreateMeshData
{

public:

	enum
	{
		POS = 2,
		NEUT = 1,
		NEG = 0
	};

	enum class Dimensions
	{
		X = 0,
		Y = 1,
		Z = 2
	};

	/**
	 * @brief Create a Mesh Data object
	 *
	 * @param pos the position of the chunk in the chunk map
	 * @param size the number of chunks to create the mesh data
	 * @param chunk_map the chunk map
	 */
	CreateMeshData(const glm::ivec3 & pos, const glm::ivec3 & size, ChunkMap & chunk_map);

	CreateMeshData(const CreateMeshData &) = delete;
	CreateMeshData & operator=(const CreateMeshData &) = delete;

	CreateMeshData(CreateMeshData && other);
	CreateMeshData & operator=(CreateMeshData && other);

	~CreateMeshData();

	void unlock();

	BlockInfo::Type getBlock(const int x, const int y, const int z);
	BlockInfo::Type getBlock(const glm::ivec3 & pos);

	uint8_t getLight(const int x, const int y, const int z);
	uint8_t getLight(const glm::ivec3 & pos);

	void create();

	void createFace(
		const int dim_1,
		const int dim_2,
		const glm::ivec3 & start,
		const glm::ivec3 & max_iter,
		const std::array<int, 6> & indices_order,
		const std::array<int, 6> & indices_order_fliped,
		const glm::ivec3 & abs_normal,
		const int normal_signe,
		const int face
	);

	void createFaceWater(
		const int dim_1,
		const int dim_2,
		const glm::ivec3 & start,
		const glm::ivec3 & max_iter,
		const std::array<int, 6> & indices_order,
		const std::array<int, 6> & indices_order_fliped,
		const glm::ivec3 & abs_normal,
		const int normal_signe,
		const int face
	);

	std::array<uint8_t, 4> getAmbientOcclusion(
		const glm::ivec3 & pos,
		const int dim_1,
		const int dim_2
	);
	int getAmbientOcclusion(
		BlockInfo::Type side_1,
		BlockInfo::Type side_2,
		BlockInfo::Type corner
	);

	std::array<uint8_t, 4> getLight(
		const glm::ivec3 & pos,
		const int dim_1,
		const int dim_2
	);
	uint8_t getLight(uint8_t pos, uint8_t side_1, uint8_t side_2, uint8_t corner);


	std::vector<std::vector<std::vector<std::shared_ptr<Chunk>>>> chunks;
	std::vector<BlockVertex> vertices;
	std::vector<uint32_t> indices;

	std::vector<BlockVertex> water_vertices;
	std::vector<uint32_t> water_indices;

	std::shared_ptr<Chunk> getCenterChunk()
	{
		return chunks[NEUT][NEUT][NEUT];
	}

private:

	struct FaceData
	{
		TextureID texture;
		std::array<uint8_t, 4> ao;
		std::array<uint8_t, 4> light;

		bool operator==(const FaceData & other) const
		{
			return texture == other.texture && ao == other.ao && light == other.light;
		}
	};

	std::vector<std::vector<std::vector<FaceData>>> face_data;
	glm::ivec3 size;

};
