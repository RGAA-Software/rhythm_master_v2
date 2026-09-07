$input v_color0, v_texcoord0
#include <bgfx_shader.sh>
// Project temporal envelope; runtime supplies elapsed-time retention and transforms.
SAMPLER2D(s_tex, 0);
SAMPLER2D(s_displace_map, 1);
uniform vec4 u_trail_settings; // retention, scale, rotation degrees, aspect
void main()
{
    vec2 p = (v_texcoord0 - 0.5) * vec2(u_trail_settings.w, 1.0);
    float angle = radians(-u_trail_settings.z);
    p = vec2(cos(angle) * p.x - sin(angle) * p.y,
             sin(angle) * p.x + cos(angle) * p.y) / u_trail_settings.y;
    vec2 uv = p / vec2(u_trail_settings.w, 1.0) + 0.5;
    float coverage = step(0.0, uv.x) * step(uv.x, 1.0) * step(0.0, uv.y) * step(uv.y, 1.0);
    vec4 history = texture2D(s_displace_map, clamp(uv, 0.0, 1.0)) *
                   (u_trail_settings.x * coverage);
    gl_FragColor = max(texture2D(s_tex, v_texcoord0), history) * v_color0;
}
