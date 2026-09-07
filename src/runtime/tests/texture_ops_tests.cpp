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
        const auto draw = [&](std::size_t expected_commands = 1) {
            render::DrawList list;
            list.width_ = list.height_ = 16;
            const auto clear = runtime::detail::DrawTexture(instruction, outputs, {}, {}, list);
            require(clear == 0 && list.commands_.size() == expected_commands);
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
        instruction.node_ = registry.MakeNode(1, "texture.displace");
        instruction.operation_ = graph::Operation::kTextureDisplace;
        instruction.inputs_ = {0, 1, 1, {}};
        outputs[1].texture_ = {42, 2, 3};
        outputs[1].scalar_ = -100;
        require(draw().commands_[0].texture_displace_->map_ == outputs[1].texture_);
        require(draw().commands_[0].texture_displace_->strength_ == -1);
        instruction.inputs_[2].reset();
        require(draw().commands_[0].texture_displace_->strength_ == 0.05f);
        instruction.node_ = registry.MakeNode(1, "texture.stack");
        instruction.operation_ = graph::Operation::kTextureStack;
        instruction.inputs_ = {0, {}, 1, {}, {}, {}, {}, 0};
        const auto layered = draw(3);
        require(layered.commands_.size() == 3 && layered.vertices_.size() == 12);
        require(layered.commands_[1].texture_ == outputs[1].texture_);
        require(layered.commands_[2].first_index_ == 12);
        require(layered.commands_[1].blend_ == render::BlendMode::kSourceOver);
        instruction.node_.properties_["composite_mode"] = 1.0;
        require(draw(3).commands_[0].blend_ == render::BlendMode::kSourceOver);
        require(draw(3).commands_[1].blend_ == render::BlendMode::kAdd);
        std::cout << "Texture commands passed: color binding, fallback, clamp and "
                     "unsupported-operation rejection\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
