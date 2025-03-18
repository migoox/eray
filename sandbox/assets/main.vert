#version 330 core
#external_definition TEST
#include "blinn_phong.glsl"
				
layout(location = 0) in vec3 a_Position;

uniform vec2 u_resolution;
uniform float u_camSize;

out vec2 v_Pos;

void main() {
    v_Pos = a_Position.xy * u_camSize;
    v_Pos.x *= u_resolution.x / u_resolution.y;
    gl_Position = vec4(a_Position, 1.0);
}
