#version 310 es
precision highp float;

layout(std140, set = 0, binding = 0) uniform UBO
{
   float time;
} constants;

layout(location = 0) in vec3 vNormal;
layout(location = 0) out vec4 FragColor;

void main()
{
   float c = normalize(vNormal).z;
   c = (1.0 - cos(c * c)) / 3.0;
   FragColor = vec4(1.0, 1.0, 1.0, c);
}
