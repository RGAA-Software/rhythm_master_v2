#include <imgui.h>
#include <imgui_internal.h>

#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "graph_canvas.h"
#include "operator_help.h"
#include "rhythm/editor/commands.h"
#include "rhythm/graph/compiler.h"

namespace {
using rhythm::editor::Position;

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct ContextDeleter {
    void operator()(ImGuiContext* context) const { ImGui::DestroyContext(context); }
};

// This test drives the real canvas through ImGui's input queue without an OS
// window, so it neither moves the user's mouse nor touches their saved project.
class CanvasFixture final {
   public:
    explicit CanvasFixture(std::uint64_t count = 3) {
        context_.reset(ImGui::CreateContext());
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {1000, 700};
        io.DeltaTime = 1.0f / 60;
        Check(io.Fonts->Build(), "font atlas");
        snapshot_.document_.nodes_ = {registry_.MakeNode(1, "core.time"),
                                      registry_.MakeNode(2, "signal.oscillator"),
                                      registry_.MakeNode(3, "texture.gradient")};
        snapshot_.positions_ = {{1, {100, 100}}, {2, {400, 100}}, {3, {100, 330}}};
        for (std::uint64_t index = 4; index <= count; ++index) {
            snapshot_.document_.nodes_.push_back(registry_.MakeNode(index, "scalar.math"));
            snapshot_.positions_[index] = {100 + float(index % 16) * 360,
                                           700 + float(index / 16) * 220};
            snapshot_.document_.edges_.push_back({index, index == 4 ? 1 : index - 1, index, "a"});
        }
        for (int frame = 0; frame < 6; ++frame) Frame();
    }
    void Frame(const std::string& activate = {}, const std::string& child = {}) {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({1000, 700});
        ImGui::Begin("Canvas test", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings);
        if (!activate.empty()) {
            auto seed = ImGui::GetCurrentWindow()->ID;
            if (!child.empty()) {
                bool found = false;
                for (const auto* window : ImGui::GetCurrentContext()->Windows)
                    if (std::string_view(window->Name).find(child) != std::string_view::npos &&
                        window->WasActive) {
                        seed = window->ID;
                        found = true;
                        break;
                    }
                Check(found, "navigation child exists");
            }
            ImGui::ActivateItemByID(ImHashStr(("###" + activate).c_str(), 0, seed));
            if (activate == "navigation.search")
                ImGui::GetCurrentContext()->NavNextActivateFlags = ImGuiActivateFlags_PreferInput;
        }
        if (auto next = canvas_.Draw(snapshot_, registry_, text_, previews_)) {
            snapshot_ = std::move(*next);
            ++edits_;
        }
        ImGui::End();
        ImGui::Render();
    }
    void Move(Position screen) {
        ImGui::GetIO().AddMousePosEvent(screen.x_, screen.y_);
        Frame();
    }
    void Button(int button, bool down) {
        ImGui::GetIO().AddMouseButtonEvent(button, down);
        Frame();
    }
    void Drag(Position from, Position to, int button = ImGuiMouseButton_Left) {
        Move(from);
        Button(button, true);
        for (int step = 1; step <= 6; ++step) {
            const auto fraction = float(step) / 6;
            Move({from.x_ + (to.x_ - from.x_) * fraction, from.y_ + (to.y_ - from.y_) * fraction});
        }
        Button(button, false);
        Frame();
    }
    // Declaration order destroys the canvas before its enclosing ImGui context.
    std::unique_ptr<ImGuiContext, ContextDeleter> context_{};
    rhythm::studio::GraphCanvas canvas_{};
    rhythm::graph::Registry registry_{};
    rhythm::editor::Snapshot snapshot_{};
    rhythm::studio::CanvasPreviews previews_{};
    std::map<std::string, std::string> text_{};
    int edits_ = 0;
};

bool InspectionTexture() {
    for (const auto* window : ImGui::GetCurrentContext()->Windows)
        if (window->Active &&
            std::string_view(window->Name).find("###node.inspection.") != std::string_view::npos)
            for (const auto& command : window->DrawList->CmdBuffer)
                if (command.ElemCount && command.GetTexID() == 1) return true;
    return false;
}

void Navigation(const std::map<std::string, std::string>& text) {
    for (const auto count : {200, 500, 1000}) {
        CanvasFixture fixture(count);
        fixture.text_ = text;
        const auto before = fixture.snapshot_;
        fixture.canvas_.Select(count);
        fixture.Frame("navigation.focus");
        fixture.Frame();
        // Activation is queued by ImGui; node-editor applies its navigation
        // transform when entering the following canvas frame.
        fixture.Frame();
        const auto position = before.positions_.at(count);
        const auto focused = fixture.canvas_.ToScreen({position.x_ + 20, position.y_ + 14});
        Check(focused.x_ > 0 && focused.x_ < 1000 && focused.y_ > 0 && focused.y_ < 700,
              "selected node must be visible at editable zoom");
        const auto unit = fixture.canvas_.ToScreen({position.x_ + 120, position.y_ + 14});
        std::cout << "Focus " << count << ": node=" << fixture.canvas_.Selection()
                  << " screen=" << focused.x_ << ',' << focused.y_
                  << " scale=" << (unit.x_ - focused.x_) / 100 << '\n';
        Check(unit.x_ - focused.x_ > 60,
              "focus must zoom into a usable node, not retain full-graph zoom");
        fixture.Frame("navigation.fit");
        fixture.Frame();
        Check(fixture.snapshot_ == before, "focus/fit must preserve document and authored layout");
    }
    CanvasFixture fixture(1000);
    fixture.text_ = text;
    auto& document = fixture.snapshot_.document_;
    std::erase_if(document.edges_, [](const auto& edge) { return edge.to_ == 777; });
    document.signals_ = {{"navigation-clock", 1}};
    document.bindings_ = {{777, "a", "navigation-clock"}};
    ++document.revision_;
    const auto before = fixture.snapshot_;
    fixture.Frame("navigation.find");
    fixture.Frame();
    fixture.Frame();
    // Popup receives keyboard focus through the real filter input; its child
    // submits only matching visible rows from the thousand-node author graph.
    ImGui::GetIO().AddInputCharactersUTF8("777");
    fixture.Frame();
    fixture.Frame("node.777", "navigation.rows");
    fixture.Frame();
    Check(fixture.canvas_.Selection() == 777, "search result must select the authored node");
    fixture.Frame("navigation.upstream");
    fixture.Frame();
    fixture.Frame("node.1", "navigation.rows");
    fixture.Frame();
    Check(fixture.canvas_.Selection() == 1, "upstream must resolve a named signal binding");
    fixture.Frame("navigation.downstream");
    fixture.Frame();
    fixture.Frame("node.777", "navigation.rows");
    fixture.Frame();
    Check(fixture.canvas_.Selection() == 777, "downstream must include named consumers");
    Check(fixture.snapshot_ == before, "search/dependency navigation cannot edit the work");

    document.bindings_.front().signal_ = "missing";
    ++document.revision_;
    fixture.Frame("navigation.upstream");
    fixture.Frame();
    fixture.Frame("node.1", "navigation.rows");
    Check(fixture.canvas_.Selection() == 777,
          "invalid bindings cannot expose a partial dependency result");
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
    fixture.Frame();
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
    fixture.Frame();

    fixture.canvas_.Select(3);
    fixture.previews_.enabled_ = true;
    fixture.previews_.textures_[3] = 1;
    fixture.previews_.current_ = false;
    fixture.Frame("navigation.inspect");
    fixture.Frame();
    Check(!InspectionTexture(), "retained output cannot appear as the current inspection image");
    fixture.previews_.current_ = true;
    fixture.Frame();
    Check(InspectionTexture(), "inspection must reuse the selected texture capture");
    fixture.canvas_.Select(1);
    fixture.Frame();
    Check(!InspectionTexture(), "changing output domain must not keep the previous texture");
    fixture.canvas_.Select(3);
    fixture.previews_.enabled_ = false;
    fixture.Frame();
    Check(!InspectionTexture(), "disabled previews must not display retained captures");
}

void DragWithOverlappingDomainIds() {
    CanvasFixture fixture;
    // With the old independent counters, node 3 collided with node 2's output
    // pin 3 and its title drag activated that other node's connection instead.
    const auto before = fixture.snapshot_;
    const auto position = before.positions_.at(3);
    const auto start = fixture.canvas_.ToScreen({position.x_ + 20, position.y_ + 14});
    fixture.Drag(start, {start.x_ + 80, start.y_ + 50});
    Check(fixture.snapshot_.positions_.at(3) != position,
          "Node 3 title drag must not activate another node's pin with numeric ID 3");
    Check(fixture.snapshot_.positions_.at(1) == before.positions_.at(1) &&
                  fixture.snapshot_.positions_.at(2) == before.positions_.at(2) &&
                  fixture.snapshot_.document_ == before.document_,
          "Dragging must only move the targeted node");
}

void ThousandNodeCanvas() {
    CanvasFixture fixture(1000);
    const auto document = fixture.snapshot_.document_;
    for (int step = 0; step < 24; ++step) {
        fixture.Move(fixture.canvas_.ToScreen({120, 114}));
        ImGui::GetIO().AddMouseWheelEvent(0, 1);
        fixture.Frame();
        fixture.Frame();
    }
    const auto before = fixture.snapshot_.positions_.at(1);
    const auto start = fixture.canvas_.ToScreen({before.x_ + 20, before.y_ + 14});
    fixture.Drag(start, {start.x_ + 60, start.y_ + 40});
    Check(fixture.snapshot_.positions_.at(1) != before && fixture.snapshot_.document_ == document,
          "1000-node canvas: zoomed header drag must move only the node layout");
    const auto saved = fixture.snapshot_.positions_;
    fixture.Drag({900, 650}, {850, 600}, ImGuiMouseButton_Right);
    Check(fixture.snapshot_.positions_ == saved && fixture.snapshot_.document_ == document,
          "1000-node canvas: panning must preserve graph and authored positions");
}

void DragNodeAndConnect() {
    CanvasFixture fixture;
    const auto before = fixture.snapshot_.positions_.at(1);
    const auto start = fixture.canvas_.ToScreen({before.x_ + 20, before.y_ + 14});
    fixture.Drag(start, {start.x_ + 60, start.y_ + 40});
    const auto after = fixture.snapshot_.positions_.at(1);
    Check(after.x_ > before.x_ + 5 && after.y_ > before.y_ + 5,
          "Dragging a node title must move the node, not begin a link");
    Check(fixture.snapshot_.document_.edges_.empty(), "Node drag must not create a link");
    fixture.Move(fixture.canvas_.ToScreen({after.x_ + 20, after.y_ + 14}));
    fixture.Button(ImGuiMouseButton_Left, true);
    fixture.Button(ImGuiMouseButton_Left, false);
    fixture.Frame();
    Check(fixture.canvas_.Selection() == 1, "Selection must use the authored node ID");
    const auto target = fixture.snapshot_.positions_.at(2);
    fixture.Drag(fixture.canvas_.ToScreen({after.x_ + 201, after.y_ + 50}),
                 fixture.canvas_.ToScreen({target.x_ + 19, target.y_ + 50}));
    Check(fixture.snapshot_.document_.edges_.size() == 1, "Dragging ports must still connect");
    const auto& edge = fixture.snapshot_.document_.edges_.front();
    Check(edge.from_ == 1 && edge.to_ == 2 && edge.input_ == "time",
          "Link must retain authored node/port identities");
    const auto second_start = fixture.canvas_.ToScreen({target.x_ + 20, target.y_ + 14});
    fixture.Drag(second_start, {second_start.x_ + 40, second_start.y_ + 20});
    Check(fixture.snapshot_.positions_.at(2) != target, "Connected nodes must remain draggable");
    Check(fixture.snapshot_.document_.edges_.size() == 1, "Moving must preserve the existing link");
    const auto moved = fixture.snapshot_.positions_.at(2);
    fixture.Move(fixture.canvas_.ToScreen({moved.x_ + 20, moved.y_ + 14}));
    fixture.Button(ImGuiMouseButton_Left, true);
    fixture.Button(ImGuiMouseButton_Left, false);
    fixture.Frame();
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Delete, true);
    fixture.Frame();
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Delete, false);
    fixture.Frame();
    fixture.Frame();
    Check(fixture.snapshot_.document_.nodes_.size() == 2 &&
                  fixture.snapshot_.document_.nodes_.front().id_ == 1 &&
                  fixture.snapshot_.document_.nodes_.back().id_ == 3 &&
                  fixture.snapshot_.document_.edges_.empty(),
          "Delete must map UI IDs back to the selected node and its links");
}

