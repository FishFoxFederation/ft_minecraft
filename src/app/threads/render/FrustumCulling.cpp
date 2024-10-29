#include "FrustumCulling.hpp"

FrustumPoints::FrustumPoints(
	const glm::vec3 & pos,
	const glm::vec3 & front,
	const glm::vec3 & right,
	const glm::vec3 & up,
	float fov,
	float aspect,
	float near,
	float far
)
{
	float tanHalfFov = tan(fov / 2.0f);

	float nearHeight = 2.0f * tanHalfFov * near;
	float nearWidth = nearHeight * aspect;

	float farHeight = 2.0f * tanHalfFov * far;
	float farWidth = farHeight * aspect;

	glm::vec3 farCenter = pos + front * far;
	glm::vec3 nearCenter = pos + front * near;

	glm::vec3 farUp = up * (farHeight / 2.0f);
	glm::vec3 farRight = right * (farWidth / 2.0f);

	glm::vec3 nearUp = up * (nearHeight / 2.0f);
	glm::vec3 nearRight = right * (nearWidth / 2.0f);

	points[0] = nearCenter - nearRight + nearUp;
	points[1] = nearCenter + nearRight + nearUp;
	points[2] = nearCenter + nearRight - nearUp;
	points[3] = nearCenter - nearRight - nearUp;

	points[4] = farCenter - farRight + farUp;
	points[5] = farCenter + farRight + farUp;
	points[6] = farCenter + farRight - farUp;
	points[7] = farCenter - farRight - farUp;
}
