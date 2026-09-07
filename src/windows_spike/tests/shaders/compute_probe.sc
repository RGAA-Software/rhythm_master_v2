#include <bgfx_compute.sh>

IMAGE2D_WO(s_output, rgba8, 0);
NUM_THREADS(8, 8, 1)
void main()
{
    imageStore(s_output, ivec2(gl_GlobalInvocationID.xy), vec4(0.25, 0.5, 0.75, 1.0));
}
