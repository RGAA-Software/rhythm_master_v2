#include <iostream>
#include <stdexcept>

#include "affine.h"

int main() {
    using namespace rhythm;
    try {
        const auto require = [](bool condition) {
            if (!condition) throw std::runtime_error("affine.geometry");
        };
        const auto near = [](float actual, float expected) {
            return std::abs(actual - expected) < 0.001f;
        };
        graph::Registry registry;
        graph::Instruction instruction;
        instruction.node_ = registry.MakeNode(1, "texture.affine");
        instruction.inputs_ = {0, {}, {}, {}, {}, {}};
        std::array<runtime::NodeOutput, 2> outputs{};
        const auto draw = [&] {
            render::DrawList list;
            list.width_ = 200;
            list.height_ = 100;
            runtime::detail::DrawAffine(instruction, outputs, list);
            return list;
        };
        auto list = draw();
        require(list.vertices_.size() == 4 && near(list.vertices_[0].x_, 0) &&
                near(list.vertices_[2].y_, 100));
        instruction.node_.properties_["rotation"] = 90.0;
        list = draw();
        require(near(list.vertices_[0].x_, 150) && near(list.vertices_[0].y_, -50) &&
                near(list.vertices_[2].x_, 50) && near(list.vertices_[2].y_, 150));
        instruction.node_.properties_["rotation"] = 0.0;
        instruction.node_.properties_["scale_x"] = -1.0;
        list = draw();
        require(near(list.vertices_[0].x_, 200) && near(list.vertices_[1].x_, 0));
        instruction.node_.properties_["scale_x"] = 1.0;
        instruction.inputs_[1] = 1;
        outputs[1].scalar_ = 0.5;
        list = draw();
        require(near(list.vertices_[0].x_, 50) && near(list.vertices_[0].y_, 25));
        instruction.inputs_[1].reset();
        instruction.inputs_[3] = 1;
        list = draw();
        require(near(list.vertices_[0].x_, 100));
        instruction.inputs_[3].reset();
        instruction.inputs_[5] = 1;
        list = draw();
        require((list.vertices_[0].color_ >> 24) == 128);
        outputs[1].scalar_ = -2;
        require(draw().commands_.empty());
        outputs[1].scalar_ = 2;
        require((draw().vertices_[0].color_ >> 24) == 255);
        instruction.inputs_[5].reset();
        instruction.node_.properties_["pivot_x"] = 0.0;
        instruction.node_.properties_["pivot_y"] = 0.0;
        instruction.node_.properties_["scale"] = 0.5;
        list = draw();
        require(near(list.vertices_[0].x_, 0) && near(list.vertices_[2].x_, 100) &&
                near(list.vertices_[2].y_, 50));
        std::cout << "Affine passed: identity, rotation, mirror, scalar binding, pivot and opacity "
                     "bounds\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
