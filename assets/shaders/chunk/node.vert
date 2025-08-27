#version 460

layout(location = 0) in uvec3 Vertex;

layout(location = 0) out GeometryObj {
    vec3 GeoPos;    // Реальная позиция в мире
    flat uint Texture;   // Текстура
    vec2 UV;
} Geometry;

layout(push_constant) uniform UniformBufferObject {
    mat4 projview;
    mat4 model;
} ubo;


// struct NodeVertexStatic {
//     uint32_t
//         FX : 9, FY : 9, FZ : 9, // Позиция 15 -120 ~ 240 360 15 / 16
//         N1 : 4,                 // Не занято
//         LS : 1,                 // Масштаб карты освещения (1м/16 или 1м)
//         Tex : 18,               // Текстура
//         N2 : 14,                // Не занято
//         TU : 16, TV : 16;       // UV на текстуре
// };

void main()
{
    vec4 baseVec = ubo.model*vec4(
        float(Vertex.x & 0x1ff) / 16.f - 135/16.f,
        float((Vertex.x >> 9) & 0x1ff) / 16.f - 135/16.f,
        float((Vertex.x >> 18) & 0x1ff) / 16.f - 135/16.f,
        1
    );

    Geometry.GeoPos = baseVec.xyz;
    Geometry.Texture = Vertex.y & 0x3ffff;
    Geometry.UV = vec2(
        float(Vertex.z & 0xffff) / pow(2, 16),
        float((Vertex.z >> 16) & 0xffff) / pow(2, 16)
    );

    gl_Position = ubo.projview*baseVec;
}
