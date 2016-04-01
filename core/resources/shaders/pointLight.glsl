/*

Expected globals:
material
light_accumulator_*

*/

struct PointLight {
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    vec4 position;

#ifdef TANGRAM_POINTLIGHT_ATTENUATION_EXPONENT
    float attenuationExponent;
#endif

#ifdef TANGRAM_POINTLIGHT_ATTENUATION_INNER_RADIUS
    float innerRadius;
#endif

#ifdef TANGRAM_POINTLIGHT_ATTENUATION_OUTER_RADIUS
    float outerRadius;
#endif
};

void calculateLight(in PointLight light, in vec3 eyeToPoint, in vec3 normal) {

    float dist = length(light.position.xyz - eyeToPoint);

    // Compute vector from surface to light position
    vec3 VP = (light.position.xyz - eyeToPoint) / dist;

    // Normalize the vector from surface to light position
    float nDotVP = clamp(dot(VP, normal), 0.0, 1.0);

    // Attenuation defaults
    float attenuation = 1.0;
    #ifdef TANGRAM_POINTLIGHT_ATTENUATION_EXPONENT
        float Rin = 1.0;
        float e = light.attenuationExponent;

        #ifdef TANGRAM_POINTLIGHT_ATTENUATION_INNER_RADIUS
            Rin = light.innerRadius;
        #endif

        #ifdef TANGRAM_POINTLIGHT_ATTENUATION_OUTER_RADIUS
            float Rdiff = light.outerRadius-Rin;
            float d = clamp(max(0.0,dist-Rin)/Rdiff, 0.0, 1.0);
            attenuation = 1.0-(pow(d,e));
        #else
            // If no outer is provide behaves like:
            // https://imdoingitwrong.wordpress.com/2011/01/31/light-attenuation/
            float d = max(0.0,dist-Rin)/Rin+1.0;
            attenuation = clamp(1.0/(pow(d,e)), 0.0, 1.0);
        #endif
    #else
        float Rin = 0.0;

        #ifdef TANGRAM_POINTLIGHT_ATTENUATION_INNER_RADIUS
            Rin = light.innerRadius;
            #ifdef TANGRAM_POINTLIGHT_ATTENUATION_OUTER_RADIUS
                float Rdiff = light.outerRadius-Rin;
                float d = clamp(max(0.0,dist-Rin)/Rdiff, 0.0, 1.0);
                attenuation = 1.0-d*d;
            #else
                // If no outer is provide behaves like:
                // https://imdoingitwrong.wordpress.com/2011/01/31/light-attenuation/
                float d = max(0.0,dist-Rin)/Rin+1.0;
                attenuation = clamp(1.0/d, 0.0, 1.0);
            #endif
        #else
            #ifdef TANGRAM_POINTLIGHT_ATTENUATION_OUTER_RADIUS
                float d = clamp(dist/light.outerRadius, 0.0, 1.0);
                attenuation = 1.0-d*d;
            #else
                attenuation = 1.0;
            #endif
        #endif
    #endif

    // Computer accumulators
    light_accumulator_ambient += light.ambient * attenuation;

    #ifdef TANGRAM_MATERIAL_DIFFUSE
        light_accumulator_diffuse += light.diffuse * nDotVP * attenuation;
    #endif

    #ifdef TANGRAM_MATERIAL_SPECULAR
        float pf = 0.0; // power factor for shiny speculars
        if (nDotVP > 0.0) {
            vec3 reflectVector = reflect(-VP, normal);
            float eyeDotR = max(0.0, dot(-normalize(eyeToPoint), reflectVector));
            pf = pow(eyeDotR, material.shininess);
        }

        light_accumulator_specular += light.specular * pf * attenuation;
    #endif
}
