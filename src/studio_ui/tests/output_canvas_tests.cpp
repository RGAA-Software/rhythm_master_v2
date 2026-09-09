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
#include "rhythm/runtime/runtime.h"

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
        ImGui::SetNextWindowSize({width_, 700});
        ImGui::Begin("Output test", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        if (!activate.empty()) ImGui::ActivateItemByID(ImGui::GetID(("###" + activate).c_str()));
        const auto extent = history_.Current().document_.canvas_;
        auto result = canvas_.Draw(history_.Current(), selected_, 1,
                                   {double(extent.width_), double(extent.height_)}, editable_,
                                   current_, text_, outputs_, authors_);
        if (result.selected_) selected_ = *result.selected_;
        if (result.open_author_) {
            requested_author_ = result.open_author_;
            unique_instance_ = result.unique_instance_;
        }
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
    std::vector<runtime::NodeOutput> outputs_{};
    std::map<graph::NodeId, graph::AuthorNode> authors_{};
    std::optional<graph::AuthorNode> requested_author_{};
    bool unique_instance_ = false;
    editor::History history_{Base()};
    std::map<std::string, std::string> text_{};
    geometry2d::Rect image_{};
    graph::NodeId selected_ = 2;
    int commits_ = 0;
    bool editable_ = true;
    bool current_ = true;
    bool changed_ = false;
    float width_ = 800;
};
void Selection(const std::filesystem::path& locale) {
    Fixture fixture(locale);
    graph::Registry registry;
    editor::Snapshot snapshot;
    auto& document = snapshot.document_;
    document.id_ = fixture.history_.Current().document_.id_;
    document.canvas_ = {200, 100};
    document.nodes_ = {
            registry.MakeNode(1, "geometry.cube"),  registry.MakeNode(2, "scene.transform"),
            registry.MakeNode(3, "output.texture"), registry.MakeNode(4, "scene.instance"),
            registry.MakeNode(5, "scene.render"),   registry.MakeNode(6, "scene.camera")};
    document.output_ = 3;
    document.edges_ = {{1, 1, 4, "geometry"},
                       {2, 4, 2, "scene"},
                       {3, 2, 5, "scene"},
                       {4, 6, 5, "camera"},
                       {5, 5, 3, "source"}};
    auto geometry = std::make_shared<scene::Geometry>();
    geometry->model_ = std::make_shared<const scene::Model>(scene::Cube());
    auto scene = std::make_shared<scene::Scene>();
    scene->instances_.push_back({geometry, scene::ComposeEuler({{.4, 0, 0}}), {}, {4, 2}});
    runtime::NodeOutput scene_output;
    scene_output.node_ = 2;
    scene_output.scene_ = scene;
    runtime::NodeOutput camera_output;
    camera_output.node_ = 6;
    camera_output.camera_ = scene::Camera{};
    camera_output.camera_->kind_ = scene::ProjectionKind::kOrthographic;
    fixture.outputs_ = {scene_output, camera_output};
    Check(fixture.history_.Apply(snapshot, fixture.history_.Current().document_.revision_),
          "install scene fixture");
    fixture.selected_ = 0;
    fixture.current_ = false;
    fixture.Frame();
    fixture.Move(fixture.Point(.6, .6));
    fixture.Button(true);
    fixture.Button(false);
    Check(fixture.selected_ == 0, "stale scene output is not selectable");
    fixture.current_ = true;
    fixture.Frame();
    fixture.Move(fixture.Point(.6, .6));
    fixture.Button(true);
    fixture.Button(false);
    Check(fixture.selected_ == 2 && fixture.commits_ == 0 && !fixture.canvas_.Active(),
          "image click selects author without changing document or dragging");
    const auto hit = studio::PickSceneOutput(document, fixture.outputs_, 2, .6, .6);
    Check(hit.hit_ && hit.selected_ == 2 && std::abs(hit.hit_->position_.x_ - .4) < 1e-8,
          "selection uses actual frame camera and transform");
    scene->instances_[0].origin_.element_ = 23;
    scene->instances_[0].origin_.generation_ = 8;
    scene->instances_.push_back({geometry, scene::ComposeEuler({{1.2, 0, 0}}), {}, {4, 2, 24, 8}});
    fixture.Move(fixture.Point(.6, .6));
    fixture.Button(true);
    fixture.Button(false);
    fixture.Frame();
    fixture.Move(fixture.Point(.5, .5));
    fixture.Button(true);
    Check(!fixture.canvas_.Active() && fixture.commits_ == 0,
          "picking a generated element does not silently edit its entire batch");
    fixture.Button(false);
    fixture.Mode("scene_scope.edit_batch");
    fixture.Move(fixture.Point(.5, .5));
    fixture.Button(true);
    Check(fixture.canvas_.Active(), "explicit batch scope enables author gizmo");
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
    fixture.Frame();
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
    fixture.Button(false);
    Check(fixture.commits_ == 0, "cancel batch gesture without author mutation");
    scene->instances_[0].origin_.transform_ = 999;
    Check(studio::PickSceneOutput(document, fixture.outputs_, 2, .6, .6).error_ ==
                  "scene_pick.component_scope",
          "expanded internal ID cannot target a root node");
    fixture.outputs_.clear();
    Check(studio::PickSceneOutput(document, fixture.outputs_, 2, .6, .6).error_ ==
                  "canvas.wait_output",
          "missing current scene diagnosed");
}
void ComponentSelection(const std::filesystem::path& locale) {
    Fixture fixture(locale);
    graph::Registry registry;
    editor::Snapshot snapshot;
    auto& document = snapshot.document_;
    document.id_ = fixture.history_.Current().document_.id_;
    document.canvas_ = {200, 100};
    graph::ComponentDefinition inner;
    inner.type_ = "component.test.sculpture";
    inner.nodes_ = {registry.MakeNode(1, "geometry.cube"), registry.MakeNode(2, "scene.instance"),
                    registry.MakeNode(3, "scene.transform")};
    inner.output_ = 3;
    inner.edges_ = {{1, 1, 2, "geometry"}, {2, 2, 3, "scene"}};
    graph::ComponentDefinition outer;
    outer.type_ = "component.test.group";
    outer.nodes_ = {{7, inner.type_}};
    outer.output_ = 7;
    document.components_ = {inner, outer};
    document.nodes_ = {{10, outer.type_},
                       {20, outer.type_},
                       registry.MakeNode(30, "scene.merge"),
                       registry.MakeNode(40, "scene.render"),
                       registry.MakeNode(50, "output.texture"),
                       registry.MakeNode(60, "scene.camera")};
    document.output_ = 50;
    document.edges_ = {{1, 10, 30, "a"},
                       {2, 20, 30, "b"},
                       {3, 30, 40, "scene"},
                       {4, 60, 40, "camera"},
                       {5, 40, 50, "source"}};
    const auto expanded = std::get<graph::ExpandedComponentScope>(
            graph::ExpandComponentScope(document, registry, {}));
    fixture.authors_ = expanded.authors_;
    auto geometry = std::make_shared<scene::Geometry>();
    geometry->model_ = std::make_shared<const scene::Model>(scene::Cube());
    auto scene = std::make_shared<scene::Scene>();
    scene->instances_ = {
            {geometry, scene::ComposeEuler({{-.6, 0, 0}, {}, {.35, .35, .35}}), {}, {100, 10}},
            {geometry, scene::ComposeEuler({{.6, 0, 0}, {}, {.35, .35, .35}}), {}, {101, 20}}};
    runtime::NodeOutput output;
    output.node_ = 30;
    output.scene_ = scene;
    runtime::NodeOutput camera;
    camera.node_ = 60;
    camera.camera_ = scene::Camera{};
    camera.camera_->kind_ = scene::ProjectionKind::kOrthographic;
    fixture.outputs_ = {output, camera};
    Check(fixture.history_.Apply(snapshot, fixture.history_.Current().document_.revision_),
          "install nested component scene");
    fixture.selected_ = 0;
    fixture.Frame();
    fixture.Move(fixture.Point(.35, .55));
    fixture.Button(true);
    fixture.Button(false);
    fixture.Frame();
    Check(fixture.selected_ == 10 && fixture.commits_ == 0,
          "pick maps preserved executable ID to root component instance");
    fixture.Mode("scene_scope.shared");
    Check(fixture.requested_author_ == graph::AuthorNode{{10, 7}, 3} && !fixture.unique_instance_,
          "shared edit request carries exact nested author path");
    fixture.Mode("scene_scope.unique");
    Check(fixture.requested_author_ == graph::AuthorNode{{10, 7}, 3} && fixture.unique_instance_,
          "independent instance is explicit, not a runtime mesh mutation");
    fixture.Move(fixture.Point(.65, .55));
    fixture.Button(true);
    fixture.Button(false);
    fixture.Mode("scene_scope.shared");
    Check(fixture.selected_ == 20 && fixture.requested_author_ == graph::AuthorNode{{20, 7}, 3},
          "same shared definition's second instance retains distinct scope");
}
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
    fixture.Move(fixture.Point(.5, .5));
    fixture.Button(true);
    Check(fixture.canvas_.Active(), "capture before resize");
    fixture.width_ = 760;
    fixture.Frame();
    Check(!fixture.canvas_.Active() && fixture.commits_ == 4,
          "viewport resize cancels instead of jumping object");
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
        Selection(argv[1]);
        ComponentSelection(argv[1]);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
