/*

Expected globals:
material
light_accumulator_*

*/

struct DirectionalLight {
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    vec3 direction;
};

void calculateLight(in DirectionalLight light, in vec3 eyeToPoint, in vec3 normal) {

    light_accumulator_ambient += light.ambient;

    float nDotVP = clamp(dot(normal, -light.direction), 0.0, 1.0);

    #ifdef TANGRAM_MATERIAL_DIFFUSE
        light_accumulator_diffuse += light.diffuse * nDotVP;
    #endif

    #ifdef TANGRAM_MATERIAL_SPECULAR
        float pf = 0.0;
        if (nDotVP > 0.0) {
            vec3 reflectVector = reflect(light.direction, normal);
            float eyeDotR = max(dot(normalize(eyeToPoint), reflectVector), 0.0);
            pf = pow(eyeDotR, material.shininess);
        }
        light_accumulator_specular += light.specular * pf;
    #endif
}
