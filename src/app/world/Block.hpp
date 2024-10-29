#pragma once

#include "define.hpp"
#include "HitBox.hpp"

#include <string>
#include <vector>

typedef uint64_t BlockProperties;
typedef uint32_t TextureID;

// Block properties
#define BLOCK_PROPERTY_NONE		0U		// default
#define BLOCK_PROPERTY_SOLID	1U		// solid block (can be walked on)
#define BLOCK_PROPERTY_OPAQUE	1U << 1 // opaque block (cannot be seen through)
#define BLOCK_PROPERTY_CUBE		1U << 2 // cube block (is a standard cube shape)
#define BLOCK_PROPERTY_LIGHT	1U << 3 // light block (emits light)
#define BLOCK_PROPERTY_FLUID	1U << 4 // fluid block (flows)

// Block faces coresponding to the texture array
#define BLOCK_FACE_TOP		0
#define BLOCK_FACE_BOTTOM	1
#define BLOCK_FACE_RIGHT	2
#define BLOCK_FACE_LEFT		3
#define BLOCK_FACE_FRONT	4
#define BLOCK_FACE_BACK		5
class BlockInfo
{

public:

	enum class Type : uint16_t
	{
		Air,
		Grass,
		Dirt,
		Stone,
		Water,
		Glass,
		Light,
		Wood,
		Leaves,
		None

	} const id = Type::None;

	const TextureID texture[6];
	const BlockProperties properties;
	const HitBox hitbox;
	const uint8_t emit_light; // light level emitted by the block
	const uint8_t absorb_light; // light level absorbed by the block


	static inline const std::vector<std::string> texture_names = {
		"assets/textures/block/grass_top.png", // 0
		"assets/textures/block/grass_top.png", // 1
		"assets/textures/block/grass_side.png", // 2
		"assets/textures/block/dirt.png", // 3
		"assets/textures/block/stone.png", // 4
		"assets/textures/block/water.png", // 5
		"assets/textures/block/glass_clear.png", // 6
		"assets/textures/block/light.png", // 7
		"assets/textures/block/wood_top.png", // 8
		"assets/textures/block/wood.png", // 9
		"assets/textures/block/oak_leaves.png", // 10
		// "assets/textures/block/debug/white.png",
		// "assets/textures/block/debug/right.png",
		// "assets/textures/block/debug/left.png",
		// "assets/textures/block/debug/front.png",
		// "assets/textures/block/debug/top.png",
		// "assets/textures/block/debug/back_bottom.png",
	};

};

typedef BlockInfo::Type BlockType;
class BlocksInfo
{

public:

	static BlocksInfo & getInstance()
	{
		static BlocksInfo instance;
		return instance;
	}

	const BlockInfo & get(const BlockInfo::Type id)
	{
		return m_infos[static_cast<size_t>(id)];
	}

	bool hasProperty(const BlockInfo::Type id, const BlockProperties properties)
	{
		return (get(id).properties & properties) == properties;
	}

	bool hasProperty(const BlockInfo::Type id, const BlockProperties properties, const BlockProperties not_properties)
	{
		return (get(id).properties & (properties | not_properties)) == properties;
	}

private:

	std::vector<BlockInfo> m_infos;

	BlocksInfo();

};

extern BlocksInfo & g_blocks_info;
