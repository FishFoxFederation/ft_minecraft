#pragma once

#include "define.hpp"
#include "Block.hpp"

#include <cstdint>

class ItemInfo
{

public:

	enum class Type : uint16_t
	{
		GrassBlock,
		DirtBlock,
		StoneBlock,
		WaterBlock,
		GlassBlock,
		LightBlock,
		WoodBlock,
		LeavesBlock,
		SandBlock,
		None

	} const type = Type::None;

	const BlockInfo::Type block_id;
	uint64_t mesh_id = 0;

private:

};

class ItemsInfo
{

public:

	static ItemsInfo & getInstance()
	{
		static ItemsInfo instance;
		return instance;
	}

	ItemInfo & get(const ItemInfo::Type type)
	{
		return m_info[static_cast<size_t>(type)];
	}

	ItemInfo & get(const size_t index)
	{
		return m_info[index];
	}

	size_t count() const
	{
		return m_info.size();
	}

private:

	std::vector<ItemInfo> m_info;

	ItemsInfo();
};

extern ItemsInfo & g_items_info;
