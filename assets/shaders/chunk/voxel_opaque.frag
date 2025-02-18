#version 450

layout(location = 0) in FragmentObj {
    vec3 GeoPos;    // Реальная позиция в мире
    flat uint VoxMTL;    // Материал вокселя
    vec2 LUV;
    flat uint Place;
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
    uv = mod(uv, 1);

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
