#version 300 es

precision highp float;

layout(location = 0) out vec4 FragColor;

in vec4 v_Color;
in vec2 v_TexCoord;
in vec3 v_WorldPos;

uniform sampler2D u_Texture;
uniform float u_Time;
uniform float u_WaterPosY;

void main() {
	vec2 pos = v_TexCoord;

	if (v_WorldPos.y > u_WaterPosY) {
		pos.x += sin(u_Time*2.0 + v_WorldPos.y*0.1) * (5.0/1024.0);
	}

	vec4 color = texture(u_Texture, pos);

	FragColor = color * v_Color;
}
