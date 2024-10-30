#include "Item.hpp"

ItemsInfo & g_items_info = ItemsInfo::getInstance();

ItemsInfo::ItemsInfo():
	m_info({
		{
			.type = ItemInfo::Type::GrassBlock,
			.block_id = BlockType::Grass
		},
		{
			.type = ItemInfo::Type::DirtBlock,
			.block_id = BlockType::Dirt
		},
		{
			.type = ItemInfo::Type::StoneBlock,
			.block_id = BlockType::Stone
		},
		{
			.type = ItemInfo::Type::WaterBlock,
			.block_id = BlockType::Water
		},
		{
			.type = ItemInfo::Type::GlassBlock,
			.block_id = BlockType::Glass
		},
		{
			.type = ItemInfo::Type::LightBlock,
			.block_id = BlockType::Light
		},
		{
			.type = ItemInfo::Type::WoodBlock,
			.block_id = BlockType::Wood
		},
		{
			.type = ItemInfo::Type::LeavesBlock,
			.block_id = BlockType::Leaves
		},
		{
			.type = ItemInfo::Type::SandBlock,
			.block_id = BlockType::Sand
		}
	})
{
}

