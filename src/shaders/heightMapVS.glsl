#version 430 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec2 texture_coordinate;
layout (location = 2) in vec3 normal;

uniform mat4 u_model;
uniform sampler2D u_height;

const float WAVE_MAX_HEIGHT = 0.5f;


layout (std140, binding = 0) uniform commom_matrices
{
    mat4 u_projection;
    mat4 u_view;
};

out V_OUT
{
   vec3 position;
   vec3 normal;
   vec2 texture_coordinate;
   vec4 clipSpace;
} v_out;

void main()
{
    //�Τ���normal�A�bfragment shader�p��N�n�C
    //v_out.normal = mat3(transpose(inverse(u_model))) * normal;
    v_out.texture_coordinate = vec2(texture_coordinate.x, texture_coordinate.y);

    vec3 color = vec3(texture(u_height, v_out.texture_coordinate));

    // �N��m���U�ǡA�H�DdFdx��dFdy�C
    v_out.position = vec3(position+vec3(0.0f, color.x*WAVE_MAX_HEIGHT-WAVE_MAX_HEIGHT/2.0f, 0.0f));
    v_out.clipSpace =  u_projection * u_view * u_model * vec4(position+vec3(0.0f, color.x*WAVE_MAX_HEIGHT-WAVE_MAX_HEIGHT/2.0f, 0.0f), 1.0f);
    //�]���w�g�O�Ƕ��F�A�]��rgb���O�@�˪��ȡA��������@�Y�i�C
    gl_Position = v_out.clipSpace;

}