#version 450

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

layout(location = 0) in highp uvec3 Geometry[];

layout(location = 0) out Fragment {
    vec3 GeoPos;    // Реальная позиция в мире
    uint VoxMTL;    // Материал вокселя
    vec2 LUV;
} fragment;

layout(push_constant) uniform UniformBufferObject {
    mat4 projview;
    mat4 model;
} ubo;

// struct VoxelVertexPoint {
//     uint32_t 
//         FX : 9, FY : 9, FZ : 9, // Позиция
//         Place : 3,              // Положение распространения xz, xy, zy, и обратные
//         N1 : 1,                 // Не занято
//         LS : 1,                 // Масштаб карты освещения (1м/16 или 1м)
//         TX : 8, TY : 8,         // Размер+1
//         VoxMtl : 16,            // Материал вокселя DefVoxelId_t
//         LU : 14, LV : 14,       // Позиция на карте освещения
//         N2 : 2;                 // Не занято
// };

void main() {
    vec4 baseVec = vec4(
        float(Geometry[0].x & 0x1ff) / 16.f,
        float((Geometry[0].x >> 9) & 0x1ff) / 16.f,
        float((Geometry[0].x >> 18) & 0x1ff) / 16.f,
        1
    );

    vec2 size = vec2(
        float(Geometry[0].y & 0xff)+1,
        float((Geometry[0].y >> 8) & 0xff)+1
    );

    uint voxMTL = ((Geometry[0].y >> 16) & 0xffff);
    vec2 luv = vec2(float(Geometry[0].z & 0x3fff)+0.5f, float((Geometry[0].z >> 14) & 0x3fff)+0.5f);

    // Стартовая вершина
    vec4 tempVec = baseVec;
    tempVec = ubo.model*tempVec;
    fragment.GeoPos = vec3(tempVec);
    fragment.VoxMTL = voxMTL;
    fragment.LUV = luv;
    gl_Position = ubo.projview*tempVec;
    EmitVertex();
    

    int place = int(Geometry[0].x >> 27) & 0x3;
    switch(place) {
    case 0: // xz
        tempVec = baseVec;
        tempVec.x += size.x;
        tempVec = ubo.model*tempVec;
        fragment.GeoPos = vec3(tempVec);
        fragment.VoxMTL = voxMTL;
        fragment.LUV = luv+vec2(size.x, 0);
        gl_Position = ubo.projview*tempVec;
        EmitVertex();

        tempVec = baseVec;
        tempVec.z += size.y;
        tempVec = ubo.model*tempVec;
        fragment.GeoPos = vec3(tempVec);
        fragment.VoxMTL = voxMTL;
        fragment.LUV = luv+vec2(0, size.y);
        gl_Position = ubo.projview*tempVec;
        EmitVertex();

        tempVec = baseVec;
        tempVec.x += size.x;
        tempVec.z += size.y;
        tempVec = ubo.model*tempVec;
        fragment.GeoPos = vec3(tempVec);
        fragment.VoxMTL = voxMTL;
        fragment.LUV = luv+vec2(size.x, size.y);
        gl_Position = ubo.projview*tempVec;
        EmitVertex();

        break;
    case 1: // xy
        tempVec = baseVec;
        tempVec.x += size.x;
        tempVec = ubo.model*tempVec;
        fragment.GeoPos = vec3(tempVec);
        fragment.VoxMTL = voxMTL;
        fragment.LUV = luv+vec2(size.x, 0);
        gl_Position = ubo.projview*tempVec;
        EmitVertex();

        tempVec = baseVec;
        tempVec.y += size.y;
        tempVec = ubo.model*tempVec;
        fragment.GeoPos = vec3(tempVec);
        fragment.VoxMTL = voxMTL;
        fragment.LUV = luv+vec2(0, size.y);
        gl_Position = ubo.projview*tempVec;
        EmitVertex();

        tempVec = baseVec;
        tempVec.x += size.x;
        tempVec.y += size.y;
        tempVec = ubo.model*tempVec;
        fragment.GeoPos = vec3(tempVec);
        fragment.VoxMTL = voxMTL;
        fragment.LUV = luv+vec2(size.x, size.y);
        gl_Position = ubo.projview*tempVec;
        EmitVertex();

        break;
    case 2: // zy
        tempVec = baseVec;
        tempVec.z += size.x;
        tempVec = ubo.model*tempVec;
        fragment.GeoPos = vec3(tempVec);
        fragment.VoxMTL = voxMTL;
        fragment.LUV = luv+vec2(size.x, 0);
        gl_Position = ubo.projview*tempVec;
        EmitVertex();

        tempVec = baseVec;
        tempVec.y += size.y;
        tempVec = ubo.model*tempVec;
        fragment.GeoPos = vec3(tempVec);
        fragment.VoxMTL = voxMTL;
        fragment.LUV = luv+vec2(0, size.y);
        gl_Position = ubo.projview*tempVec;
        EmitVertex();

        tempVec = baseVec;
        tempVec.z += size.x;
        tempVec.y += size.y;
        tempVec = ubo.model*tempVec;
        fragment.GeoPos = vec3(tempVec);
        fragment.VoxMTL = voxMTL;
        fragment.LUV = luv+vec2(size.x, size.y);
        gl_Position = ubo.projview*tempVec;
        EmitVertex();

        break;
    case 3: // xz inv
        tempVec = baseVec;
        tempVec.z += size.y;
        tempVec = ubo.model*tempVec;
        fragment.GeoPos = vec3(tempVec);
        fragment.VoxMTL = voxMTL;
        fragment.LUV = luv+vec2(size.x, 0);
        gl_Position = ubo.projview*tempVec;
        EmitVertex();

        tempVec = baseVec;
        tempVec.x += size.x;
        tempVec = ubo.model*tempVec;
        fragment.GeoPos = vec3(tempVec);
        fragment.VoxMTL = voxMTL;
        fragment.LUV = luv+vec2(0, size.y);
        gl_Position = ubo.projview*tempVec;
        EmitVertex();

        tempVec = baseVec;
        tempVec.x += size.x;
        tempVec.z += size.y;
        tempVec = ubo.model*tempVec;
        fragment.GeoPos = vec3(tempVec);
        fragment.VoxMTL = voxMTL;
        fragment.LUV = luv+vec2(size.x, size.y);
        gl_Position = ubo.projview*tempVec;
        EmitVertex();

        break;
    case 4: // xy inv
        tempVec = baseVec;
        tempVec.y += size.y;
        tempVec = ubo.model*tempVec;
        fragment.GeoPos = vec3(tempVec);
        fragment.VoxMTL = voxMTL;
        fragment.LUV = luv+vec2(size.x, 0);
        gl_Position = ubo.projview*tempVec;
        EmitVertex();

        tempVec = baseVec;
        tempVec.x += size.x;
        tempVec = ubo.model*tempVec;
        fragment.GeoPos = vec3(tempVec);
        fragment.VoxMTL = voxMTL;
        fragment.LUV = luv+vec2(0, size.y);
        gl_Position = ubo.projview*tempVec;
        EmitVertex();

        tempVec = baseVec;
        tempVec.x += size.x;
        tempVec.y += size.y;
        tempVec = ubo.model*tempVec;
        fragment.GeoPos = vec3(tempVec);
        fragment.VoxMTL = voxMTL;
        fragment.LUV = luv+vec2(size.x, size.y);
        gl_Position = ubo.projview*tempVec;
        EmitVertex();

        break;
    case 5: // zy inv
        tempVec = baseVec;
        tempVec.y += size.y;
        tempVec = ubo.model*tempVec;
        fragment.GeoPos = vec3(tempVec);
        fragment.VoxMTL = voxMTL;
        fragment.LUV = luv+vec2(size.x, 0);
        gl_Position = ubo.projview*tempVec;
        EmitVertex();

        tempVec = baseVec;
        tempVec.z += size.x;
        tempVec = ubo.model*tempVec;
        fragment.GeoPos = vec3(tempVec);
        fragment.VoxMTL = voxMTL;
        fragment.LUV = luv+vec2(0, size.y);
        gl_Position = ubo.projview*tempVec;
        EmitVertex();

        tempVec = baseVec;
        tempVec.z += size.x;
        tempVec.y += size.y;
        tempVec = ubo.model*tempVec;
        fragment.GeoPos = vec3(tempVec);
        fragment.VoxMTL = voxMTL;
        fragment.LUV = luv+vec2(size.x, size.y);
        gl_Position = ubo.projview*tempVec;
        EmitVertex();

        break;
    default:
        break;
    }

    EndPrimitive();
}  
