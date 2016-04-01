/*

Expected globals:
light_accumulator_*

*/

struct AmbientLight {
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
};

void calculateLight(in AmbientLight light, in vec3 eyeToPoint, in vec3 normal) {
    light_accumulator_ambient += light.ambient;
}
