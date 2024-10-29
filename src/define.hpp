#pragma once

/**
 * @brief a define for the project name
 *
 */
#define PROJECT_NAME "my_project"

#define WORLD_Y_MAX 256
#define RENDER_DISTANCE 16
#define LOAD_DISTANCE (RENDER_DISTANCE + 2)
#define IP_ADDRESS "localhost"
#define PORT 4245

constexpr static int TICKET_LEVEL_ENTITY_UPDATE = 31;
constexpr static int TICKET_LEVEL_BLOCK_UPDATE = 32;
constexpr static int TICKET_LEVEL_BORDER = 33;
constexpr static int TICKET_LEVEL_INACTIVE = 34;

constexpr static int SPAWN_TICKET_LEVEL = 30;
constexpr static int PLAYER_TICKET_LEVEL = 20;

constexpr static int SERVER_LOAD_DISTANCE = TICKET_LEVEL_INACTIVE - PLAYER_TICKET_LEVEL;
