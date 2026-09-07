$input v_color0, v_texcoord0
#include <bgfx_shader.sh>
SAMPLER2D(s_tex, 0);
uniform vec4 u_contours_settings; // count, half width, phase
uniform vec4 u_contours_color_a;
uniform vec4 u_contours_color_b;

void main()
{
    vec4 source = texture2D(s_tex, v_texcoord0);
    vec3 straight = source.rgb / max(source.a, 0.000001);
    float level = dot(straight, vec3(0.2126, 0.7152, 0.0722));
    float band = level * u_contours_settings.x + u_contours_settings.z;
    float distance_to_line = abs(fract(band + 0.5) - 0.5);
    float footprint = max(fwidth(band), 0.0001);
    float coverage = 1.0 - smoothstep(u_contours_settings.y - footprint,
                                    u_contours_settings.y + footprint, distance_to_line);
    // Unresolved fine lines converge to their average coverage, reducing shimmer.
    coverage = mix(coverage, 2.0 * u_contours_settings.y, smoothstep(0.35, 1.0, footprint));
    vec4 first = vec4(u_contours_color_a.rgb * u_contours_color_a.a, u_contours_color_a.a);
    vec4 second = vec4(u_contours_color_b.rgb * u_contours_color_b.a, u_contours_color_b.a);
    gl_FragColor = mix(first, second, clamp(level, 0.0, 1.0)) * coverage * source.a * v_color0;
}
