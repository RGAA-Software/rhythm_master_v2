// P6.2 probe adapted from TiXL TransformPoints.hlsl (MIT).
// provenance/tixl_point_attributes.json; third_party/notices/tixl-effects/LICENSE.txt.
#include <bgfx_compute.sh>
BUFFER_RO(s_attribute_source, vec4, 0);
BUFFER_RW(s_attribute_result, vec4, 1);
uniform mat4 u_attribute_transform;
uniform vec4 u_attribute_info;
uniform vec4 u_attribute_color;
NUM_THREADS(64, 1, 1)
void main()
{
    uint index = gl_GlobalInvocationID.x;
    if (index >= uint(u_attribute_info.x)) return;
    uint base = index * 4u;
    vec4 position = s_attribute_source[base];
    vec4 velocity = s_attribute_source[base + 1u];
    vec4 color = s_attribute_source[base + 2u];
    vec4 shape = s_attribute_source[base + 3u];
    position.xyz = mul(u_attribute_transform, vec4(position.xyz, 1.0)).xyz;
    color *= u_attribute_color;
    shape.x *= u_attribute_info.y;
    s_attribute_result[base] = position;
    s_attribute_result[base + 1u] = velocity;
    s_attribute_result[base + 2u] = color;
    s_attribute_result[base + 3u] = shape;
}
