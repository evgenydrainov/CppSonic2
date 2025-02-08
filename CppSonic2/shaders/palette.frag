#version 300 es

precision mediump float;

layout(location = 0) out vec4 FragColor;

in vec4 v_Color;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform sampler2D u_Palette;
uniform float u_PaletteIndex;
uniform float u_PaletteWidth;
uniform float u_PaletteHeight;

void main() {
	vec4 index = texture(u_Texture, v_TexCoord);

	float texel_width  = 1.0 / u_PaletteWidth;
	float texel_height = 1.0 / u_PaletteHeight;

	// Add half a pixel to avoid floating point precision problems.
	float lookup_x = index.r + texel_width*0.5;
	float lookup_y = u_PaletteIndex/u_PaletteHeight + texel_height*0.5;
	vec4 color = texture(u_Palette, vec2(lookup_x, lookup_y));

	FragColor = vec4(color.rgb, index.a) * v_Color;
}
