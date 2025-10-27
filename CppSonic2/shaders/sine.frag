varying vec4 v_Color;
varying vec2 v_TexCoord;
varying vec3 v_WorldPos;

uniform sampler2D u_Texture;
uniform float u_Time;
uniform float u_WaterPosY;

void main() {
	vec2 pos = v_TexCoord;

	if (v_WorldPos.y > u_WaterPosY) {
		pos.x += sin(u_Time*2.0 + v_WorldPos.y*0.1) * (5.0/1024.0);
	}

	vec4 color = texture2D(u_Texture, pos);

	gl_FragColor = color * v_Color;
}
