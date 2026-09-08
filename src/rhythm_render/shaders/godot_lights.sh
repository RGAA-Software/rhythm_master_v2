// Focused Godot 4.5.1 GLES omni/spot adaptation, MIT. See provenance/godot_lighting.json.
float GodotAttenuation(float distance, float inverse_range, float decay) {
    float nd = distance * inverse_range;
    nd *= nd;
    nd *= nd;
    nd = max(1.0 - nd, 0.0);
    nd *= nd;
    return nd * pow(max(distance, 0.0001), -decay);
}
float GodotSpot(vec3 toward_light, vec3 direction, float cone_cosine, float cone_decay) {
    float cosine = max(dot(-toward_light, direction), cone_cosine);
    float rim = max(0.0001, (1.0 - cosine) / (1.0 - cone_cosine));
    return 1.0 - pow(rim, cone_decay);
}
