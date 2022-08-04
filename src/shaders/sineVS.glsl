#version 430 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 texture_coordinate;

// for wave
const float PI = 3.14159;
const vec4 DIRECTION = vec4(1.0f, 1.0f, 0.0f, 0.0f);
uniform float amplitude;
uniform float wavelength;
uniform float time;


uniform mat4 u_model;

uniform vec3 cameraPosition;
uniform vec3 lightPosition;

layout (std140, binding = 0) uniform commom_matrices
{
    mat4 u_projection;
    mat4 u_view;
};

out V_OUT
{
   vec3 position;
   vec2 texture_coordinate;
   vec4 clipSpace;
} v_out;

vec3 GerstnerWave(vec4 wave, vec3 p, vec3 tangent)
{
    float k = 2 * PI / wave.w;
    float c = sqrt(9.8 / k);
    vec2 d = normalize(wave.xy);
    float f = k * (dot(d, p.xz) - c * time);
    float a = wave.z / k;

    return vec3(d.x * (a * cos(f)), a * sin(f), d.y * (a * cos(f)));
}



void main()
{
    //using for wave
    vec4 wave = vec4(DIRECTION.x, DIRECTION.y, amplitude, wavelength);
    vec3 tangent = vec3(1.0f, 0.0f, 0.0f);
    vec3 binormal = vec3(0.0f, 0.0f, 1.0f);

    vec3 sineWaveHeight =vec3(0.0f, GerstnerWave(wave, position, tangent).y,0.0f);
    v_out.position = vec3(position + sineWaveHeight);
    v_out.clipSpace = u_projection * u_view * u_model* vec4( v_out.position, 1.0f);
    v_out.texture_coordinate = vec2(v_out.position.x/2.0f+0.5f,v_out.position.z/2.0f+0.5f);
    
    gl_Position = v_out.clipSpace;

   
}