void PanCursor() {
    CanvasFixture fixture;
    const auto snapshot = fixture.snapshot_;
    const auto before = fixture.canvas_.ToScreen({100, 100});
    fixture.Move({100, 600});
    fixture.Button(ImGuiMouseButton_Right, true);
    fixture.Move({140, 620});
    fixture.Frame();
    Check(ImGui::GetMouseCursor() == ImGuiMouseCursor_Hand, "Right-drag must show the hand cursor");
    const auto after = fixture.canvas_.ToScreen({100, 100});
    Check(std::abs(after.x_ - before.x_ - 40) < 2 && std::abs(after.y_ - before.y_ - 20) < 2,
          "Right-drag must pan the view");
    fixture.Button(ImGuiMouseButton_Right, false);
    Check(ImGui::GetMouseCursor() == ImGuiMouseCursor_Arrow, "Releasing pan must restore cursor");
    Check(fixture.snapshot_ == snapshot, "Panning must not change graph data or node layout");
}

void ReplaceDeletedOutput() {
    CanvasFixture fixture;
    auto& document = fixture.snapshot_.document_;
    document.nodes_ = {fixture.registry_.MakeNode(1, "texture.gradient"),
                       fixture.registry_.MakeNode(2, "texture.affine"),
                       fixture.registry_.MakeNode(3, "output.texture")};
    document.edges_ = {{1, 1, 2, "source"}, {2, 2, 3, "source"}};
    document.output_ = 3;
    ++document.revision_;
    fixture.canvas_.RestoreLayout();
    for (int frame = 0; frame < 6; ++frame) fixture.Frame();
    const auto before = fixture.snapshot_;
    const auto position = fixture.snapshot_.positions_.at(3);
    fixture.Move(fixture.canvas_.ToScreen({position.x_ + 20, position.y_ + 14}));
    fixture.Button(ImGuiMouseButton_Left, true);
    fixture.Button(ImGuiMouseButton_Left, false);
    fixture.Frame();
    Check(fixture.canvas_.Selection() == 3, "select existing output before deletion");
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Delete, true);
    fixture.Frame();
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Delete, false);
    fixture.Frame();
    fixture.Frame();
    Check(document.nodes_.size() == 2 && document.output_ == 0,
          "deleting the active output must clear its author reference");
    rhythm::editor::History history(before);
    Check(history.Apply(fixture.snapshot_, before.document_.revision_) && history.Undo() &&
                  history.Current().document_.output_ == 3,
          "undo must restore the deleted output and its reference together");
    auto rebuilt = std::get<rhythm::editor::Snapshot>(rhythm::editor::AddNode(
            fixture.snapshot_, fixture.registry_, "output.texture", {800, 100}, 4));
    rebuilt = std::get<rhythm::editor::Snapshot>(
            rhythm::editor::Connect(rebuilt, fixture.registry_, 2, 4, "source"));
    Check(rebuilt.document_.output_ == 4 &&
                  std::holds_alternative<rhythm::graph::ExecutionPlan>(
                          rhythm::graph::Compile(rebuilt.document_, fixture.registry_)),
          "replacement output must compile without retaining the deleted ID");
}

