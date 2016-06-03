#version 300 es
#include phong_functions_fsh

uniform highp vec3 camera_position;
uniform lowp vec3 ambient_light_color;
uniform lowp vec4 material_diffuse_surface_color;
uniform lowp float material_diffuse_intensity;
uniform lowp float material_alpha;
uniform lowp float material_shininess;

uniform VROSceneLightingUniforms lighting;
uniform sampler2D specular_texture;
uniform samplerCube reflect_texture;

in lowp vec3 v_normal;
in highp vec2 v_texcoord;
in highp vec3 v_surface_position;

out lowp vec4 frag_color;

void main() {
    VROPhongLighting phong;
    phong.normal = v_normal;
    phong.texcoord = v_texcoord;
    phong.surface_position = v_surface_position;
    phong.camera_position = camera_position;
    
    phong.ambient_color = ambient_light_color * material_diffuse_surface_color.xyz;
    phong.material_color = material_diffuse_surface_color;
    phong.material_shininess = material_shininess;
    phong.diffuse_intensity = material_diffuse_intensity;
    phong.material_alpha = material_alpha;
    
    lowp vec4 reflective_color = compute_reflection(v_surface_position, camera_position, v_normal, reflect_texture);
    lowp vec4 lighting_color = phong_lighting_diffuse_fixed(phong, specular_texture, lighting);
    
    frag_color = vec4(lighting_color.xyz + reflective_color.xyz, lighting_color.a);
}