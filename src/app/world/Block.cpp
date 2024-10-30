#include "Block.hpp"

BlocksInfo & g_blocks_info = BlocksInfo::getInstance();

BlocksInfo::BlocksInfo():
	m_infos({
		{
			.id = BlockInfo::Type::Air,
			.texture = { 0, 0, 0, 0, 0, 0 },
			.properties = BLOCK_PROPERTY_NONE,
			.hitbox = {{0, 0, 0}, {1, 1, 1}},
			.emit_light = 0,
			.absorb_light = 0
		},
		{
			.id = BlockInfo::Type::Grass,
			.texture = {
				1, // grass_top
				3, // dirt
				2, // grass_side
				2, // grass_side
				2, // grass_side
				2  // grass_side
			},
			.properties =
				BLOCK_PROPERTY_SOLID
				| BLOCK_PROPERTY_OPAQUE
				| BLOCK_PROPERTY_CUBE,
			.hitbox = {{0, 0, 0}, {1, 1, 1}},
			.emit_light = 0,
			.absorb_light = 15
		},
		{
			.id = BlockInfo::Type::Dirt,
			.texture = {
				3, // dirt
				3, // dirt
				3, // dirt
				3, // dirt
				3, // dirt
				3  // dirt
			},
			.properties =
				BLOCK_PROPERTY_SOLID
				| BLOCK_PROPERTY_OPAQUE
				| BLOCK_PROPERTY_CUBE,
			.hitbox = {{0, 0, 0}, {1, 1, 1}},
			.emit_light = 0,
			.absorb_light = 15
		},
		{
			.id = BlockInfo::Type::Stone,
			.texture = {
				4, // stone
				4, // stone
				4, // stone
				4, // stone
				4, // stone
				4  // stone
			},
			.properties =
				BLOCK_PROPERTY_SOLID
				| BLOCK_PROPERTY_OPAQUE
				| BLOCK_PROPERTY_CUBE,
			.hitbox = {{0, 0, 0}, {1, 1, 1}},
			.emit_light = 0,
			.absorb_light = 15
		},
		{
			.id = BlockInfo::Type::Sand,
			.texture = {
				11, // sand
				11, // sand
				11, // sand
				11, // sand
				11, // sand
				11  // sand
			},
			.properties =
				BLOCK_PROPERTY_SOLID
				| BLOCK_PROPERTY_OPAQUE
				| BLOCK_PROPERTY_CUBE,
			.hitbox = {{0, 0, 0}, {1, 1, 1}},
			.emit_light = 0,
			.absorb_light = 15
		},
		{
			.id = BlockInfo::Type::Water,
			.texture = {
				5, // water
				5, // water
				5, // water
				5, // water
				5, // water
				5  // water
			},
			.properties =
				BLOCK_PROPERTY_FLUID,
			.hitbox = {{0, 0, 0}, {1, 1, 1}},
			.emit_light = 0,
			.absorb_light = 1
		},
		{
			.id = BlockInfo::Type::Glass,
			.texture = {
				6, // glass
				6, // glass
				6, // glass
				6, // glass
				6, // glass
				6  // glass
			},
			.properties =
				BLOCK_PROPERTY_SOLID
				| BLOCK_PROPERTY_CUBE,
			.hitbox = {{0, 0, 0}, {1, 1, 1}},
			.emit_light = 0,
			.absorb_light = 0
		},
		{
			.id = BlockInfo::Type::Light,
			.texture = {
				7, // light
				7, // light
				7, // light
				7, // light
				7, // light
				7  // light
			},
			.properties =
				BLOCK_PROPERTY_SOLID
				| BLOCK_PROPERTY_OPAQUE
				| BLOCK_PROPERTY_CUBE
				| BLOCK_PROPERTY_LIGHT,
			.hitbox = {{0, 0, 0}, {1, 1, 1}},
			.emit_light = 15,
			.absorb_light = 0
		},
		{
			.id = BlockInfo::Type::Wood,
			.texture = {
				8, // wood_top
				8, // wood_top
				9, // wood
				9, // wood
				9, // wood
				9  // wood
			},
			.properties = 
				BLOCK_PROPERTY_SOLID
				| BLOCK_PROPERTY_OPAQUE
				| BLOCK_PROPERTY_CUBE,
			.hitbox = {{0, 0, 0}, {1, 1, 1}},
			.emit_light = 0,
			.absorb_light = 15
		},
		{
			.id = BlockInfo::Type::Leaves,
			.texture = {
				10, // oak_leaves
				10, // oak_leaves
				10, // oak_leaves
				10, // oak_leaves
				10, // oak_leaves
				10  // oak_leaves
			},
			.properties =
				BLOCK_PROPERTY_SOLID
				| BLOCK_PROPERTY_CUBE,
			.hitbox = {{0, 0, 0}, {1, 1, 1}},
			.emit_light = 0,
			.absorb_light = 0
		}
	})
{
}

