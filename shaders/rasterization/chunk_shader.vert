#version 450

layout(set = 0, binding = 0) uniform CameraMatrices
{
	mat4 view;
	mat4 projection;
}cm;

layout(push_constant) uniform PushConstants
{
	mat4 model;
}pc;

layout(location = 0) in vec3 positions;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoords;
layout(location = 3) in uint texLayer;
layout(location = 4) in uint ao;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragTexCoords;
layout(location = 2) out vec4 fragPosWorldSpace;
layout(location = 3) out float fragAO;

void main()
{
	gl_Position = cm.projection * cm.view * pc.model * vec4(positions, 1.0);

	fragNormal = normal;
	fragTexCoords = vec3(texCoords, texLayer);

	fragPosWorldSpace = pc.model * vec4(positions, 1.0);

	fragAO = float(ao);
}
