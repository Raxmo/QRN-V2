#version 450 core
out vec2 uv;
uniform float aspectratio = 0.75;

void main()
{
    vec2 positions[3] = vec2[](
        vec2(-1.0,  3.0),   // top
        vec2( 3.0, -1.0),   // bottom left
        vec2(-1.0, -1.0)    // bottom right
    );
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
    uv = positions[gl_VertexID] * vec2(min(1.0, 1.0/aspectratio), min(1.0, aspectratio));
}