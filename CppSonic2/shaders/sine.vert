#version 300 es

precision mediump float;

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec3 in_Normal;
layout(location = 2) in vec4 in_Color;
layout(location = 3) in vec2 in_TexCoord;

out vec4 v_Color;
out vec2 v_TexCoord;
out vec3 v_WorldPos;

uniform mat4 u_ModelView;
uniform mat4 u_Proj;

void main() {
	mat4 MVP = u_Proj * u_ModelView;

	gl_Position = MVP * vec4(in_Position, 1.0);

	v_Color    = in_Color;
	v_TexCoord = in_TexCoord;
	v_WorldPos = (u_ModelView * vec4(in_Position, 1.0)).xyz;
}
