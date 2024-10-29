#pragma once

#include <glm/glm.hpp>

struct FrustumPoints
{
	FrustumPoints(
		const glm::vec3 & pos,
		const glm::vec3 & front,
		const glm::vec3 & right,
		const glm::vec3 & up,
		float fov,
		float aspect,
		float near,
		float far
	);

	glm::vec3 points[8];
};


