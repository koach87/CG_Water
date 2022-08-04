#version 430 core
out vec4 f_color;

uniform mat4 u_model;
in V_OUT
{ 
   vec3 position;
   vec3 normal;
   vec2 texture_coordinate;
   vec4 clipSpace;
} f_in;

uniform vec3 u_color;

uniform sampler2D u_height;
uniform sampler2D refractionTexture;
uniform sampler2D reflectionTexture;
uniform vec3 cameraPos;

void main()
{   
    vec2 ndc = (f_in.clipSpace.xy/f_in.clipSpace.w)/2.0f +0.5f;
    vec2 reflectTexCoords = vec2(ndc.x, ndc.y);
    vec2 refractTexCoords = vec2(ndc.x, ndc.y);

    vec3 normal = normalize(cross(dFdx(f_in.position),dFdy(f_in.position)));
    vec3 toCam = normalize(f_in.position-cameraPos);
    float dis = distance(normal, toCam)*0.02f;

    
    vec4 reflectionColor = texture(reflectionTexture, reflectTexCoords);
    vec4 refractionColor = texture(refractionTexture, refractTexCoords +dis);

    
    
    const vec4 WATER_COLOR = vec4(0.83f, 0.94f, 0.97f, 1.0f);

    float lightIndensity = dot(normal,vec3(0.0f, 1.0f, 0.0f));

    f_color = mix(refractionColor,reflectionColor, 0.5f);
    f_color = mix(f_color, WATER_COLOR,0.2f);
    f_color = f_color*lightIndensity;
    
}