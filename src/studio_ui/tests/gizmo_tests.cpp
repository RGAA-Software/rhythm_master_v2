#include <imgui.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

#include "gizmo.h"

namespace {
using namespace rhythm;
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct ContextDeleter {
    void operator()(ImGuiContext* context) const { ImGui::DestroyContext(context); }
};
class Fixture final {
   public:
    explicit Fixture(bool orthographic) {
        context_.reset(ImGui::CreateContext());
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {1000, 850};
        io.DeltaTime = 1.0f / 60;
        Check(io.Fonts->Build(), "font atlas");
        input_.identity_ = 33;
        input_.viewport_ = {230, 270, 600, 400};
        input_.camera_.orthographic_height_ = 4;
        input_.camera_.kind_ = orthographic ? scene::ProjectionKind::kOrthographic
                                            : scene::ProjectionKind::kPerspective;
        for (int frame = 0; frame < 3; ++frame) Frame();
    }
    ~Fixture() { gizmo_.Cancel(); }
    void Frame() {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({100, 150});
        ImGui::SetNextWindowSize({800, 650});
        ImGui::Begin("Gizmo test", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        ImGui::Dummy({780, 620});
        last_ = gizmo_.Draw(input_);
        if (last_.changed_) input_.local_ = last_.local_;
        ImGui::End();
        ImGui::Render();
    }
    void Move(float x, float y) {
        ImGui::GetIO().AddMousePosEvent(x, y);
        Frame();
    }
    void Button(bool down) {
        ImGui::GetIO().AddMouseButtonEvent(0, down);
        Frame();
    }
    void Reset() {
        gizmo_.Cancel();
        input_.local_ = {};
        Button(false);
    }
    std::unique_ptr<ImGuiContext, ContextDeleter> context_{};
    studio::Gizmo gizmo_{};
    studio::GizmoInput input_{};
    studio::GizmoResult last_{};
};
void Projection(bool orthographic) {
    Fixture fixture(orthographic);
    fixture.input_.parent_ = scene::Compose({}, {0, 0, std::sqrt(.5), std::sqrt(.5)}, {2, 1, 1});
    fixture.input_.world_ = true;
    fixture.Move(530, 470);
    Check(fixture.last_.hovered_, "center handle hover");
    fixture.Button(true);
    Check(fixture.last_.active_, "center handle captured");
    fixture.Move(570, 470);
    Check(fixture.last_.changed_, "actual translation drag");
    fixture.Button(false);
    Check(!fixture.last_.active_, "mouse release ends capture");
    const auto world = scene::Multiply(fixture.input_.parent_, fixture.input_.local_);
    const auto expected = orthographic ? .4 : 6 * std::tan(3.141592653589793 / 6) * .1;
    Check(std::abs(world.values_[12] - expected) < .005 && std::abs(world.values_[13]) < .005 &&
                  std::abs(fixture.input_.local_.values_[13] + expected) < .005,
          "screen/world/local translation under rotated nonuniform parent");
    fixture.Reset();
    fixture.Move(530, 470);
    fixture.Button(true);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
    fixture.Frame();
    Check(fixture.last_.canceled_ && !fixture.last_.active_, "Escape cancels upstream capture");
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
    fixture.Button(false);
    fixture.Move(530, 470);
    fixture.Button(true);
    ++fixture.input_.identity_;
    fixture.Frame();
    Check(fixture.last_.canceled_ && !fixture.last_.active_, "identity switch cancels");
    fixture.Button(false);
    fixture.Move(530, 700);
    fixture.Button(true);
    Check(!fixture.last_.active_ && !fixture.last_.hovered_,
          "outside viewport rejects despite upstream YMax typo");
    fixture.Button(false);
}
void OtherOperations() {
    Fixture fixture(true);
    for (const auto operation : {studio::GizmoOperation::kRotate, studio::GizmoOperation::kScale}) {
        for (const bool world : {false, true}) {
            if (world && operation == studio::GizmoOperation::kScale) continue;
            fixture.input_.operation_ = operation;
            fixture.input_.world_ = world;
            fixture.Reset();
            bool changed = false;
            for (int y = 410; y <= 530 && !changed; y += 6)
                for (int x = 470; x <= 590 && !changed; x += 6) {
                    fixture.Move(float(x), float(y));
                    if (!fixture.last_.hovered_) continue;
                    fixture.Button(true);
                    if (fixture.last_.active_) {
                        fixture.Move(float(x + 17), float(y + 23));
                        changed = fixture.last_.changed_;
                    }
                    fixture.Button(false);
                }
            Check(changed && scene::ValidAffine(fixture.input_.local_),
                  "rotate/scale local/world real mouse");
            const auto& values = fixture.input_.local_.values_;
            Check(std::abs(values[12]) < 1e-5 && std::abs(values[13]) < 1e-5 &&
                          std::abs(values[14]) < 1e-5,
                  "origin rotation/scale preserves translation");
        }
    }
    fixture.input_.world_ = true;
    bool rejected = false;
    try {
        fixture.Frame();
    } catch (const std::invalid_argument& error) {
        rejected = std::string_view(error.what()) == "gizmo.world_scale_unsupported";
        ImGui::End();
        ImGui::EndFrame();
    }
    Check(rejected, "unsupported world scale cannot silently use local axes");
}
}  // namespace
int main() {
    try {
        Projection(true);
        Projection(false);
        OtherOperations();
        std::cout << "ImGuizmo 1.10 + project docking ImGui: perspective/ortho, parent conversion, "
                     "local/world operations, input capture/cancel/viewport passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
