#version 450 core

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vUVW;

layout (location = 2) uniform mat4 MVP;

out vec3 UVW;

void main() { 
	gl_Position = MVP * vec4(vPosition, 1);
	UVW = vUVW;
}