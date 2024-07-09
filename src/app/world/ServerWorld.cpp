#include "ServerWorld.hpp"

ServerWorld::ServerWorld(Server & server, ThreadPool & threadPool)
:	World(threadPool), 
	m_server(server)
{
	addTicket({TICKET_LEVEL_PLAYER, glm::ivec3(0, 0, 0)});
	// std::shared_ptr<Chunk> chunk;

	// std::lock_guard lock(m_chunks_mutex);

	// chunk = m_world_generator.generateChunkColumn(0, 0);
	// this->m_chunks.insert({glm::ivec3(0, 0, 0), chunk});

	// chunk = m_world_generator.generateChunkColumn(0, 1);
	// this->m_chunks.insert({glm::ivec3(0, 0, 1), chunk});

	// chunk = m_world_generator.generateChunkColumn(1, 0);
	// this->m_chunks.insert({glm::ivec3(1, 0, 0), chunk});

	// chunk = m_world_generator.generateChunkColumn(1, 1);
	// this->m_chunks.insert({glm::ivec3(1, 0, 1), chunk});
}

ServerWorld::~ServerWorld()
{
}

void ServerWorld::update()
{
	ZoneScopedN("Block Update");

	{
		std::lock_guard lock(m_tickets_mutex);

		LOG_INFO("BU size: " << m_block_update_chunks.size());
	}
	savePlayerPositions();
	// MAIN BLOCK UPDATE FUNCTION

	// do all block updates
	updateBlocks();

	// do all chunk updates
	updateTickets();

	waitForFutures();

	// if player changed chunk send new chunks and update observations
	updatePlayerPositions();
}

void ServerWorld::savePlayerPositions()
{
	std::lock_guard lock(m_players_mutex);

	m_current_tick_player_positions.clear();
	for (auto & [player_id, player] : m_players)
	{
		std::lock_guard lock(player->mutex);
		m_current_tick_player_positions.insert({player_id, player->transform.position});
		if (m_last_tick_player_positions.contains(player_id))
		{
			glm::ivec3 last_chunk = getChunkPosition(m_last_tick_player_positions.at(player_id));
			glm::ivec3 current_chunk = getChunkPosition(player->transform.position);

			//change ticket if player changed chunk
			if (last_chunk != current_chunk)
			{
				Ticket next_ticket{ TICKET_LEVEL_PLAYER, current_chunk };
				player->player_ticket_id = changeTicket(player->player_ticket_id, next_ticket);
			}
		}
	}
}

void ServerWorld::updatePlayerPositions()
{
	std::lock_guard lock(m_players_mutex);
	for (auto & [player_id, current_pos] : m_current_tick_player_positions)
	{
		ChunkLoadUnloadData data = updateChunkObservations(player_id);
		sendChunkLoadUnloadData(data, player_id);
		m_last_tick_player_positions.at(player_id) = m_current_tick_player_positions.at(player_id);
	}
}


