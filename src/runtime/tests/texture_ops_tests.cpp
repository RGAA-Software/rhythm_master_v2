#include <iostream>
#include <stdexcept>

#include "texture_ops.h"

int main() {
    using namespace rhythm;
    try {
        const auto require = [](bool condition) {
            if (!condition) throw std::runtime_error("texture_ops.contract");
        };
        graph::Registry registry;
        graph::Instruction instruction;
        instruction.node_ = registry.MakeNode(1, "texture.color_adjust");
        instruction.operation_ = graph::Operation::kColorAdjust;
        instruction.inputs_ = {0, 1, {}, {}, {}};
        instruction.node_.properties_["exposure"] = -3.0;
        std::array<runtime::NodeOutput, 2> outputs{};
        outputs[1].scalar_ = 2;
        const auto draw = [&] {
            render::DrawList list;
            list.width_ = list.height_ = 16;
            const auto clear = runtime::detail::DrawTexture(instruction, outputs, {}, {}, list);
            require(clear == 0 && list.commands_.size() == 1);
            return list;
        };
        require(draw().commands_[0].color_adjustment_->exposure_ == 2);
        outputs[1].scalar_ = 1e12;
        require(draw().commands_[0].color_adjustment_->exposure_ == 8);
        instruction.inputs_[1].reset();
        require(draw().commands_[0].color_adjustment_->exposure_ == -3);
        instruction.operation_ = graph::Operation::kTime;
        bool rejected = false;
        try {
            draw();
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected);
        instruction.node_ = registry.MakeNode(1, "texture.noise");
        instruction.operation_ = graph::Operation::kTextureNoise;
        instruction.inputs_ = {1};
        outputs[1].scalar_ = 5000;
        require(draw().commands_[0].texture_noise_->phase_ == 4096);
        instruction.inputs_[0].reset();
        require(draw().commands_[0].texture_noise_->phase_ == 0);
        instruction.node_ = registry.MakeNode(1, "texture.mapping");
        instruction.operation_ = graph::Operation::kTextureMapping;
        instruction.inputs_ = {0, {}, {}, {}, 1};
        outputs[1].scalar_ = -1;
        require(draw().commands_[0].texture_mapping_->scale_ == 0.125f);
        instruction.inputs_[4].reset();
        require(draw().commands_[0].texture_mapping_->scale_ == 1);
        instruction.node_ = registry.MakeNode(1, "texture.contours");
        instruction.operation_ = graph::Operation::kTextureContours;
        instruction.inputs_ = {0, 1};
        outputs[1].scalar_ = -5000;
        require(draw().commands_[0].texture_contours_->phase_ == -4096);
        instruction.inputs_[1].reset();
        require(draw().commands_[0].texture_contours_->phase_ == 0);
        std::cout << "Texture commands passed: color binding, fallback, clamp and "
                     "unsupported-operation rejection\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
