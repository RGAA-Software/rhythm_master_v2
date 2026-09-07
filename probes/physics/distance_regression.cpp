#include <box2d/collision.h>

#include <array>
#include <iostream>
#include <stdexcept>

// Native API regression stays inside this isolated dependency validation target.
void CheckNativeDistance() {
    const std::array<b2Vec2, 4> vertices{
            {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.5f, 0.5f}, {-0.5f, 0.5f}}};
    const b2Vec2 center{};
    b2DistanceInput input{};
    input.proxyA = b2MakeProxy(vertices.data(), 4, 0);
    input.proxyB = b2MakeProxy(&center, 1, 0.2f);
    input.transformA = b2Transform_identity;
    input.transformB = b2Transform_identity;
    input.useRadii = true;
    for (const float x : {-0.5001f, -0.50001f, -0.500002f, -0.5f, -0.4999f}) {
        input.transformB.p.x = x;
        b2SimplexCache cache{};
        const auto result = b2ShapeDistance(&input, &cache, nullptr, 0);
        std::cout << "native overlap x=" << x << " distance=" << result.distance << '\n';
        if (result.distance != 0) throw std::runtime_error("native rounded overlap distance");
    }
    input.transformB.p.x = -0.8f;
    b2SimplexCache cache{};
    const auto separated = b2ShapeDistance(&input, &cache, nullptr, 0);
    if (separated.distance < 0.099f || separated.distance > 0.101f)
        throw std::runtime_error("native separated distance");
}
