#version 450

layout(location = 0) in FragmentObj {
    vec3 GeoPos;    // Реальная позиция в мире
    flat uint VoxMTL;    // Материал вокселя
    vec2 LUV;
    flat uint Place;
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
    uv = mod(uv, 1);

    AtlasEntry entry = MainAtlasLayout.Entries[texId];
    if((entry.Flags & ATLAS_ENTRY_VALID) == 0u)
        return vec4(((int(gl_FragCoord.x / 128) + int(gl_FragCoord.y / 128)) % 2) * vec3(1, 0, 1), 1);

    vec2 baseUV = vec2(uv.x, 1.0f - uv.y);
    vec2 atlasUV = mix(entry.UVMinMax.xy, entry.UVMinMax.zw, baseUV);
    atlasUV = clamp(atlasUV, entry.UVMinMax.xy, entry.UVMinMax.zw);
    return texture(MainAtlas, vec3(atlasUV, entry.Layer));
}

void main() {
    vec2 uv;

    switch(Fragment.Place) {
    case 0:
        uv = Fragment.GeoPos.xz; break;
    case 1:
        uv = Fragment.GeoPos.xy; break;
    case 2:
        uv = Fragment.GeoPos.zy; break;
    case 3:
        uv = Fragment.GeoPos.xz*vec2(-1, -1); break;
    case 4:
        uv = Fragment.GeoPos.xy*vec2(-1, 1); break;
    case 5:
        uv = Fragment.GeoPos.zy*vec2(-1, 1); break;
    default:
        uv = vec2(0);
    }

    Frame = atlasColor(Fragment.VoxMTL, uv);
}
