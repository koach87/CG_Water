#version 430 core
out vec4 f_color;

in V_OUT
{
   vec3 position;
   vec3 normal;
   vec2 texture_coordinate;
} f_in;

uniform vec3 u_color;

uniform int clip_mode;
uniform sampler2D u_texture;

void main()
{   
    vec3 color = vec3(texture(u_texture, f_in.texture_coordinate));
    if(clip_mode==1)
        f_color = vec4(color, 0.5f);
    else
         f_color = vec4(color, 1.0f);

   
    //f_color = vec4(normalize(f_in.position),0.5f);
}