#version 460

// layout(early_fragment_tests) in;

layout(location = 0) in FragmentObj {
    vec3 GeoPos;    // Реальная позиция в мире
    vec3 Normal;
    flat uint Texture;   // Текстура
    vec2 UV;
} Fragment;

layout(location = 0) out vec4 Frame;

struct AtlasEntry {
    vec4 UVMinMax;
    uint Layer;
    uint Flags;
    uint _Pad0;
    uint _Pad1;
};

const uint ATLAS_ENTRY_VALID = 1u;

uniform layout(set = 0, binding = 0) sampler2DArray MainAtlas;
layout(set = 0, binding = 1) readonly buffer MainAtlasLayoutObj {
    AtlasEntry Entries[];
} MainAtlasLayout;

uniform layout(set = 1, binding = 0) sampler2DArray LightMap;
layout(set = 1, binding = 1) readonly buffer LightMapLayoutObj {
    vec3 Color;
} LightMapLayout;

vec4 atlasColor(uint texId, vec2 uv)
{
    AtlasEntry entry = MainAtlasLayout.Entries[texId];
    if((entry.Flags & ATLAS_ENTRY_VALID) == 0u)
        return vec4(((int(gl_FragCoord.x / 128) + int(gl_FragCoord.y / 128)) % 2) * vec3(1, 0, 1), 1);

    vec2 baseUV = vec2(uv.x, 1.0f - uv.y);
    vec2 atlasUV = mix(entry.UVMinMax.xy, entry.UVMinMax.zw, baseUV);
    atlasUV = clamp(atlasUV, entry.UVMinMax.xy, entry.UVMinMax.zw);
    return texture(MainAtlas, vec3(atlasUV, entry.Layer));
}

vec3 blendOverlay(vec3 base, vec3 blend) {
    vec3 result;
    for (int i = 0; i < 3; ++i) {
        if (base[i] <= 0.5)
            result[i] = 2.0 * base[i] * blend[i];
        else
            result[i] = 1.0 - 2.0 * (1.0 - base[i]) * (1.0 - blend[i]);
    }
    return result;
}

void main() {
    Frame = atlasColor(Fragment.Texture, Fragment.UV);
    Frame.xyz *= max(0.2f, dot(Fragment.Normal, normalize(vec3(0.5, 1, 0.8))));
    // Frame = vec4(blendOverlay(vec3(Frame), vec3(Fragment.GeoPos/64.f)), Frame.w);
    if(Frame.w == 0)
        discard;
}
