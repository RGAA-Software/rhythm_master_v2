#include <bgfx/bgfx.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <numbers>

#include "rhythm/assets/store.h"
#include "rhythm/graph/registry.h"
#include "rhythm/project/package.h"
#include "rhythm/project/store.h"
#include "rhythm/studio/studio.h"
#include "rhythm/surface_shader/program.h"
#include "studio_input.h"
#include "workflow_evidence.h"

namespace {
void Check(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
}  // namespace
int main(int argc, char** argv) {
    using namespace rhythm;
    try {
        Check(argc == 4, "resources recipe output");
        const std::filesystem::path resources(argv[1]), recipe_path(argv[2]), output(argv[3]);
        std::ifstream input(recipe_path);
        const auto recipe = nlohmann::json::parse(input);
        const auto path = output / "Projects/work.rhythmproj";
        editor::Snapshot empty;
        empty.document_.id_ = recipe.at("id").get<std::string>();
        empty.document_.canvas_ = {1280, 720};
        empty.title_ = recipe.at("title").get<std::string>();
        // Load a valid seed, then delete it through the canvas. Persistence
        // deliberately requires a valid output reference.
        graph::Registry registry;
        empty.document_.nodes_ = {registry.MakeNode(1, "output.texture")};
        empty.document_.output_ = 1;
        project::Save(path, empty);
        platform::Host host(true);
        host.Resize({1600, 1000});
        ImGui::GetIO().IniFilename = nullptr;
        auto renderer = host.CreateRenderer();
        auto font = host.CreateFontTexture(renderer);
        studio::Studio studio(resources, path);
        testing::WorkflowEvidence evidence(output);
        int frames = 0;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
        const auto frame = [&] {
            Check(host.Poll() && std::chrono::steady_clock::now() < deadline,
                  "empty authoring timeout");
            host.BeginUi();
            renderer.BeginFrame();
            testing::StudioInput::Place("###graph", {0, 0}, {820, 990});
            testing::StudioInput::Place("###inspector", {835, 0}, {750, 990});
            testing::StudioInput::Place("###output", {100, 500}, {680, 450});
            studio.Frame(host, renderer, frames++ / 60.0);
            renderer.Submit({}, host.EndUi(), 0x111822ff);
            renderer.EndFrame();
            evidence.Record("author-from-empty", studio);
        };
        testing::StudioInput ui(frame);
        ui.Settle(6);
        if (recipe.value("builtin_font", false)) {
            const auto generation = studio.Workflow().requested_generation_;
            ui.Button("###graph", "###asset.manager");
            ui.Button(testing::StudioInput::Popup(), "###text.add_builtin_font");
            for (int attempt = 0;
                 attempt < 500 && studio.Workflow().requested_generation_ == generation; ++attempt)
                frame();
            Check(studio.Workflow().requested_generation_ != generation,
                  "bundled font import not applied");
            ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
            frame();
            ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
            ui.Settle();
            ui.Button("###graph", "###save");
            for (int attempt = 0;
                 attempt < 200 && project::Load(path).snapshot_.assets_.size() != 2; ++attempt)
                frame();
            const auto imported = project::Load(path).snapshot_;
            Check(imported.assets_.size() == 2 &&
                          std::any_of(imported.assets_.begin(), imported.assets_.end(),
                                      [](const auto& asset) {
                                          return asset.media_type_ == "text/plain";
                                      }),
                  "font and license not saved together");
        }
        auto& io = ImGui::GetIO();
        io.AddMousePosEvent(400, 400);
        ui.Settle();
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
        ui.Settle();
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
        ui.Settle();
        io.AddKeyEvent(ImGuiKey_Delete, true);
        ui.Settle();
        io.AddKeyEvent(ImGuiKey_Delete, false);
        ui.Settle();
        Check(studio.Status().authored_nodes_ == 0 && !studio.HasValidPlan(),
              "must begin from an empty graph");
        std::uint64_t id = 0;
        std::vector<graph::NodeId> authored;
        for (const auto& node : recipe.at("nodes")) {
            ++id;
            const auto type = node.at("type").get<std::string>();
            ui.AddNode(type);
            const auto actual_id = studio.Workflow().selected_author_node_;
            authored.push_back(actual_id);
            Check(studio.Status().authored_nodes_ == id && actual_id != 0,
                  "palette did not add expected node " + std::to_string(id) + " " + type);
            const auto properties = node.value("properties", nlohmann::json::object());
            const auto descriptor = registry.Find(type);
            Check(descriptor.has_value(), "authoring descriptor missing");
            for (const auto& [key, value] : properties.items()) {
                const auto label = "###property." + std::to_string(actual_id) + "." + key;
                const auto property =
                        std::find_if(descriptor->properties_.begin(), descriptor->properties_.end(),
                                     [&](const auto& item) { return item.key_ == key; });
                Check(property != descriptor->properties_.end(), "recipe property missing: " + key);
                if (!property->choices_.empty()) {
                    ui.Button("###inspector", label);
                    ui.Button(testing::StudioInput::Popup(),
                              "###" + property->choices_.at(value.get<std::size_t>()));
                } else if (value.is_object() && value.contains("asset_sha256")) {
                    ui.Button("###inspector", label);
                    ui.Button(testing::StudioInput::Popup(),
                              "###" + value.at("asset_sha256").get<std::string>());
                } else if (std::holds_alternative<std::string>(property->default_))
                    ui.Text("###inspector", label, value.get<std::string>());
                else if (value.is_string())
                    ui.Color(label, value.get<std::string>());
                else
                    ui.Text("###inspector", label, value.dump());
            }
            if (node.contains("shader_source")) {
                const auto before = studio.Workflow().requested_generation_;
                ui.MultilineText("###inspector", "###shader.source",
                                 node.at("shader_source").get<std::string>());
                ui.Button("###inspector", "###shader.compile");
                for (int attempt = 0; attempt < 300 && studio.Workflow().shader_busy_; ++attempt)
                    frame();
                Check(!studio.Workflow().shader_busy_ && studio.Workflow().shader_error_.empty() &&
                              studio.Workflow().requested_generation_ > before,
                      "visible shader compilation failed: " + studio.Workflow().shader_error_);
            }
            ui.Export("n" + std::to_string(id));
            const auto inputs = node.value("inputs", nlohmann::json::object());
            for (const auto& [port, source] : inputs.items())
                ui.Bind(port, "n" + std::to_string(source.get<std::uint64_t>()));
            std::cout << "Authored " << id << ' ' << type << '\n';
        }
        for (int index = 0; index < 150 && !studio.HasValidPlan(); ++index) frame();
        Check(studio.HasValidPlan() && !studio.Status().budget_limited_,
              "authored graph did not produce a current plan");
        const auto wait_shader = [&] {
            for (int attempt = 0; attempt < 300 && studio.Workflow().shader_busy_; ++attempt)
                frame();
            Check(!studio.Workflow().shader_busy_, "shader task did not finish");
        };
        for (std::size_t index = 0; index < recipe.at("nodes").size(); ++index) {
            const auto& step = recipe.at("nodes")[index];
            if (!step.contains("shader_source")) continue;
            const auto original = step.at("shader_source").get<std::string>();
            ui.FindNode(authored[index], step.at("type").get<std::string>());
            wait_shader();
            Check(ui.ReadMultilineText("###inspector", "###shader.source") == original,
                  "selecting an existing shader must restore its source");
            const auto before = studio.Workflow().requested_generation_;
            ui.MultilineText("###inspector", "###shader.source", "vec2(uv)");
            ui.Button("###inspector", "###shader.compile");
            wait_shader();
            Check(!studio.Workflow().shader_error_.empty() &&
                          studio.Workflow().requested_generation_ == before &&
                          studio.HasValidPlan(),
                  "failed shader must report a diagnostic and retain the previous graph");
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE,
                                    (output / "shader-error").string().c_str());
            ui.Settle(6);
            const auto replacement = "(" + original + ") * 0.7";
            ui.MultilineText("###inspector", "###shader.source", replacement);
            ui.Button("###inspector", "###shader.compile");
            wait_shader();
            for (int attempt = 0; attempt < 150 && !studio.HasValidPlan(); ++attempt) frame();
            Check(studio.Workflow().shader_error_.empty() && studio.HasValidPlan() &&
                          studio.Workflow().requested_generation_ > before,
                  "fixed shader must install a new current graph");
            for (const auto& [button, expected] : std::vector<std::pair<std::string, std::string>>{
                         {"###undo", original}, {"###redo", replacement}, {"###undo", original}}) {
                ui.Button("###graph", button);
                wait_shader();
                Check(ui.ReadMultilineText("###inspector", "###shader.source") == expected,
                      "shader undo/redo must restore the complete source");
            }
            for (int attempt = 0; attempt < 150 && !studio.HasValidPlan(); ++attempt) frame();
            Check(studio.HasValidPlan(), "restored shader graph has no current output");
            std::cout << "Shader error, correction and source undo/redo passed\n";
        }
        ui.Button("###inspector", "###audio.demo");
        for (int index = 0; index < 300 && studio.Status().audio_rms_ < .001f; ++index) frame();
        Check(studio.Status().audio_rms_ >= .001f, "Studio demo has no decoded audio input");
        ui.Button("###inspector", "###audio.input");
        const auto before_music = studio.Workflow().requested_generation_;
        ui.Button("###inspector", "###music.bind");
        for (int index = 0; index < 200 && studio.Workflow().requested_generation_ == before_music;
             ++index)
            frame();
        Check(studio.Workflow().requested_generation_ > before_music,
              "music binding did not complete");
        ui.Settle(6);
        ui.Button("###graph", "###save");
        for (int index = 0;
             index < 80 && project::Load(path).snapshot_.document_.nodes_.size() != id; ++index)
            frame();
        const auto saved = project::Load(path).snapshot_;
        Check(saved.soundtrack_.has_value() && !saved.assets_.empty(),
              "UI-bound demo soundtrack not saved");
        Check(saved.document_.nodes_.size() == id && saved.document_.edges_.empty(),
              "authoring save did not preserve named graph");
        Check(saved.document_.output_ == authored.back(),
              "new output not assigned from empty graph");
        std::size_t expected_bindings = 0;
        for (std::size_t index = 0; index < recipe.at("nodes").size(); ++index) {
            const auto& step = recipe.at("nodes")[index];
            Check(saved.document_.nodes_[index].type_ == step.at("type").get<std::string>(),
                  "saved node type differs from recipe");
            if (step.contains("shader_source")) {
                const auto& asset_id = std::get<assets::AssetId>(
                        saved.document_.nodes_[index].properties_.at("asset"));
                const auto record =
                        std::find_if(saved.assets_.begin(), saved.assets_.end(),
                                     [&](const auto& item) { return item.id_ == asset_id; });
                Check(record != saved.assets_.end(), "compiled source asset not saved");
                const auto bytes = assets::Store(path / "assets").Read(*record);
                const auto program = surface_shader::Decode(std::span<const std::uint8_t>(
                        reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()));
                Check(program.expression_ == step.at("shader_source").get<std::string>(),
                      "saved source differs from the text entered in Studio");
            }
            const auto properties = step.value("properties", nlohmann::json::object());
            for (const auto& [key, value] : properties.items()) {
                if (std::holds_alternative<std::string>(
                            saved.document_.nodes_[index].properties_.at(key))) {
                    Check(std::get<std::string>(saved.document_.nodes_[index].properties_.at(
                                  key)) == value.get<std::string>(),
                          "UTF-8 text input not saved: " + key);
                } else if (value.is_object() && value.contains("asset_sha256")) {
                    Check(std::get<assets::AssetId>(
                                  saved.document_.nodes_[index].properties_.at(key))
                                          .sha256_ == value.at("asset_sha256").get<std::string>(),
                          "font binding not saved");
                } else if (value.is_string()) {
                    const auto hex = std::stoull(value.get<std::string>(), nullptr, 16);
                    const auto color = std::get<graph::Color>(
                            saved.document_.nodes_[index].properties_.at(key));
                    Check(std::abs(color.r_ - double((hex >> 24) & 255) / 255) < .001 &&
                                  std::abs(color.g_ - double((hex >> 16) & 255) / 255) < .001 &&
                                  std::abs(color.b_ - double((hex >> 8) & 255) / 255) < .001 &&
                                  std::abs(color.a_ - double(hex & 255) / 255) < .001,
                          "color input not saved: " + key);
                } else {
                    Check(std::abs(graph::Scalar(saved.document_.nodes_[index], key, 0) -
                                   value.get<double>()) < 1e-5,
                          "property input not saved: " + key);
                }
            }
            const auto name = "n" + std::to_string(index + 1);
            Check(std::count_if(saved.document_.signals_.begin(), saved.document_.signals_.end(),
                                [&](const auto& signal) {
                                    return signal.name_ == name &&
                                           signal.source_ == authored[index];
                                }) == 1,
                  "named output missing: " + name);
            const auto inputs = step.value("inputs", nlohmann::json::object());
            expected_bindings += inputs.size();
            for (const auto& [port, source] : inputs.items()) {
                const auto source_name = "n" + std::to_string(source.get<std::uint64_t>());
                Check(std::count_if(
                              saved.document_.bindings_.begin(), saved.document_.bindings_.end(),
                              [&](const auto& binding) {
                                  return binding.node_ == authored[index] &&
                                         binding.input_ == port && binding.signal_ == source_name;
                              }) == 1,
                      "saved connection missing: " + name + "." + port);
            }
        }
        Check(saved.document_.bindings_.size() == expected_bindings &&
                      saved.document_.signals_.size() == authored.size(),
              "unexpected named connections");
        const auto view_id = authored.at(recipe.at("view_node").get<std::size_t>() - 1);
        const auto view_type = recipe.value("view_type", std::string("texture.affine"));
        ui.FindNode(view_id, view_type);
        Check(studio.Workflow().selected_author_node_ == view_id, "view author not selected");
        ui.Button("###output", "###canvas.edit");
        ui.Focus("###output");
        const auto rect = testing::StudioInput::ImageRect("###output");
        io.AddMousePosEvent(rect.GetCenter().x, rect.GetCenter().y);
        ui.Settle();
        io.AddMouseButtonEvent(0, true);
        frame();
        for (int step = 1; step <= 6; ++step) {
            io.AddMousePosEvent(rect.GetCenter().x + rect.GetWidth() * .1f * step / 6,
                                rect.GetCenter().y);
            frame();
        }
        io.AddMouseButtonEvent(0, false);
        ui.Settle();
        bgfx::requestScreenShot(BGFX_INVALID_HANDLE, (output / "drag").string().c_str());
        ui.Settle(6);
        ui.Button("###graph", "###save");
        ui.Settle(20);
        const auto moved = project::Load(path).snapshot_;
        const auto moved_node =
                std::find_if(moved.document_.nodes_.begin(), moved.document_.nodes_.end(),
                             [&](const auto& node) { return node.id_ == view_id; });
        if (moved_node != moved.document_.nodes_.end())
            std::cout << "Drag translation=" << graph::Scalar(*moved_node, "translate_x", 0)
                      << " selection=" << studio.Workflow().selected_author_node_
                      << " rect=" << rect.Min.x << ',' << rect.Min.y << ',' << rect.GetWidth()
                      << ',' << rect.GetHeight() << '\n';
        Check(moved_node != moved.document_.nodes_.end() &&
                      std::abs(graph::Scalar(*moved_node, "translate_x", 0) -
                               recipe.value("expected_translation", .1)) < .005,
              "from-empty work view drag not saved");
        const bool scene_view = view_type == "scene.transform";
        ui.Button("###output", "###canvas.rotate");
        ui.Focus("###output");
        const auto rotation_rect = testing::StudioInput::ImageRect("###output");
        const ImVec2 pivot{rotation_rect.Min.x + rotation_rect.GetWidth() * .6f,
                           rotation_rect.GetCenter().y};
        constexpr float kAngle = float(std::numbers::pi / 12);
        const auto rotation_start =
                scene_view ? ui.SceneHandle(pivot)
                           : ImVec2{pivot.x, pivot.y - rotation_rect.GetHeight() * .34f};
        const ImVec2 offset{rotation_start.x - pivot.x, rotation_start.y - pivot.y};
        ui.Drag(rotation_start,
                {pivot.x + offset.x * std::cos(kAngle) - offset.y * std::sin(kAngle),
                 pivot.y + offset.x * std::sin(kAngle) + offset.y * std::cos(kAngle)});
        ui.Button("###graph", "###save");
        ui.Settle(20);
        const auto rotated = project::Load(path).snapshot_;
        const auto rotated_node =
                std::find_if(rotated.document_.nodes_.begin(), rotated.document_.nodes_.end(),
                             [&](const auto& node) { return node.id_ == view_id; });
        Check(rotated_node != rotated.document_.nodes_.end(), "rotated author missing");
        const auto rotation =
                scene_view ? std::abs(graph::Scalar(*rotated_node, "rotation_x", 0)) +
                                     std::abs(graph::Scalar(*rotated_node, "rotation_y", 0)) +
                                     std::abs(graph::Scalar(*rotated_node, "rotation_z", 0))
                           : std::abs(graph::Scalar(*rotated_node, "rotation", 0));
        Check(scene_view ? rotation > 1 : std::abs(rotation - 15) < 1,
              "view rotation gesture not saved");
        ui.Button("###output", "###canvas.scale");
        ui.Focus("###output");
        const auto scale_rect = testing::StudioInput::ImageRect("###output");
        const ImVec2 scale_pivot{scale_rect.Min.x + scale_rect.GetWidth() * .6f,
                                 scale_rect.GetCenter().y};
        const auto angle =
                float(graph::Scalar(*rotated_node, "rotation", 0) * std::numbers::pi / 180);
        const ImVec2 corner{-scale_rect.GetWidth() * .425f, scale_rect.GetHeight() * .425f};
        const auto scale_start = scene_view ? ui.SceneHandle(scale_pivot)
                                            : ImVec2{scale_pivot.x + corner.x * std::cos(angle) -
                                                             corner.y * std::sin(angle),
                                                     scale_pivot.y + corner.x * std::sin(angle) +
                                                             corner.y * std::cos(angle)};
        ui.Drag(scale_start, {scale_pivot.x + (scale_start.x - scale_pivot.x) * 1.1f,
                              scale_pivot.y + (scale_start.y - scale_pivot.y) * 1.1f});
        ui.Button("###graph", "###save");
        ui.Settle(20);
        const auto scaled = project::Load(path).snapshot_;
        const auto scaled_node =
                std::find_if(scaled.document_.nodes_.begin(), scaled.document_.nodes_.end(),
                             [&](const auto& node) { return node.id_ == view_id; });
        Check(scaled_node != scaled.document_.nodes_.end(), "scaled author missing");
        const auto scale_delta = std::abs(graph::Scalar(*scaled_node, "scale_x", 1) - 1) +
                                 std::abs(graph::Scalar(*scaled_node, "scale_y", 1) - 1) +
                                 std::abs(graph::Scalar(*scaled_node, "scale_z", 1) - 1);
        Check(scale_delta > .03, "view scale gesture not saved");
        // Gesture checks may deliberately tilt the whole composition. A recipe
        // can restore its intended delivery framing through the same inspector.
        const auto final_view = recipe.value("final_view", nlohmann::json::object());
        for (const auto& [key, value] : final_view.items())
            ui.Text("###inspector", "###property." + std::to_string(view_id) + "." + key,
                    value.dump());
        ui.Button("###inspector", "###component.panel");
        ui.Text("###inspector", "###component.name",
                recipe.value("component_title", std::string("Contour Composition")));
        ui.Button("###inspector", "###component.create");
        ui.Button("###graph", "###save");
        for (int index = 0;
             index < 200 && project::Load(path).snapshot_.document_.components_.empty(); ++index)
            frame();
        const auto component = project::Load(path).snapshot_;
        Check(component.document_.components_.size() == 1 &&
                      component.document_.components_[0].nodes_.size() == 1 &&
                      component.document_.components_[0].nodes_[0].type_ == view_type,
              "authored composition component missing");
        ui.Button("###graph", "###publish");
        const auto package = output / "Published/work.rhythmpack";
        for (int index = 0; index < 150 && !std::filesystem::exists(package); ++index) frame();
        Check(std::filesystem::exists(package), "from-empty publication missing");
        const auto published = project::LoadPackage(package);
        Check(published.soundtrack_ == component.soundtrack_ && !published.assets_.empty(),
              "published work lost its bound music");
        Check(project::EncodeProgram(published.program_) ==
                      project::EncodeProgram(std::get<graph::ExecutionPlan>(
                              graph::Compile(component.document_, registry))),
              "published program differs from authored work");
        const auto before_reopen = studio.Workflow().requested_generation_;
        ui.Button("###graph", "###reopen");
        for (int index = 0;
             index < 150 &&
             (studio.Workflow().requested_generation_ == before_reopen || !studio.HasValidPlan());
             ++index)
            frame();
        Check(studio.HasValidPlan() && studio.Workflow().requested_generation_ > before_reopen,
              "reopened authored work has no new current plan");
        for (std::size_t index = 0; index < recipe.at("nodes").size(); ++index) {
            const auto& step = recipe.at("nodes")[index];
            if (!step.contains("shader_source")) continue;
            ui.FindNode(authored[index], step.at("type").get<std::string>());
            wait_shader();
            Check(ui.ReadMultilineText("###inspector", "###shader.source") ==
                          step.at("shader_source").get<std::string>(),
                  "reopened project must restore saved shader source");
        }
        for (const auto index : recipe.value("inspect_nodes", std::vector<std::size_t>{})) {
            Check(index > 0 && index <= authored.size(), "inspection node out of range");
            const auto& step = recipe.at("nodes").at(index - 1);
            ui.FindNode(authored.at(index - 1), step.at("type").get<std::string>());
            ui.Settle(30);
            Check(studio.Workflow().selected_author_node_ == authored.at(index - 1) &&
                          studio.HasValidPlan(),
                  "reopened inspection did not select its node");
            bgfx::requestScreenShot(
                    BGFX_INVALID_HANDLE,
                    (output / ("inspected-" + std::to_string(index))).string().c_str());
            ui.Settle(6);
        }
        bgfx::requestScreenShot(BGFX_INVALID_HANDLE, (output / "authored").string().c_str());
        ui.Settle(6);
        std::cout
                << "From-empty Studio palette, colors, bound music, named connections, view drag, "
                   "component, save/publish/reopen pass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
