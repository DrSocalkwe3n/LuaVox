#version 450

layout(location = 0) in Fragment {
    vec3 GeoPos;    // Реальная позиция в мире
    uint VoxMTL;    // Материал вокселя
    vec2 LUV;
} fragment;

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
