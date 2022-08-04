#version 430 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texture_coordinate;

const float PI = 3.14159;
uniform mat4 u_model;
uniform mat4 new_view_matrix;
uniform commom_matrices
{
    mat4 u_projection;
    mat4 u_view;
};
uniform int clip_mode;

out V_OUT
{
   vec3 position;
   vec3 normal;
   vec2 texture_coordinate;
} v_out;

// function to rotate matrix in glsl
mat4 rotationMatrix(vec3 axis, float angle)
{
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;
    
    return mat4(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0,
                oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,  0.0,
                oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0,
                0.0,                                0.0,                                0.0,                                1.0);
}

mat4 translateMatrix(vec3 delta)
{
    return mat4(
        vec4(1.0, 0.0, 0.0, 0.0),
        vec4(0.0, 1.0, 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(delta, 1.0));
}

void main()
{
    /* clip_mode
        0: No clip
        1: Clip for reflection
        2: Clip for refraction
    */
    // reflection
    if(clip_mode==1){
        gl_ClipDistance[0] = position.y; 
        gl_Position = u_projection* u_view * u_model * vec4(position+vec3(0.0f, 0.0f, 0.0f), 1.0f);
    }
    // refraction 0.8 = 0.6+ max_height/2
    else if(clip_mode==2){
        gl_ClipDistance[0] = -position.y;
        gl_Position = u_projection * u_view * u_model * vec4(position, 1.0f);
    }
    else
    {
        //gl_ClipDistance[0] = position.y-0.6f; 
        gl_Position = u_projection * u_view * u_model * vec4(position, 1.0f);
    }

    v_out.position = vec3(u_model * vec4(position, 1.0f));
    v_out.normal = mat3(transpose(inverse(u_model))) * normal;
    v_out.texture_coordinate = vec2(texture_coordinate.x, 1.0f - texture_coordinate.y);
    
}