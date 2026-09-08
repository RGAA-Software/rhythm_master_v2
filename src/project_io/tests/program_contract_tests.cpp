#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "graph.pb.h"
#include "rhythm/project/package.h"
#include "rhythm/runtime/runtime.h"

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void Reject(std::string_view bytes) {
    bool rejected = false;
    try {
        rhythm::project::DecodeProgram(bytes);
    } catch (const std::exception&) {
        rejected = true;
    }
    Check(rejected, "Untrusted invalid program was accepted");
}
}  // namespace

int main() {
    using namespace rhythm;
    try {
        graph::Registry registry;
        for (const auto type : {"scene.point_light", "scene.spot_light"}) {
            graph::Document document;
            document.id_ = "local-light-publication";
            document.nodes_ = {registry.MakeNode(1, type), registry.MakeNode(2, "scene.render"),
                               registry.MakeNode(3, "output.texture")};
            document.nodes_[0].properties_["light_range"] = 27.0;
            document.edges_ = {{1, 1, 2, "scene"}, {2, 2, 3, "source"}};
            document.output_ = 3;
            const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
            const auto restored = project::DecodeProgram(project::EncodeProgram(plan));
            auto renderer = render::Renderer::CreateNull();
            runtime::Runtime runtime;
            renderer.BeginFrame();
            const auto frame = runtime.Evaluate(restored, {0, 0, {16, 16}}, renderer);
            renderer.EndFrame();
            const auto& light = frame.outputs_[0].scene_->positional_lights_.at(0);
            Check(light.range_ == 27 &&
                          light.spot_ == (std::string_view(type) == "scene.spot_light"),
                  "local light type and properties survive publication and playback");
        }
        {
            graph::Document gpu;
            gpu.id_ = "gpu-program";
            gpu.nodes_ = {registry.MakeNode(1, "gpu.particles"), registry.MakeNode(2, "gpu.render"),
                          registry.MakeNode(3, "output.texture")};
            gpu.edges_ = {{1, 1, 2, "points"}, {2, 2, 3, "source"}};
            gpu.output_ = 3;
            const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(gpu, registry));
            const auto restored = project::DecodeProgram(project::EncodeProgram(plan));
            Check(restored.instructions_[0].operation_ == graph::Operation::kGpuParticleEmitter &&
                          restored.instructions_[1].operation_ == graph::Operation::kGpuPointRender,
                  "GPU types survive program publication");
            schema::CompiledProgram wrong;
            Check(wrong.ParseFromString(project::EncodeProgram(plan)), "GPU program");
            wrong.mutable_instructions(1)->set_operator_type("point.render");
            wrong.mutable_instructions(1)->mutable_configuration()->set_type_key("point.render");
            Reject(wrong.SerializeAsString());
        }
        {
            graph::Document scene;
            scene.id_ = "legacy-scene-scale";
            scene.nodes_ = {
                    registry.MakeNode(1, "geometry.cube"), registry.MakeNode(2, "scene.instance"),
                    registry.MakeNode(3, "scene.transform"), registry.MakeNode(4, "scene.render"),
                    registry.MakeNode(5, "output.texture")};
            scene.nodes_[2].properties_["scale"] = 2.0;
            for (const auto key : {"scale_x", "scale_y", "scale_z"})
                scene.nodes_[2].properties_.erase(key);
            scene.edges_ = {{1, 1, 2, "geometry"},
                            {2, 2, 3, "scene"},
                            {3, 3, 4, "scene"},
                            {4, 4, 5, "source"}};
            scene.output_ = 5;
            const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(scene, registry));
            schema::CompiledProgram legacy;
            Check(legacy.ParseFromString(project::EncodeProgram(plan)), "Scene program");
            auto& slots = *legacy.mutable_instructions(2)->mutable_input_slots();
            while (slots.size() > 8) slots.RemoveLast();
            const auto restored = project::DecodeProgram(legacy.SerializeAsString());
            Check(restored.instructions_[2].inputs_.size() == 11 &&
                          !restored.instructions_[2].inputs_[8] &&
                          !restored.instructions_[2].inputs_[9] &&
                          !restored.instructions_[2].inputs_[10],
                  "Legacy scene gains three neutral optional axis inputs");
            auto renderer = render::Renderer::CreateNull();
            runtime::Runtime runtime;
            renderer.BeginFrame();
            const auto frame = runtime.Evaluate(restored, {0, 0, {16, 16}}, renderer);
            renderer.EndFrame();
            Check(frame.outputs_[2].scene_->instances_[0].transform_ ==
                          scene::Compose({}, {}, {2, 2, 2}),
                  "Legacy scene with absent axis properties preserves uniform scaling");
        }
        {
            graph::Document particles;
            particles.id_ = "legacy-particle-inputs";
            particles.nodes_ = {registry.MakeNode(1, "point.emitter"),
                                registry.MakeNode(2, "point.render"),
                                registry.MakeNode(3, "output.texture")};
            particles.edges_ = {{1, 1, 2, "points"}, {2, 2, 3, "source"}};
            particles.output_ = 3;
            const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(particles, registry));
            schema::CompiledProgram legacy;
            Check(legacy.ParseFromString(project::EncodeProgram(plan)), "Particle program");
            legacy.mutable_instructions(0)->mutable_input_slots()->RemoveLast();
            const auto restored = project::DecodeProgram(legacy.SerializeAsString());
            Check(restored.instructions_[0].inputs_.size() == 3 &&
                          !restored.instructions_[0].inputs_[2],
                  "Legacy emitter gains neutral optional flow input");
            legacy.mutable_instructions(1)->clear_input_slots();
            Reject(legacy.SerializeAsString());
        }
        graph::Document document;
        document.id_ = "program.roundtrip";
        document.revision_ = 17;
        document.output_ = 8;
        document.canvas_ = {720, 1280};
        document.nodes_ = {registry.MakeNode(1, "core.time"),
                           registry.MakeNode(2, "signal.oscillator"),
                           registry.MakeNode(3, "signal.sample"),
                           registry.MakeNode(4, "texture.gradient"),
                           registry.MakeNode(5, "texture.transform"),
                           registry.MakeNode(6, "texture.feedback"),
                           registry.MakeNode(7, "texture.blend"),
                           registry.MakeNode(8, "output.texture"),
                           registry.MakeNode(9, "time.local"),
                           registry.MakeNode(10, "scalar.constant"),
                           registry.MakeNode(11, "scalar.math"),
                           registry.MakeNode(12, "scalar.curve")};
        document.nodes_[1].properties_["frequency"] = 2.0;
        document.edges_ = {{1, 12, 2, "time"},  {2, 2, 3, "signal"}, {3, 11, 4, "amount"},
                           {4, 4, 7, "a"},      {5, 6, 5, "source"}, {6, 5, 7, "b"},
                           {7, 7, 6, "source"}, {8, 7, 8, "source"}, {9, 1, 9, "time"},
                           {10, 3, 11, "a"},    {11, 10, 11, "b"},   {12, 9, 12, "time"}};
        document.nodes_[11].properties_["curve"] =
                parameters::Curve({{0, 0.2, parameters::Interpolation::kSmooth}, {1, 1}});
        document.nodes_[8].properties_["time_mode"] = 1.0;
        document.nodes_[10].properties_["math_mode"] = 2.0;
        document.nodes_.push_back(registry.MakeNode(13, "signal.noise"));
        document.nodes_.push_back(registry.MakeNode(14, "signal.sample"));
        document.nodes_.push_back(registry.MakeNode(15, "scalar.map"));
        document.nodes_.push_back(registry.MakeNode(16, "scalar.compare"));
        document.nodes_.push_back(registry.MakeNode(17, "scalar.select"));
        document.nodes_.push_back(registry.MakeNode(18, "session.time"));
        document.nodes_.push_back(registry.MakeNode(19, "participant.role"));
        document.nodes_.push_back(registry.MakeNode(20, "participant.control"));
        document.nodes_.push_back(registry.MakeNode(21, "scalar.math"));
        document.nodes_.push_back(registry.MakeNode(22, "scalar.math"));
        document.edges_[8].from_ = 22;
        document.edges_[11].from_ = 21;
        document.edges_[2].from_ = 17;
        document.edges_.insert(document.edges_.end(), {{13, 1, 13, "time"},
                                                       {14, 13, 14, "signal"},
                                                       {15, 14, 15, "value"},
                                                       {16, 15, 16, "a"},
                                                       {17, 16, 17, "condition"},
                                                       {18, 11, 17, "b"}});
        document.edges_.insert(
                document.edges_.end(),
                {{19, 18, 21, "a"}, {20, 9, 21, "b"}, {21, 19, 22, "a"}, {22, 20, 22, "b"}});
        document.nodes_.push_back(registry.MakeNode(23, "scalar.expression"));
        document.nodes_.back().properties_["expression"] =
                parameters::Expression("a + sin(time) * 0.1");
        document.edges_[2].from_ = 23;
        document.edges_.insert(document.edges_.end(), {{23, 17, 23, "a"}, {24, 1, 23, "time"}});
        const auto compiled = graph::Compile(document, registry);
        Check(std::holds_alternative<graph::ExecutionPlan>(compiled), "Fixture compile");
        const auto& original = std::get<graph::ExecutionPlan>(compiled);
        const auto bytes = project::EncodeProgram(original);
        const auto loaded = project::DecodeProgram(bytes);
        Check(project::EncodeProgram(loaded) == bytes,
              "Program serialization must be deterministic");
        Check(loaded.document_id_ == original.document_id_ && loaded.revision_ == 17 &&
                      loaded.canvas_ == document.canvas_ &&
                      loaded.instructions_.size() == original.instructions_.size(),
              "Program identity and instruction count");
        for (std::size_t index = 0; index < loaded.instructions_.size(); ++index) {
            const auto& first = original.instructions_[index];
            const auto& second = loaded.instructions_[index];
            Check(first.node_ == second.node_ && first.operation_ == second.operation_ &&
                          first.inputs_ == second.inputs_,
                  "Typed slot and configuration parity");
        }
        auto first_renderer = render::Renderer::CreateNull();
        auto second_renderer = render::Renderer::CreateNull();
        runtime::Runtime first_runtime;
        runtime::Runtime second_runtime;
        for (int frame = 0; frame < 60; ++frame) {
            first_renderer.BeginFrame();
            second_renderer.BeginFrame();
            const runtime::FrameContext context{frame / 60.0, 0, {256, 144}};
            const auto first = first_runtime.Evaluate(original, context, first_renderer);
            const auto second = second_runtime.Evaluate(loaded, context, second_renderer);
            Check(first.evaluated_ == second.evaluated_ &&
                          first.outputs_.size() == second.outputs_.size(),
                  "Compiled and loaded execution parity");
            for (std::size_t index = 0; index < first.outputs_.size(); ++index)
                Check(first.outputs_[index].node_ == second.outputs_[index].node_ &&
                              first.outputs_[index].version_ == second.outputs_[index].version_ &&
                              std::abs(first.outputs_[index].scalar_ -
                                       second.outputs_[index].scalar_) < 1e-12,
                      "Signal and dirty-state parity");
            Check(first_renderer.Stats().passes_ == second_renderer.Stats().passes_,
                  "Render pass parity");
            first_renderer.EndFrame();
            second_renderer.EndFrame();
        }
        schema::CompiledProgram message;
        Check(message.ParseFromString(bytes), "Encoded protobuf");
        auto legacy = message;
        legacy.set_abi_version(1);
        legacy.clear_canvas();
        Check(project::DecodeProgram(legacy.SerializeAsString()).canvas_ == graph::Canvas{},
              "Legacy program defaults to landscape");
        auto bad = message;
        bad.clear_canvas();
        Reject(bad.SerializeAsString());
        bad = message;
        bad.mutable_canvas()->set_width(0);
        Reject(bad.SerializeAsString());
        bad = message;
        bad.set_abi_version(99);
        Reject(bad.SerializeAsString());
        bad = message;
        bad.set_output_slot(9999);
        Reject(bad.SerializeAsString());
        bad = message;
        bad.mutable_instructions(0)->set_source_node(0);
        Reject(bad.SerializeAsString());
        bad = message;
        for (auto& instruction : *bad.mutable_instructions()) {
            if (instruction.operator_type() == "signal.oscillator") {
                instruction.set_input_slots(0, bad.instructions_size() + 1);
                break;
            }
        }
        Reject(bad.SerializeAsString());
        bad = message;
        for (auto& instruction : *bad.mutable_instructions())
            if (instruction.operator_type() == "signal.oscillator")
                (*instruction.mutable_configuration()->mutable_properties())["frequency"]
                        .set_scalar(std::numeric_limits<double>::quiet_NaN());
        Reject(bad.SerializeAsString());
        bad = message;
        bad.mutable_instructions()->SwapElements(0, bad.instructions_size() - 1);
        Reject(bad.SerializeAsString());
        Reject(bytes.substr(0, bytes.size() / 2));
        Reject(std::string(project::kMaximumProgramBytes + 1, 'x'));
        std::cout << "program contracts passed: ABI, typed slots, feedback, execution parity, "
                     "invalid input\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
