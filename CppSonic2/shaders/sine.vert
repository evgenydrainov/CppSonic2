attribute vec3 in_Position;
attribute vec3 in_Normal;
attribute vec4 in_Color;
attribute vec2 in_TexCoord;

varying vec4 v_Color;
varying vec2 v_TexCoord;
varying vec3 v_WorldPos;

uniform mat4 u_MVP;
uniform mat4 u_ModelView;

void main() {
	gl_Position = u_MVP * vec4(in_Position, 1.0);

	v_Color    = in_Color;
	v_TexCoord = in_TexCoord;
	v_WorldPos = (u_ModelView * vec4(in_Position, 1.0)).xyz;
}
