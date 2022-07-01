#version 430 core

// �ڪ���l
layout(location = 0) in vec3 aPos;

// �K��
out vec3 TexCoords;


// ���ƣx
uniform mat4 projection;
uniform mat4 view;

void main()
{
    TexCoords = aPos;
    vec4 pos = projection * view * vec4(aPos, 1.0);
    gl_Position = pos.xyww;
}