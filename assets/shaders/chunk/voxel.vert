#version 450

layout(location = 0) in uvec3 Vertex;
layout(location = 0) out uvec3 Geometry;


void main()
{
    Geometry = Vertex;
}
