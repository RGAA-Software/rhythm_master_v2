// TiXL mesh-Deform taper/twist adapted to bounded vertex-local modifiers.
// MIT source and notices: provenance/mesh_deformation.json.
// Normals use the analytic inverse transpose; tangents use the Jacobian.
uniform vec4 u_scene_deform[4];
uniform vec4 u_scene_deform_pivot[4];

vec3 DeformNormalize(vec3 value)
{
    value /= max(max(abs(value.x), abs(value.y)), max(abs(value.z), 0.000001));
    return value / max(length(value), 0.000001);
}

void DeformMesh(inout vec3 position, inout vec3 normal, inout vec3 tangent)
{
    for (int i = 0; i < 4; ++i) {
        vec4 parameters = u_scene_deform[i];
        if (parameters.w > 0.5) {
            vec3 axis = parameters.z < 0.5 ? vec3(1.0, 0.0, 0.0)
                : parameters.z < 1.5 ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0);
            vec3 pivot = u_scene_deform_pivot[i].xyz;
            vec3 q = position - pivot;
            float along = dot(q, axis);
            vec3 radial = q - axis * along;
            float angle = along * parameters.x;
            float c = cos(angle), s = sin(angle);
            vec3 rotated = radial * c + cross(axis, radial) * s;
            float raw_scale = 1.0 - parameters.y * along;
            float scale = clamp(raw_scale, 0.05, 20.0);
            float slope = raw_scale > 0.05 && raw_scale < 20.0 ? -parameters.y : 0.0;
            vec3 gradient = slope * rotated + scale * parameters.x * cross(axis, rotated);
            float n_axis = dot(normal, axis);
            vec3 n_radial = normal - axis * n_axis;
            vec3 n_rotated = (n_radial * c + cross(axis, n_radial) * s) / scale;
            normal = DeformNormalize(n_rotated + axis * (n_axis - dot(gradient, n_rotated)));
            float t_axis = dot(tangent, axis);
            vec3 t_radial = tangent - axis * t_axis;
            tangent = DeformNormalize(scale * (t_radial * c + cross(axis, t_radial) * s)
                                     + (axis + gradient) * t_axis);
            position = pivot + axis * along + rotated * scale;
        }
    }
}
