#if (defined(TANGRAM_VERTEX_SHADER) && defined(TANGRAM_LIGHTING_VERTEX)) || (defined(TANGRAM_FRAGMENT_SHADER) && defined(TANGRAM_LIGHTING_FRAGMENT))
vec4 calculateLighting(in vec3 eyeToPoint, in vec3 normal, in vec4 color) {

    // Do initial material calculations over normal, emission, ambient, diffuse and specular values
    calculateMaterial(eyeToPoint, normal);
   

    // Unroll the loop of individual lights to calculate
    #pragma tangram: lights_to_compute

    //  Final light intensity calculation
    vec4 result = vec4(0.0);

    #ifdef TANGRAM_MATERIAL_EMISSION
        result = material.emission;
    #endif

    #ifdef TANGRAM_MATERIAL_AMBIENT
        result += light_accumulator_ambient * color * material.ambient;
    #else
        #ifdef TANGRAM_MATERIAL_DIFFUSE
            result += light_accumulator_ambient * color * material.diffuse;
        #endif
    #endif

    #ifdef TANGRAM_MATERIAL_DIFFUSE
        result += light_accumulator_diffuse * color * material.diffuse;
    #endif

    #ifdef TANGRAM_MATERIAL_SPECULAR
        result += light_accumulator_specular * material.specular;
    #endif

    // Clamp final result
    result = clamp(result, 0.0, 1.0);

    return result;
}
#endif
