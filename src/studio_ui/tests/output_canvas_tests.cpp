#include <imgui.h>
#include <imgui_internal.h>

#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "output_canvas.h"
#include "rhythm/project/package.h"
#include "rhythm/project/store.h"

namespace {
using namespace rhythm;
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct ContextDeleter {
    void operator()(ImGuiContext* context) const { ImGui::DestroyContext(context); }
};
editor::Snapshot Base() {
    graph::Registry registry;
    editor::Snapshot snapshot;
    snapshot.document_.id_ = "output-canvas-ui";
    snapshot.document_.canvas_ = {200, 100};
    snapshot.document_.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                                 registry.MakeNode(2, "texture.affine"),
                                 registry.MakeNode(3, "output.texture")};
    snapshot.document_.nodes_[1].properties_["scale"] = .5;
    snapshot.document_.edges_ = {{1, 1, 2, "source"}, {2, 2, 3, "source"}};
    snapshot.document_.output_ = 3;
    return snapshot;
}
class Fixture final {
   public:
    explicit Fixture(const std::filesystem::path& locale) {
        context_.reset(ImGui::CreateContext());
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {800, 700};
        io.DeltaTime = 1.0f / 60;
        Check(io.Fonts->Build(), "font atlas");
        std::ifstream input(locale);
        text_ = nlohmann::json::parse(input).get<std::map<std::string, std::string>>();
        Frame();
        Frame("canvas.edit");
        Frame();
    }
    void Frame(const std::string& activate = {}) {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({800, 700});
        ImGui::Begin("Output test", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        if (!activate.empty()) ImGui::ActivateItemByID(ImGui::GetID(("###" + activate).c_str()));
        const auto extent = history_.Current().document_.canvas_;
        auto result = canvas_.Draw(history_.Current(), selected_, 1,
                                   {double(extent.width_), double(extent.height_)}, editable_,
                                   current_, text_);
        const auto first = ImGui::GetItemRectMin(), last = ImGui::GetItemRectMax();
        image_ = {first.x, first.y, last.x - first.x, last.y - first.y};
        if (result.committed_) {
            Check(history_.Apply(std::move(*result.committed_),
                                 history_.Current().document_.revision_),
                  "UI commit");
            ++commits_;
        }
        changed_ |= result.preview_changed_;
        ImGui::End();
        ImGui::Render();
    }
    geometry2d::Point Point(double x, double y) const {
        return {image_.x_ + image_.width_ * x, image_.y_ + image_.height_ * y};
    }
    void Move(geometry2d::Point point) {
        ImGui::GetIO().AddMousePosEvent(float(point.x_), float(point.y_));
        Frame();
    }
    void Button(bool down) {
        ImGui::GetIO().AddMouseButtonEvent(0, down);
        Frame();
    }
    void Mode(const char* mode) {
        Frame(mode);
        Frame();
    }
    void Drag(geometry2d::Point from, geometry2d::Point to) {
        Move(from);
        Button(true);
        Check(canvas_.Active(), "handle starts captured edit");
        for (int step = 1; step <= 4; ++step)
            Move({from.x_ + (to.x_ - from.x_) * step / 4, from.y_ + (to.y_ - from.y_) * step / 4});
        Button(false);
        Frame();
    }
    std::unique_ptr<ImGuiContext, ContextDeleter> context_{};
    studio::OutputCanvas canvas_{};
    editor::History history_{Base()};
    std::map<std::string, std::string> text_{};
    geometry2d::Rect image_{};
    graph::NodeId selected_ = 2;
    int commits_ = 0;
    bool editable_ = true;
    bool current_ = true;
    bool changed_ = false;
};
void Run(const std::filesystem::path& locale, const std::filesystem::path& root) {
    Fixture fixture(locale);
    const auto initial = fixture.history_.Current();
    const auto letterbox = geometry2d::Point{fixture.image_.x_ + 40, fixture.image_.y_ - 15};
    fixture.Move(letterbox);
    fixture.Button(true);
    Check(!fixture.canvas_.Active(), "letterbox never picks canvas");
    fixture.Button(false);
    fixture.Drag(fixture.Point(.5, .5), fixture.Point(.6, .6));
    Check(fixture.commits_ == 1 && fixture.changed_, "one drag one commit and preview");
    std::cout << "move="
              << graph::Scalar(fixture.history_.Current().document_.nodes_[1], "translate_x", 0)
              << " viewport=" << fixture.image_.x_ << ',' << fixture.image_.y_ << ','
              << fixture.image_.width_ << ',' << fixture.image_.height_ << '\n';
    Check(std::abs(graph::Scalar(fixture.history_.Current().document_.nodes_[1], "translate_x", 0) -
                   .1) < 1.0 / fixture.image_.width_,
          "real mouse maps canvas move");
    Check(fixture.history_.Undo() && !fixture.history_.Undo(), "single undo for drag");
    fixture.Frame();
    fixture.Mode("canvas.rotate");
    fixture.Drag(fixture.Point(.5, .3), fixture.Point(.6, .5));
    Check(std::abs(graph::Scalar(fixture.history_.Current().document_.nodes_[1], "rotation", 0) -
                   90) < 1.0,
          "rotation ring drag");
    Check(fixture.history_.Undo(), "undo rotation");
    fixture.Frame();
    fixture.Mode("canvas.scale");
    fixture.Drag(fixture.Point(.75, .75), fixture.Point(.85, .9));
    Check(std::abs(graph::Scalar(fixture.history_.Current().document_.nodes_[1], "scale_x", 0) -
                   1.4) < 4.0 / fixture.image_.width_,
          "corner scale drag");
    Check(fixture.history_.Undo(), "undo scale");
    fixture.Frame();
    fixture.Mode("canvas.pivot");
    fixture.Drag(fixture.Point(.5, .5), fixture.Point(.55, .55));
    const auto pose = std::get<editor::CanvasTarget>(
            editor::InspectCanvasTarget(fixture.history_.Current(), 2));
    const auto original = std::get<editor::CanvasTarget>(editor::InspectCanvasTarget(initial, 2));
    const auto before = geometry2d::Compose(original.pose_, original.canvas_);
    const auto after = geometry2d::Compose(pose.pose_, pose.canvas_);
    for (std::size_t index = 0; index < before.values_.size(); ++index)
        Check(std::abs(before.values_[index] - after.values_[index]) < 1e-5,
              "pivot preserves image");
    fixture.Mode("canvas.move");
    fixture.Move(fixture.Point(.5, .5));
    fixture.Button(true);
    fixture.Move(fixture.Point(.6, .6));
    Check(fixture.canvas_.Active(), "preview active before escape");
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
    fixture.Frame();
    Check(!fixture.canvas_.Active() && fixture.commits_ == 4, "Escape cancels without commit");
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
    fixture.Button(false);
    fixture.Move(fixture.Point(.5, .5));
    fixture.Button(true);
    fixture.selected_ = 1;
    fixture.Frame();
    Check(!fixture.canvas_.Active(), "selection change cancels");
    fixture.Button(false);
    fixture.selected_ = 2;
    fixture.Frame();
    fixture.Move(fixture.Point(.5, .5));
    fixture.Button(true);
    ImGui::GetIO().AddFocusEvent(false);
    fixture.Frame();
    Check(!fixture.canvas_.Active(), "focus loss cancels");
    ImGui::GetIO().AddFocusEvent(true);
    fixture.Button(false);
    fixture.current_ = false;
    fixture.Frame();
    fixture.Move(fixture.Point(.5, .5));
    fixture.Button(true);
    Check(!fixture.canvas_.Active(), "old output cannot start drag");
    fixture.Button(false);
    const auto snapshot = fixture.history_.Current();
    project::Save(root, snapshot);
    const auto reopened = project::Load(root).snapshot_;
    Check(project::EncodeGraph(reopened.document_) == project::EncodeGraph(snapshot.document_) &&
                  reopened.document_.nodes_[1].properties_ ==
                          snapshot.document_.nodes_[1].properties_,
          "save/reopen author transform and stable wire representation");
    const auto plan =
            std::get<graph::ExecutionPlan>(graph::Compile(reopened.document_, graph::Registry{}));
    const auto package = project::EncodePackage(reopened.document_, reopened.title_);
    Check(project::EncodeProgram(project::DecodePackage(package).program_) ==
                  project::EncodeProgram(plan),
          "edited author document publishes same program");
    std::cout << "Output canvas: actual mouse four handles, cancellation, old-output guard, "
                 "save/reopen/publication passed\n";
}
}  // namespace
int main(int argc, char* argv[]) {
    try {
        Check(argc == 3, "locale root");
        Run(argv[1], argv[2]);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
