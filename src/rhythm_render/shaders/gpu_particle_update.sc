// Adapted from TiXL ParticleSystem.hlsl (MIT); provenance/tixl_particles.json.
// Copyright notices: third_party/notices/tixl-effects/LICENSE.txt.
#include <bgfx_compute.sh>
BUFFER_RW(s_gpu_particles, vec4, 0);
uniform vec4 u_gpu_step0;
uniform vec4 u_gpu_step1;
uniform vec4 u_gpu_emit;
uniform vec4 u_gpu_dynamics;
uniform vec4 u_gpu_gravity;
uniform vec4 u_gpu_color_a;
uniform vec4 u_gpu_color_b;
float Random(inout uint state)
{
    state = state * 1664525u + 1013904223u;
    return float(state >> 8u) / 16777216.0;
}
NUM_THREADS(64, 1, 1)
void main()
{
    uint index = gl_GlobalInvocationID.x;
    uint capacity = uint(u_gpu_step1.x);
    if (index >= capacity) return;
    uint base = index * 4u;
    vec4 position = vec4(0.0, 0.0, 0.0, -1.0);
    vec4 velocity = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 color = vec4(0.0, 0.0, 0.0, 0.0);
    vec4 shape = vec4(0.0, 0.0, 0.0, 0.0);
    if (u_gpu_step0.y < 0.5)
    {
        position = s_gpu_particles[base];
        velocity = s_gpu_particles[base + 1u];
        color = s_gpu_particles[base + 2u];
        shape = s_gpu_particles[base + 3u];
    }
    uint insertion = (index + capacity - uint(u_gpu_step0.z)) % capacity;
    if (insertion < uint(u_gpu_step0.w))
    {
        uint state = (index + 1u) * 747796405u + uint(u_gpu_step1.y) + uint(u_gpu_step1.z) * 2891336453u;
        float angle = Random(state) * 6.28318530718;
        float radius = sqrt(Random(state)) * u_gpu_emit.z;
        vec2 direction = vec2(cos(angle), sin(angle));
        position = vec4(u_gpu_emit.xy + direction * radius, u_gpu_gravity.w, 0.0);
        velocity = vec4(direction * u_gpu_emit.w, 0.0, u_gpu_dynamics.y);
        color = mix(u_gpu_color_a, u_gpu_color_b, Random(state));
        shape = vec4(u_gpu_dynamics.z * mix(0.5, 1.0, Random(state)), angle, 0.0, 0.0);
    }
    float dt = u_gpu_step0.x;
    if (position.w >= 0.0 && dt > 0.0)
    {
        // TiXL's time-based drag and ring lifetime, with explicit inactive age.
        velocity.xyz *= pow(max(0.0, 1.0 - u_gpu_dynamics.x), dt);
        velocity.xy += u_gpu_gravity.xy * dt;
        vec2 p = (position.xy - u_gpu_emit.xy) * u_gpu_gravity.z;
        float phase = u_gpu_step1.w;
        // Curl of a bounded analytic scalar potential: divergence-free advection.
        vec2 flow = vec2(sin(p.x + phase) * cos(p.y - phase), -cos(p.x + phase) * sin(p.y - phase));
        position.xy += (velocity.xy + flow * u_gpu_dynamics.w) * dt;
        position.z += velocity.z * dt;
        position.w += dt;
        if (position.w >= velocity.w) position.w = -1.0;
    }
    s_gpu_particles[base] = position;
    s_gpu_particles[base + 1u] = velocity;
    s_gpu_particles[base + 2u] = color;
    s_gpu_particles[base + 3u] = shape;
}
