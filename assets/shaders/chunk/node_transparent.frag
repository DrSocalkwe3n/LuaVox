#version 450

layout(location = 0) in FragmentObj {
    vec3 GeoPos;    // Реальная позиция в мире
    vec3 Normal;
    flat uint Texture;   // Текстура
    vec2 UV;
} Fragment;

layout(location = 0) out vec4 Frame;

uniform layout(set = 0, binding = 0) sampler2DArray MainAtlas;
layout(set = 0, binding = 1) readonly buffer MainAtlasLayoutObj {
    vec3 Color;
} MainAtlasLayout;

uniform layout(set = 1, binding = 0) sampler2DArray LightMap;
layout(set = 1, binding = 1) readonly buffer LightMapLayoutObj {
    vec3 Color;
} LightMapLayout;

void main() {
    Frame = vec4(1);
}