#version 450

layout(location = 0) in FragmentObj {
    vec3 GeoPos;    // Реальная позиция в мире
    vec3 Normal;
    flat uint Texture;   // Текстура
    vec2 UV;
} Fragment;

layout(location = 0) out vec4 Frame;

struct InfoSubTexture {
    uint Flags; // 1 isExist
    uint PosXY, WidthHeight;

    uint AnimationFrames_AnimationTimePerFrame;
};

uniform layout(set = 0, binding = 0) sampler2D MainAtlas;
layout(set = 0, binding = 1) readonly buffer MainAtlasLayoutObj {
    uint SubsCount;
    uint Counter;
    uint WidthHeight;

    InfoSubTexture SubTextures[];
} MainAtlasLayout;

uniform layout(set = 1, binding = 0) sampler2D LightMap;
layout(set = 1, binding = 1) readonly buffer LightMapLayoutObj {
    vec3 Color;
} LightMapLayout;

vec4 atlasColor(uint texId, vec2 uv)
{
    uint flags = (texId & 0xffff0000) >> 16;
    texId &= 0xffff;
    vec4 color = vec4(uv, 0, 1);
    
    if((flags & (2 | 4)) > 0)
    {
        if((flags & 2) > 0)
            color = vec4(1, 1, 1, 1);
        else if((flags & 4) > 0)
        {
            color = vec4(1);
        }
        
    }
    else if(texId >= uint(MainAtlasLayout.SubsCount))
        return vec4(((int(gl_FragCoord.x / 128) + int(gl_FragCoord.y / 128)) % 2 ) * vec3(0, 1, 1), 1);
    else {
        InfoSubTexture texInfo = MainAtlasLayout.SubTextures[texId];
        if(texInfo.Flags == 0)
            return vec4(((int(gl_FragCoord.x / 128) + int(gl_FragCoord.y / 128)) % 2 ) * vec3(1, 0, 1), 1);

        uint posX = texInfo.PosXY & 0xffff;
        uint posY = (texInfo.PosXY >> 16) & 0xffff;
        uint width = texInfo.WidthHeight & 0xffff;
        uint height = (texInfo.WidthHeight >> 16) & 0xffff;
        uint awidth = MainAtlasLayout.WidthHeight & 0xffff;
        uint aheight = (MainAtlasLayout.WidthHeight >> 16) & 0xffff;

        if((flags & 1) > 0)
            color = texture(MainAtlas, vec2((posX+0.5f+uv.x*(width-1))/awidth, (posY+0.5f+(1-uv.y)*(height-1))/aheight));
        else
            color = texture(MainAtlas, vec2((posX+uv.x*width)/awidth, (posY+(1-uv.y)*height)/aheight));
    }


    return color;    
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