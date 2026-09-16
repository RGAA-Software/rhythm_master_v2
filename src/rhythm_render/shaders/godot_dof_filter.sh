// Adapted from Godot 4.5.1 bokeh_dof_raster.glsl, MIT.
// See provenance/depth_pipeline.json. Color is premultiplied; signed CoC is separate.
void DofFilter(vec2 uv, vec4 vertex_color, out vec4 output_color, out float output_weight) {
    vec4 center = texture2D(s_tex, uv);
    float original_alpha = center.a;
    center.a = texture2D(s_displace_map, uv).r;
    vec4 accum = center;
    float total = 1.0;
    float shape = u_dof_filter.x;
    bool second_pass = u_dof_filter.y > 0.5;
    float blur_size = u_dof_filter.z;
    if (shape < 0.5) {
        float blur_scale = u_dof_filter.w;
        float radius = blur_scale;
        for (int i = 0; i < 1024; ++i) {
            if (radius >= blur_size) break;
            float angle = float(i) * 2.39996323;
            vec2 sample_uv = uv + vec2(cos(angle), sin(angle)) * u_dof_domain.xy * radius;
            vec4 sample_color = texture2D(s_tex, sample_uv);
            sample_color.a = texture2D(s_displace_map, sample_uv).r;
            float limit = abs(sample_color.a);
            if (sample_color.a > center.a) limit = min(limit, abs(center.a) * 2.0);
            float weight = smoothstep(radius - 0.5, radius + 0.5, limit);
            accum += mix(accum / total, sample_color, weight);
            total += 1.0;
            radius += blur_scale / radius;
        }
    } else {
        vec2 direction = shape < 1.5
                                 ? (second_pass ? vec2(0.0, 1.0) : vec2(1.0, 0.0))
                                 : (second_pass ? normalize(vec2(1.0, 0.577350269189626))
                                                : vec2(0.0, 1.0));
        direction *= u_dof_domain.xy;
        float steps = u_dof_filter.w;
        float blur_scale = blur_size / steps;
        for (int i = -24; i <= 24; ++i) {
            if (i == 0 || abs(float(i)) > steps) continue;
            float radius = abs(float(i) * blur_scale);
            vec2 sample_uv = uv + direction * float(i) * blur_scale;
            vec4 sample_color = texture2D(s_tex, sample_uv);
            sample_color.a = texture2D(s_displace_map, sample_uv).r;
            float limit = sample_color.a < center.a ? abs(sample_color.a) : abs(center.a);
            float weight = smoothstep(radius - 0.5, radius + 0.5, limit);
            accum += mix(center, sample_color, weight);
            total += 1.0;
        }
        accum /= total;
        if (shape > 1.5 && second_pass) {
            direction = normalize(vec2(-1.0, 0.577350269189626)) * u_dof_domain.xy;
            vec4 second_accum = center;
            total = 1.0;
            for (int i = -24; i <= 24; ++i) {
                if (i == 0 || abs(float(i)) > steps) continue;
                float radius = abs(float(i) * blur_scale);
                vec2 sample_uv = uv + direction * float(i) * blur_scale;
                vec4 sample_color = texture2D(s_tex, sample_uv);
                sample_color.a = texture2D(s_displace_map, sample_uv).r;
                float limit = sample_color.a < center.a ? abs(sample_color.a) : abs(center.a);
                float weight = smoothstep(radius - 0.5, radius + 0.5, limit);
                second_accum += mix(center, sample_color, weight);
                total += 1.0;
            }
            second_accum /= total;
            accum.rgb = min(accum.rgb, second_accum.rgb);
            accum.a = (accum.a + second_accum.a) * 0.5;
        }
    }
    if (shape < 0.5) accum /= total;
    output_color = vec4(accum.rgb, original_alpha) * vertex_color;
    output_weight = accum.a;
}
