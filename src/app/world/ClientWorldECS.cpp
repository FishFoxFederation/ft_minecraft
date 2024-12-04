#include "ClientWorld.hpp"

/*************************************
 *  MOBS
 *************************************/

void ClientWorld::createBaseMob(const glm::dvec3 & position, uint64_t player_id)
{
	auto [succes, entity] = m_ecs_manager.createEntity();

	if (!succes)
	{
		LOG_ERROR("Failed to create mob entity");
		return;
	}
	m_render_api.addEntity(entity, {m_render_api.getCubeMeshId(), glm::translate(glm::dmat4(1.0), position)});

	m_ecs_manager.add(entity,
		Position{position},
		Velocity{glm::vec3(0.0f)},
		Acceleration{glm::vec3(10.0f, 10.0f, 10.0f)},
		AITarget{player_id},
		Mesh{0}
	);

	LOG_INFO("Created mob entity: " << entity);
}

void ClientWorld::MovementSystem(const double delta_time_second)
{
	auto view = m_ecs_manager.view<Position, Velocity>();


	for ( auto entity : view )
	{
		auto [ position, velocity ] = m_ecs_manager.getComponents<Position, Velocity>(entity);

		position.p += velocity.v * delta_time_second;
	}
}

void ClientWorld::renderSystem()
{
	auto view = m_ecs_manager.view<Position, Mesh>();
	for (auto entity : view )
	{
		auto [ position, mesh ] = m_ecs_manager.getComponents<Position, Mesh>(entity);

		m_render_api.updateEntity(entity, [position](MeshRenderData & data)
		{
			data.model = glm::translate(glm::dmat4(1.0), position.p);
		});
	}
}

void ClientWorld::AISystem()
{
	auto view = m_ecs_manager.view<Position, AITarget, Velocity, Acceleration>();

	for (auto entity : view)
	{
		auto [ position, ai_target, velocity, acceleration ] = m_ecs_manager.getComponents<Position, AITarget, Velocity, Acceleration>(entity);

		auto player_position = getPlayerPosition(ai_target.target_id);

		glm::dvec3 diff = player_position - position.p;
		if (glm::length(diff) >= 0.0001)
			diff = glm::normalize(diff);

		velocity.v = diff * acceleration.a;
	}
}

void ClientWorld::updateSystems(const double delta_time_second)
{
	// LOG_INFO("Updating systems");
	AISystem();
	MovementSystem(delta_time_second);
	renderSystem();
}
