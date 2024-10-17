#version 450

#include "common.glsl"

layout(set = BINDLESS_DESCRIPTOR_SET, binding = BINDLESS_PARAMS_BINDING) uniform bindlessParams
{
	BindlessDescriptorParams bindless_params;
};
layout(set = BINDLESS_DESCRIPTOR_SET, binding = BINDLESS_UNIFORM_BUFFER_BINDING) uniform CameraMatrices
{
	ViewProjMatrices cm;
} camera_matrices[BINDLESS_DESCRIPTOR_MAX_COUNT];

layout(push_constant) uniform PushConstants
{
	ObjectData obj_data;
};

layout(location = 0) in vec3 positions;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoords;

layout(location = 0) out vec2 fragTexCoords;

void main()
{
	const ViewProjMatrices cam = camera_matrices[bindless_params.camera_ubo_index].cm;
	gl_Position = cam.proj * cam.view * obj_data.matrix * vec4(positions, 1.0);

	fragTexCoords = texCoords;
}
