#include <imgui.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

#include "graph_canvas.h"

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
    void Frame() {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({1000, 700});
        ImGui::Begin("Canvas test", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings);
        if (auto next = canvas_.Draw(snapshot_, registry_, {}, previews_)) {
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
    int edits_ = 0;
};

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

void InlinePreviewVisibility() {
    CanvasFixture fixture;
    fixture.previews_.enabled_ = true;
    fixture.previews_.textures_[3] = 1;
    fixture.canvas_.RestoreLayout();
    for (int frame = 0; frame < 6; ++frame) fixture.Frame();
    Check(fixture.canvas_.PreviewNodes().size() == 1 &&
                  fixture.canvas_.PreviewNodes().front() == 3 &&
                  fixture.canvas_.DrawnPreviews() == 1,
          "Only a visible texture node should request and draw an inline preview");
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
}  // namespace

int main() {
    try {
        DragWithOverlappingDomainIds();
        DragNodeAndConnect();
        PanCursor();
        InlinePreviewVisibility();
        ThousandNodeCanvas();
        std::cout
                << "Canvas interactions passed: node drag, port link, connected drag, pan/cursor\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