void InlinePreviewVisibility() {
    CanvasFixture fixture;
    fixture.previews_.enabled_ = true;
    fixture.previews_.textures_[3] = 1;
    rhythm::runtime::SignalTrace trace;
    trace.samples_[0] = -1;
    trace.samples_[1] = 1;
    trace.count_ = 2;
    trace.value_ = 1;
    fixture.previews_.signals_[1] = trace;
    fixture.previews_.signals_[2] = trace;
    fixture.canvas_.RestoreLayout();
    for (int frame = 0; frame < 6; ++frame) fixture.Frame();
    Check(fixture.canvas_.PreviewNodes().size() == 3 && fixture.canvas_.DrawnPreviews() == 3,
          "Visible scalar, signal and texture nodes must request and draw inline previews");
    const auto signal_before = fixture.snapshot_.positions_.at(2);
    const auto plot = fixture.canvas_.ToScreen({signal_before.x_ + 90, signal_before.y_ + 120});
    fixture.Drag(plot, {plot.x_ + 40, plot.y_ + 25});
    Check(fixture.snapshot_.positions_.at(2) != signal_before &&
                  fixture.snapshot_.document_.edges_.empty(),
          "Dragging the signal plot must move its node without capturing a widget or linking");
    const auto before = fixture.snapshot_.positions_.at(3);
    const auto image = fixture.canvas_.ToScreen({before.x_ + 90, before.y_ + 120});
    fixture.Drag(image, {image.x_ + 60, image.y_ + 40});
    Check(fixture.snapshot_.positions_.at(3) != before &&
                  fixture.snapshot_.document_.edges_.empty(),
          "Dragging the inline image must move its node without starting a link");
    fixture.previews_.enabled_ = false;
    fixture.Frame();
    Check(fixture.canvas_.PreviewNodes().empty() && fixture.canvas_.DrawnPreviews() == 0,
          "Disabling previews must remove viewer demand");
    fixture.previews_.enabled_ = true;
    fixture.Frame();
    fixture.Drag({100, 600}, {-2000, 600}, ImGuiMouseButton_Right);
    Check(fixture.canvas_.PreviewNodes().empty(),
          "Offscreen previews must stop requesting evaluation");
}

