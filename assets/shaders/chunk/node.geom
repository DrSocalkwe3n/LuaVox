#version 450

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

layout(location = 0) in GeometryObj {
    vec3 GeoPos;    // Реальная позиция в мире
    uint Texture;   // Текстура
    vec2 UV;
} Geometry[];

layout(location = 0) out FragmentObj {
    vec3 GeoPos;    // Реальная позиция в мире
    uint Texture;   // Текстура
    vec2 UV;
} Fragment;

void main() {
    for(int iter = 0; iter < 3; iter++) {
        gl_Position = gl_in[iter].gl_Position;
        Fragment.GeoPos = Geometry[iter].GeoPos;
        Fragment.Texture = Geometry[iter].Texture;
        Fragment.UV = Geometry[iter].UV;
        EmitVertex();
    }

    EndPrimitive();
}  
