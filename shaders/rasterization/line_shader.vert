#version 450

#include "common.glsl"

layout(set = 0, binding = 0) uniform CameraMatrices
{
	ViewProjMatrices cm;
};

layout(push_constant) uniform PushConstants
{
	LinePipelinePushConstant pc;
};

layout(location = 0) in vec3 positions;

void main()
{
    gl_Position = cm.proj * cm.view * pc.model * vec4(positions, 1.0);
}