void EventPreviewAndHelp() {
    CanvasFixture fixture;
    fixture.snapshot_.document_.nodes_[0] = fixture.registry_.MakeNode(1, "event.beat");
    fixture.snapshot_.document_.nodes_[1] = fixture.registry_.MakeNode(2, "event.envelope");
    fixture.snapshot_.document_.edges_ = {{1, 1, 2, "events"}};
    fixture.previews_.enabled_ = true;
    rhythm::runtime::SignalTrace trace;
    trace.samples_[0] = 0;
    trace.samples_[1] = 1;
    trace.count_ = 2;
    trace.value_ = 1;
    trace.event_observation_ = rhythm::runtime::EventObservation{1, 1, 0.5};
    fixture.previews_.signals_[1] = trace;
    fixture.canvas_.RestoreLayout();
    for (int frame = 0; frame < 6; ++frame) fixture.Frame();
    Check(fixture.canvas_.PreviewNodes().size() == 3,
          "event output missing from canvas preview demand");
    const auto before = fixture.snapshot_.positions_.at(1);
    const auto plot = fixture.canvas_.ToScreen({before.x_ + 90, before.y_ + 120});
    fixture.Drag(plot, {plot.x_ + 40, plot.y_ + 25});
    Check(fixture.snapshot_.positions_.at(1) != before &&
                  fixture.snapshot_.document_.edges_.size() == 1,
          "event histogram captured drag or changed its existing wire");
    ImGui::NewFrame();
    ImGui::Begin("Event help");
    for (const auto type : {"event.beat", "event.envelope", "texture.feedback", "gpu.particles"})
        rhythm::studio::DrawOperatorHelp(*fixture.registry_.Find(type), {}, true);
    ImGui::End();
    ImGui::Render();
}
}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2) {
            std::ifstream input(argv[1]);
            Navigation(nlohmann::json::parse(input).get<std::map<std::string, std::string>>());
            std::cout << "Navigation: 200/500/1000-node focus, search, named dependencies, current "
                         "preview checks pass\n";
            return 0;
        }
        DragWithOverlappingDomainIds();
        DragNodeAndConnect();
        PanCursor();
        ReplaceDeletedOutput();
        InlinePreviewVisibility();
        EventPreviewAndHelp();
        ThousandNodeCanvas();
        std::cout
                << "Canvas interactions passed: node drag, port link, connected drag, pan/cursor\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
