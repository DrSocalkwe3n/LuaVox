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
//         FX : 11, FY : 11, N1 : 10, // Позиция, 64 позиции на метр, +3.5м запас
//         FZ : 11,                 // Позиция
//         LS : 1,                  // Масштаб карты освещения (1м/16 или 1м)
//         Tex : 18,                // Текстура
//         N2 : 2,                  // Не занято
//         TU : 16, TV : 16;        // UV на текстуре
// };

void main()
{
    uint fx = Vertex.x & 0x7ffu;
    uint fy = (Vertex.x >> 11) & 0x7ffu;
    uint fz = Vertex.y & 0x7ffu;

    vec4 baseVec = ubo.model*vec4(
        float(fx) / 64.f - 3.5f,
        float(fy) / 64.f - 3.5f,
        float(fz) / 64.f - 3.5f,
        1
    );

    Geometry.GeoPos = baseVec.xyz;
    Geometry.Texture = (Vertex.y >> 12) & 0x3ffffu;
    Geometry.UV = vec2(
        float(Vertex.z & 0xffff) / pow(2, 16),
        float((Vertex.z >> 16) & 0xffff) / pow(2, 16)
    );

    gl_Position = ubo.projview*baseVec;
}
