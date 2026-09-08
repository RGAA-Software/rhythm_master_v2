// Four-influence linear blend skinning. Godot glTF supplies the palette;
// TiXL SkinMeshVertices is the source reference for weighted transforms.
// Project convention is column vectors. Use inverse-transpose of the blended
// basis for normals; upstream's direct direction transform assumes rigid scale.
uniform mat4 u_skin_bones[48];
float SkinMesh(inout vec3 position, inout vec3 normal, inout vec3 tangent, vec4 indices, vec4 weights)
{
    // UNORM8 is portable through float attributes on both D3D11 and GLES.
    indices = floor(indices * 255.0 + 0.5);
    mat4 skin = u_skin_bones[int(indices.x)] * weights.x
              + u_skin_bones[int(indices.y)] * weights.y
              + u_skin_bones[int(indices.z)] * weights.z
              + u_skin_bones[int(indices.w)] * weights.w;
    vec3 x = mul(skin, vec4(1.0, 0.0, 0.0, 0.0)).xyz;
    vec3 y = mul(skin, vec4(0.0, 1.0, 0.0, 0.0)).xyz;
    vec3 z = mul(skin, vec4(0.0, 0.0, 1.0, 0.0)).xyz;
    float determinant = dot(x, cross(y, z));
    position = mul(skin, vec4(position, 1.0)).xyz;
    tangent = mul(skin, vec4(tangent, 0.0)).xyz;
    if (abs(determinant) > 0.000001)
        normal = normalize((cross(y, z) * normal.x + cross(z, x) * normal.y + cross(x, y) * normal.z) / determinant);
    return determinant < 0.0 ? -1.0 : 1.0;
}
