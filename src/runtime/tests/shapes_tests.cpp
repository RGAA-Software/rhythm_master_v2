#include <iostream>
#include <numbers>
#include <stdexcept>

#include "shapes.h"

int main() {
    using namespace rhythm;
    try {
        const auto require = [](bool value) {
            if (!value) throw std::runtime_error("shape.geometry");
        };
        graph::Registry registry;
        auto node = registry.MakeNode(1, "texture.shape");
        node.properties_["shape_width"] = 1.0;
        node.properties_["shape_height"] = 1.0;
        const auto draw = [&] {
            render::DrawList list;
            list.width_ = 200;
            list.height_ = 100;
            runtime::detail::DrawShape(node, {}, list);
            return list;
        };
        for (const auto type : {0.0, 1.0, 2.0, 3.0}) {
            node.properties_["shape_type"] = type;
            const auto list = draw();
            require(list.commands_.size() == 1);
            double area = 0;
            for (const auto& vertex : list.vertices_)
                require(std::isfinite(vertex.x_) && std::isfinite(vertex.y_) && vertex.x_ >= 0 &&
                        vertex.x_ <= 200 && vertex.y_ >= 0 && vertex.y_ <= 100);
            for (std::size_t index = 0; index < list.indices_.size(); index += 3) {
                const auto& a = list.vertices_.at(list.indices_[index]);
                const auto& b = list.vertices_.at(list.indices_[index + 1]);
                const auto& c = list.vertices_.at(list.indices_[index + 2]);
                area += std::abs((b.x_ - a.x_) * (c.y_ - a.y_) - (c.x_ - a.x_) * (b.y_ - a.y_)) *
                        0.5;
            }
            const auto expected =
                    type == 0   ? 20000.0
                    : type == 3 ? 3 * 5000 * std::sin(std::numbers::pi / 3)
                                : std::numbers::pi * 5000 * (type == 2 ? 1 - 0.75 * 0.75 : 1);
            require(std::abs(area - expected) / expected < 0.001);
        }
        node.properties_["shape_type"] = 2.0;
        node.properties_["inner_ratio"] = 1.0;
        require(draw().commands_.empty());
        node.properties_["shape_type"] = 0.0;
        node.properties_["shape_width"] = 0.0;
        require(draw().commands_.empty());
        std::cout << "Shapes passed: rectangle, ellipse, ring, polygon areas and bounds, empty "
                     "shapes\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
