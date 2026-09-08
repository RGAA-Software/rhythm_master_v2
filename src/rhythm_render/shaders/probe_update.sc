#include <bgfx_compute.sh>
BUFFER_RW(s_probe_instances, vec4, 0);
uniform vec4 u_probe_phase;
NUM_THREADS(2, 1, 1)
void main()
{
    uint index = gl_GlobalInvocationID.x;
    if (index >= 2u) return;
    float green = mod(float(index) + u_probe_phase.x, 2.0);
    s_probe_instances[index] = vec4(float(index) - 0.5, 0.0, 1.0 - green, green);
}
