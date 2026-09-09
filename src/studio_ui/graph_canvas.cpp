#include "graph_canvas.h"

#include <imgui.h>
#include <imgui_node_editor.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "node_inspection.h"
#include "node_navigation.h"
#include "node_visual.h"
#include "rhythm/editor/commands.h"
#include "rhythm/graph/controls.h"

namespace rhythm::studio {
namespace ed = ax::NodeEditor;
namespace {
struct ContextDeleter {
    void operator()(ed::EditorContext* context) const { ed::DestroyEditor(context); }
};
}  // namespace
class GraphCanvas::Impl final {
   public:
    Impl() {
        ed::Config config;
        config.SettingsFile = nullptr;
        config.DragButtonIndex = ImGuiMouseButton_Left;
        config.NavigateButtonIndex = ImGuiMouseButton_Right;
        context_.reset(ed::CreateEditor(&config));
        if (!context_) throw std::runtime_error("ui.node_context");
        ed::SetCurrentEditor(context_.get());
        auto& style = ed::GetStyle();
        style.NodePadding = {10, 8, 10, 8};
        style.NodeRounding = 8;
        style.NodeBorderWidth = 1.5f;
        style.Colors[ed::StyleColor_NodeBg] = ImColor(25, 32, 43);
        style.Colors[ed::StyleColor_NodeBorder] = ImColor(73, 89, 108);
        style.Colors[ed::StyleColor_SelNodeBorder] = ImColor(238, 187, 87);
        style.Colors[ed::StyleColor_HovNodeBorder] = ImColor(139, 174, 196);
        ed::SetCurrentEditor(nullptr);
    }
    std::uint64_t Node(graph::NodeId node) {
        auto [found, inserted] = node_ids_.try_emplace(node, next_item_);
        if (inserted) {
            reverse_nodes_[next_item_] = node;
            ++next_item_;
        }
        return found->second;
    }
    std::uint64_t Link(std::uint64_t edge) {
        auto [found, inserted] = link_ids_.try_emplace(edge, next_item_);
        if (inserted) {
            reverse_links_[next_item_] = edge;
            ++next_item_;
        }
        return found->second;
    }
    std::uint64_t Pin(graph::NodeId node, const std::string& port) {
        const auto key = std::make_pair(node, port);
        auto [found, inserted] = pins_.try_emplace(key, next_item_);
        if (inserted) {
            reverse_pins_[next_item_] = key;
            ++next_item_;
        }
        return found->second;
    }
    // Upstream context exclusively owned by the node-editor boundary adapter.
    std::unique_ptr<ed::EditorContext, ContextDeleter> context_{};
    // Upstream hit widgets hash numeric IDs without their Node/Pin/Link type.
    // Keep one UI identity space, independent of persisted domain identifiers.
    std::map<graph::NodeId, std::uint64_t> node_ids_{};
    std::map<std::uint64_t, std::uint64_t> link_ids_{};
    std::map<std::uint64_t, graph::NodeId> reverse_nodes_{};
    std::map<std::uint64_t, std::uint64_t> reverse_links_{};
    std::map<std::pair<graph::NodeId, std::string>, std::uint64_t> pins_{};
    std::map<std::uint64_t, std::pair<graph::NodeId, std::string>> reverse_pins_{};
    std::uint64_t next_item_ = 1;
    graph::NodeId selection_ = 0;
    std::vector<graph::NodeId> selections_{};
    std::optional<graph::NodeId> pending_selection_{};
    bool focus_selection_ = false;
    bool fit_content_ = false;
    NodeNavigation navigation_{};
    NodeInspection inspection_{};
    bool restore_layout_ = true;
    ImVec2 last_size_{};
    int stable_frames_ = 0;
    std::size_t visible_nodes_ = 0;
    editor::Position insertion_point_{};
    bool pan_pressed_ = false;
    std::vector<graph::NodeId> preview_nodes_{};
    std::size_t drawn_previews_ = 0;
};
GraphCanvas::GraphCanvas() : impl_(std::make_unique<Impl>()) {}
GraphCanvas::~GraphCanvas() = default;
graph::NodeId GraphCanvas::Selection() const { return impl_->selection_; }
std::span<const graph::NodeId> GraphCanvas::Selections() const { return impl_->selections_; }
void GraphCanvas::Select(graph::NodeId node) {
    impl_->selection_ = node;
    impl_->selections_ = {node};
    impl_->pending_selection_ = node;
}
void GraphCanvas::RestoreLayout() {
    impl_->restore_layout_ = true;
    impl_->stable_frames_ = 0;
    impl_->navigation_.Reset();
    impl_->inspection_.Close();
}
void GraphCanvas::FocusSelection() { impl_->focus_selection_ = true; }
void GraphCanvas::FitContent() { impl_->fit_content_ = true; }
std::size_t GraphCanvas::VisibleNodes() const { return impl_->visible_nodes_; }
editor::Position GraphCanvas::InsertionPoint() const { return impl_->insertion_point_; }
std::span<const graph::NodeId> GraphCanvas::PreviewNodes() const { return impl_->preview_nodes_; }
std::size_t GraphCanvas::DrawnPreviews() const { return impl_->drawn_previews_; }
editor::Position GraphCanvas::ToScreen(editor::Position position) const {
    ed::SetCurrentEditor(impl_->context_.get());
    const auto screen = ed::CanvasToScreen({position.x_, position.y_});
    ed::SetCurrentEditor(nullptr);
    return {screen.x, screen.y};
}

std::optional<editor::Snapshot> GraphCanvas::Draw(const editor::Snapshot& snapshot,
                                                  const graph::Registry& registry,
                                                  const std::map<std::string, std::string>& labels,
                                                  const CanvasPreviews& previews) {
    const auto text = [&](const std::string& key) -> const std::string& {
        const auto found = labels.find(key);
        return found == labels.end() ? key : found->second;
    };
    const auto navigation = impl_->navigation_.Draw(snapshot.document_, Selection(), labels);
    if (navigation.selected_) Select(*navigation.selected_);
    if (navigation.focus_) FocusSelection();
    if (navigation.fit_) FitContent();
    if (navigation.inspect_) impl_->inspection_.Show();
    ed::SetCurrentEditor(impl_->context_.get());
    std::optional<editor::Snapshot> next;
    const auto edit = [&]() -> editor::Snapshot& {
        if (!next) next = snapshot;
        return *next;
    };
    const auto current = [&]() -> const editor::Snapshot& { return next ? *next : snapshot; };
    const auto canvas_size = ImGui::GetContentRegionAvail();
    const auto origin = ImGui::GetCursorScreenPos();
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) impl_->pan_pressed_ = false;
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsWindowHovered() &&
        ImGui::IsMouseHoveringRect(origin, {origin.x + canvas_size.x, origin.y + canvas_size.y}))
        impl_->pan_pressed_ = true;
    ed::Begin("graph.canvas", canvas_size);
    std::map<graph::NodeId, graph::ValueType> output_types;
    std::map<graph::NodeId, PreviewBounds> preview_bounds;
    // Describe each type once per draw. Frame-local storage observes component
    // interface edits immediately without copying descriptors for every instance.
    std::map<std::string, std::optional<graph::OperatorDescriptor>> descriptors;
    impl_->drawn_previews_ = 0;
    for (const auto& node : snapshot.document_.nodes_) {
        if (impl_->restore_layout_) {
            const auto found = snapshot.positions_.find(node.id_);
            if (found != snapshot.positions_.end())
                ed::SetNodePosition(ed::NodeId(impl_->Node(node.id_)),
                                    {found->second.x_, found->second.y_});
        }
        const auto [description, inserted] = descriptors.try_emplace(node.type_);
        if (inserted)
            description->second = registry.Find(node.type_, snapshot.document_.components_);
        const auto& descriptor = description->second;
        NodeVisual visual;
        visual.id_ = impl_->Node(node.id_);
        visual.title_ = text(node.type_);
        if (const auto definition = std::find_if(
                    snapshot.document_.components_.begin(), snapshot.document_.components_.end(),
                    [&](const auto& value) { return value.type_ == node.type_; });
            definition != snapshot.document_.components_.end() && !definition->title_.empty() &&
            visual.title_ == node.type_)
            visual.title_ = definition->title_;
        if (descriptor) {
            for (const auto& port : descriptor->inputs_) {
                const auto bound = std::find_if(
                        snapshot.document_.bindings_.begin(), snapshot.document_.bindings_.end(),
                        [&](const auto& binding) {
                            return binding.node_ == node.id_ && binding.input_ == port.key_;
                        });
                const bool has_binding = bound != snapshot.document_.bindings_.end();
                const auto label =
                        text(port.key_) + (has_binding ? " [" + bound->signal_ + "]" : "");
                visual.inputs_.push_back(
                        {impl_->Pin(node.id_, port.key_), label, port.type_, has_binding});
            }
        }
        visual.output_ = {impl_->Pin(node.id_, ""), text("result"),
                          descriptor ? descriptor->output_ : graph::ValueType::kScalar};
        output_types[node.id_] = visual.output_.type_;
        visual.preview_enabled_ = previews.enabled_ && descriptor &&
                                  (descriptor->output_ == graph::ValueType::kTexture ||
                                   descriptor->output_ == graph::ValueType::kScalar ||
                                   descriptor->output_ == graph::ValueType::kSignal ||
                                   descriptor->output_ == graph::ValueType::kEvent ||
                                   descriptor->output_ == graph::ValueType::kPoints ||
                                   descriptor->output_ == graph::ValueType::kGpuPoints ||
                                   descriptor->output_ == graph::ValueType::kSceneImage ||
                                   descriptor->output_ == graph::ValueType::kDepth ||
                                   descriptor->output_ == graph::ValueType::kPath ||
                                   descriptor->output_ == graph::ValueType::kGeometry ||
                                   descriptor->output_ == graph::ValueType::kMaterial ||
                                   descriptor->output_ == graph::ValueType::kScene);
        visual.preview_waiting_ = text("preview_waiting");
        if (const auto found = previews.textures_.find(node.id_); found != previews.textures_.end())
            visual.preview_texture_ = found->second;
        if (const auto found = previews.signals_.find(node.id_); found != previews.signals_.end())
            visual.preview_signal_ = found->second;
        preview_bounds[node.id_] = DrawNode(visual);
    }
    for (const auto& edge : snapshot.document_.edges_)
        DrawLink(impl_->Link(edge.id_), impl_->Pin(edge.from_, ""),
                 impl_->Pin(edge.to_, edge.input_), output_types.at(edge.from_));
    if (ed::BeginCreate()) {
        ed::PinId first;
        ed::PinId second;
        if (ed::QueryNewLink(&first, &second) && first && second) {
            auto from = impl_->reverse_pins_.at(first.Get());
            auto to = impl_->reverse_pins_.at(second.Get());
            if (!from.second.empty()) std::swap(from, to);
            bool valid = from.second.empty() && !to.second.empty();
            if (valid) {
                auto candidate =
                        editor::Connect(snapshot, registry, from.first, to.first, to.second);
                valid = std::holds_alternative<editor::Snapshot>(candidate);
                if (valid && ed::AcceptNewItem())
                    next = std::get<editor::Snapshot>(std::move(candidate));
            }
            if (!valid) ed::RejectNewItem(ImColor(230, 70, 110));
        }
    }
    ed::EndCreate();
    if (ed::BeginDelete()) {
        ed::LinkId link;
        while (ed::QueryDeletedLink(&link))
            if (ed::AcceptDeletedItem()) {
                std::erase_if(edit().document_.edges_, [&](const auto& edge) {
                    return edge.id_ == impl_->reverse_links_.at(link.Get());
                });
            }
        ed::NodeId node;
        while (ed::QueryDeletedNode(&node))
            if (ed::AcceptDeletedItem()) {
                auto& edited = edit();
                const auto node_id = impl_->reverse_nodes_.at(node.Get());
                std::erase_if(edited.document_.nodes_,
                              [&](const auto& item) { return item.id_ == node_id; });
                std::erase_if(edited.document_.edges_, [&](const auto& edge) {
                    return edge.from_ == node_id || edge.to_ == node_id;
                });
                edited.positions_.erase(node_id);
                graph::PruneControls(edited.document_);
                std::erase_if(edited.document_.bindings_, [&](const auto& binding) {
                    return binding.node_ == node_id ||
                           std::any_of(edited.document_.signals_.begin(),
                                       edited.document_.signals_.end(), [&](const auto& signal) {
                                           return signal.source_ == node_id &&
                                                  signal.name_ == binding.signal_;
                                       });
                });
                std::erase_if(edited.document_.signals_,
                              [&](const auto& signal) { return signal.source_ == node_id; });
            }
    }
    ed::EndDelete();
    if (impl_->pending_selection_) {
        if (std::any_of(snapshot.document_.nodes_.begin(), snapshot.document_.nodes_.end(),
                        [&](const auto& node) { return node.id_ == *impl_->pending_selection_; }))
            ed::SelectNode(ed::NodeId(impl_->Node(*impl_->pending_selection_)));
        impl_->pending_selection_.reset();
    }
    std::vector<ed::NodeId> selected(snapshot.document_.nodes_.size());
    const int selected_count =
            selected.empty()
                    ? 0
                    : ed::GetSelectedNodes(selected.data(), static_cast<int>(selected.size()));
    impl_->selections_.clear();
    for (int index = 0; index < selected_count; ++index)
        impl_->selections_.push_back(
                impl_->reverse_nodes_.at(selected[static_cast<std::size_t>(index)].Get()));
    if (!impl_->selections_.empty()) impl_->selection_ = impl_->selections_.front();
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        for (const auto& node : current().document_.nodes_) {
            const auto position = ed::GetNodePosition(ed::NodeId(impl_->Node(node.id_)));
            const editor::Position value{position.x, position.y};
            if (!current().positions_.contains(node.id_) ||
                current().positions_.at(node.id_) != value)
                edit().positions_[node.id_] = value;
        }
    }
    ed::End();
    if (impl_->pan_pressed_ && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0))
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    // Docking can give the first frame a provisional 4-pixel canvas. Fit only
    // after layout has usable dimensions, otherwise the graph shrinks to a dot.
    if (std::abs(canvas_size.x - impl_->last_size_.x) < 1 &&
        std::abs(canvas_size.y - impl_->last_size_.y) < 1)
        ++impl_->stable_frames_;
    else
        impl_->stable_frames_ = 0;
    impl_->last_size_ = canvas_size;
    if (impl_->restore_layout_ && impl_->stable_frames_ >= 2 && canvas_size.x >= 100 &&
        canvas_size.y >= 100) {
        ed::NavigateToContent(0);
        impl_->restore_layout_ = false;
    }
    if (!impl_->restore_layout_ && canvas_size.x >= 100 && canvas_size.y >= 100) {
        if (impl_->fit_content_)
            ed::NavigateToContent(0);
        else if (impl_->focus_selection_ && !impl_->selections_.empty())
            ed::NavigateToSelection(true, 0);
        impl_->fit_content_ = false;
        impl_->focus_selection_ = false;
    }
    impl_->visible_nodes_ = 0;
    impl_->preview_nodes_.clear();
    const auto center =
            ed::ScreenToCanvas({origin.x + canvas_size.x * 0.5f, origin.y + canvas_size.y * 0.5f});
    impl_->insertion_point_ = {center.x, center.y};
    for (const auto& node : snapshot.document_.nodes_) {
        const auto position = ed::GetNodePosition(ed::NodeId(impl_->Node(node.id_)));
        const auto size = ed::GetNodeSize(ed::NodeId(impl_->Node(node.id_)));
        const auto first = ed::CanvasToScreen(position);
        const auto last = ed::CanvasToScreen({position.x + size.x, position.y + size.y});
        if (last.x - first.x >= 20 && last.y - first.y >= 15 && first.x >= origin.x - 1 &&
            first.y >= origin.y - 1 && last.x <= origin.x + canvas_size.x + 1 &&
            last.y <= origin.y + canvas_size.y + 1)
            ++impl_->visible_nodes_;
        const auto bounds = preview_bounds.at(node.id_);
        if (bounds.width_ <= 0) continue;
        const auto preview_first = ed::CanvasToScreen({bounds.x_, bounds.y_});
        const auto preview_last =
                ed::CanvasToScreen({bounds.x_ + bounds.width_, bounds.y_ + bounds.height_});
        if (preview_last.x - preview_first.x >= 64 && preview_last.x > origin.x &&
            preview_first.x < origin.x + canvas_size.x && preview_last.y > origin.y &&
            preview_first.y < origin.y + canvas_size.y) {
            impl_->preview_nodes_.push_back(node.id_);
            if (previews.textures_.contains(node.id_) || previews.signals_.contains(node.id_))
                ++impl_->drawn_previews_;
        }
    }
    ed::SetCurrentEditor(nullptr);
    impl_->inspection_.Draw(snapshot.document_, Selection(), previews, labels);
    return next;
}
}  // namespace rhythm::studio
