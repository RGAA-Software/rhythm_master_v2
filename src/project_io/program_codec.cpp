#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include <stdexcept>

#include "control_codec.h"
#include "graph.pb.h"
#include "property_codec.h"
#include "rhythm/graph/controls.h"
#include "rhythm/project/package.h"
#include "wire_limits.h"

namespace rhythm::project {
namespace {
void ValidatePlan(const graph::ExecutionPlan& plan) {
    if (graph::ValidatePointBudget(plan)) throw std::invalid_argument("package.point_budget");
    if (graph::ValidateSceneBudget(plan)) throw std::invalid_argument("package.scene_budget");
    if (plan.document_id_.empty() || plan.document_id_.size() > 256 || plan.instructions_.empty() ||
        plan.instructions_.size() > 10000 || plan.output_ >= plan.instructions_.size())
        throw std::invalid_argument("package.program_limits");
    graph::Registry registry;
    graph::Document document;
    document.id_ = plan.document_id_;
    document.revision_ = plan.revision_;
    document.canvas_ = plan.canvas_;
    document.beat_grid_ = plan.beat_grid_;
    document.output_ = plan.instructions_[plan.output_].node_.id_;
    for (const auto& control : plan.controls_.Definitions())
        document.control_titles_[control.id_] = control.title_;
    document.control_snapshots_.assign(plan.controls_.Snapshots().begin(),
                                       plan.controls_.Snapshots().end());
    if (plan.control_sequence_)
        document.control_cues_.assign(plan.control_sequence_->Cues().begin(),
                                      plan.control_sequence_->Cues().end());
    for (const auto& instruction : plan.instructions_) document.nodes_.push_back(instruction.node_);
    for (const auto& instruction : plan.instructions_) {
        const auto descriptor = registry.Find(instruction.node_.type_);
        if (!descriptor || descriptor->operation_ != instruction.operation_ ||
            descriptor->inputs_.size() != instruction.inputs_.size())
            throw std::invalid_argument("package.operator");
        for (std::size_t port = 0; port < instruction.inputs_.size(); ++port) {
            const auto input = instruction.inputs_[port];
            if (!input) continue;
            if (*input >= plan.instructions_.size()) throw std::invalid_argument("package.slot");
            document.edges_.push_back({document.edges_.size() + 1,
                                       plan.instructions_[*input].node_.id_, instruction.node_.id_,
                                       descriptor->inputs_[port].key_});
        }
    }
    // Reuse graph invariants to validate untrusted slot/type/state data at load
    // time. Playback consumes the received plan, without any editor dependency.
    const auto validation = graph::Compile(document, registry);
    if (!std::holds_alternative<graph::ExecutionPlan>(validation))
        throw std::invalid_argument("package.invalid_program");
    const auto& canonical = std::get<graph::ExecutionPlan>(validation);
    if (canonical.controls_ != plan.controls_) throw std::invalid_argument("package.controls");
    if (canonical.control_sequence_ != plan.control_sequence_)
        throw std::invalid_argument("package.cues");
    if (canonical.instructions_.size() != plan.instructions_.size() ||
        canonical.output_ != plan.output_)
        throw std::invalid_argument("package.noncanonical_program");
    for (std::size_t index = 0; index < plan.instructions_.size(); ++index)
        if (canonical.instructions_[index].node_.id_ != plan.instructions_[index].node_.id_ ||
            canonical.instructions_[index].inputs_ != plan.instructions_[index].inputs_)
            throw std::invalid_argument("package.instruction_order");
}
}  // namespace

std::string EncodeProgram(const graph::ExecutionPlan& plan) {
    ValidatePlan(plan);
    schema::CompiledProgram message;
    message.set_abi_version(plan.beat_grid_ ? 4 : plan.control_sequence_ ? 3 : 2);
    if (plan.beat_grid_) detail::EncodeBeatGrid(*plan.beat_grid_, *message.mutable_beat_grid());
    message.mutable_canvas()->set_width(plan.canvas_.width_);
    message.mutable_canvas()->set_height(plan.canvas_.height_);
    message.set_document_id(plan.document_id_);
    message.set_revision(plan.revision_);
    message.set_output_slot(plan.output_);
    if (!plan.controls_.Definitions().empty()) {
        graph::Document metadata;
        if (plan.control_sequence_)
            metadata.control_cues_.assign(plan.control_sequence_->Cues().begin(),
                                          plan.control_sequence_->Cues().end());
        for (const auto& control : plan.controls_.Definitions())
            metadata.control_titles_[control.id_] = control.title_;
        metadata.control_snapshots_.assign(plan.controls_.Snapshots().begin(),
                                           plan.controls_.Snapshots().end());
        detail::EncodeControls(metadata, *message.mutable_controls());
    }
    for (const auto& instruction : plan.instructions_) {
        auto& encoded = *message.add_instructions();
        encoded.set_operator_type(instruction.node_.type_);
        encoded.set_source_node(instruction.node_.id_);
        for (const auto slot : instruction.inputs_) encoded.add_input_slots(slot ? *slot + 1 : 0);
        auto& node = *encoded.mutable_configuration();
        node.set_id(instruction.node_.id_);
        node.set_type_key(instruction.node_.type_);
        node.set_schema_version(instruction.node_.version_);
        for (const auto& [key, value] : instruction.node_.properties_) {
            auto& property = (*node.mutable_properties())[key];
            detail::EncodeProperty(value, property);
        }
    }
    if (message.ByteSizeLong() > kMaximumProgramBytes)
        throw std::length_error("package.program_bytes");
    std::string bytes;
    google::protobuf::io::StringOutputStream sink(&bytes);
    {
        google::protobuf::io::CodedOutputStream output(&sink);
        output.SetSerializationDeterministic(true);
        if (!message.SerializeToCodedStream(&output)) throw std::runtime_error("package.serialize");
    }
    return bytes;
}

graph::ExecutionPlan DecodeProgram(std::string_view bytes, std::uint32_t required_abi) {
    if (bytes.size() > kMaximumProgramBytes) throw std::length_error("package.program_bytes");
    detail::CheckWireLimits(bytes, detail::WireRoot::kProgram);
    schema::CompiledProgram message;
    google::protobuf::io::ArrayInputStream source(bytes.data(), static_cast<int>(bytes.size()));
    google::protobuf::io::CodedInputStream input(&source);
    input.SetRecursionLimit(32);
    input.SetTotalBytesLimit(static_cast<int>(kMaximumProgramBytes));
    if (!message.ParseFromCodedStream(&input) || !input.ConsumedEntireMessage() ||
        (message.abi_version() < 1 || message.abi_version() > 4) ||
        (required_abi != 0 && message.abi_version() != required_abi))
        throw std::invalid_argument("package.abi");
    if ((message.abi_version() >= 2) != message.has_canvas())
        throw std::invalid_argument("package.canvas_abi");
    graph::ExecutionPlan plan;
    if ((message.abi_version() == 4) != message.has_beat_grid())
        throw std::invalid_argument("package.beat_abi");
    if (message.has_beat_grid()) plan.beat_grid_ = detail::DecodeBeatGrid(message.beat_grid());
    plan.document_id_ = message.document_id();
    plan.revision_ = message.revision();
    if (message.has_canvas()) plan.canvas_ = {message.canvas().width(), message.canvas().height()};
    if (message.instructions_size() > 10000) throw std::length_error("package.instruction_limit");
    if (message.output_slot() >= static_cast<std::uint64_t>(message.instructions_size()))
        throw std::invalid_argument("package.output_slot");
    plan.output_ = static_cast<std::size_t>(message.output_slot());
    graph::Registry registry;
    for (const auto& record : message.instructions()) {
        const auto& config = record.configuration();
        const auto descriptor = registry.Find(record.operator_type());
        if (!descriptor ||
            static_cast<std::size_t>(record.input_slots_size()) > descriptor->inputs_.size() ||
            config.properties_size() > 128 || config.schema_version() != 1 ||
            config.id() != record.source_node() || config.type_key() != record.operator_type())
            throw std::invalid_argument("package.operator");
        graph::Instruction instruction;
        instruction.operation_ = descriptor->operation_;
        instruction.node_.id_ = config.id();
        instruction.node_.type_ = config.type_key();
        instruction.node_.version_ = config.schema_version();
        for (const auto slot : record.input_slots()) {
            if (slot > static_cast<std::uint64_t>(message.instructions_size()))
                throw std::invalid_argument("package.slot");
            instruction.inputs_.push_back(slot ? std::optional<std::size_t>(slot - 1)
                                               : std::nullopt);
        }
        // An appended optional port does not invalidate packages published by an
        // earlier version of the same operator. Required ports still must exist.
        for (std::size_t port = instruction.inputs_.size(); port < descriptor->inputs_.size();
             ++port) {
            if (descriptor->inputs_[port].required_)
                throw std::invalid_argument("package.operator");
            instruction.inputs_.push_back(std::nullopt);
        }
        for (const auto& [key, property] : config.properties())
            instruction.node_.properties_[key] = detail::DecodeProperty(property, false);
        plan.instructions_.push_back(std::move(instruction));
    }
    graph::Document metadata;
    detail::DecodeControls(message.controls(), metadata);
    for (const auto& instruction : plan.instructions_) metadata.nodes_.push_back(instruction.node_);
    plan.controls_ = graph::DescribeControls(metadata);
    if (message.abi_version() < 4 &&
        ((message.abi_version() == 3) != !metadata.control_cues_.empty()))
        throw std::invalid_argument("package.cue_abi");
    if (!metadata.control_cues_.empty())
        plan.control_sequence_.emplace(plan.controls_, metadata.control_cues_);
    ValidatePlan(plan);
    return plan;
}
}  // namespace rhythm::project
