#pragma once

#include <fstream>
#include <iomanip>
#include <optional>

#include "rhythm/studio/studio.h"

namespace rhythm::testing {
// A test-owned journal survives stack unwinding and export workspace cleanup.
// Only changed status/actions are written; every record is flushed immediately.
class WorkflowEvidence final {
   public:
    explicit WorkflowEvidence(const std::filesystem::path& root) {
        std::filesystem::create_directories(root);
        file_.exceptions(std::ios::failbit | std::ios::badbit);
        file_.open(root / "workflow.log", std::ios::out | std::ios::app);
    }
    void Record(const std::string& action, const studio::Studio& studio) {
        const auto status = studio.Workflow();
        if (last_ == status && action_ == action) return;
        file_ << "action=" << std::quoted(action) << " requested=" << status.requested_generation_
              << " installed=" << status.installed_generation_ << " valid=" << studio.HasValidPlan()
              << " nodes=" << studio.Status().authored_nodes_
              << " budget=" << studio.Status().budget_limited_ << " export=" << status.export_state_
              << " phase=" << status.export_phase_ << " frames=" << status.exported_frames_ << '/'
              << status.export_total_frames_ << " error=" << std::quoted(status.export_error_)
              << " shader_busy=" << status.shader_busy_
              << " shader_error=" << std::quoted(status.shader_error_);
        for (const auto& error : status.graph_errors_)
            file_ << " graph_error=" << std::quoted(error);
        file_ << '\n';
        file_.flush();
        last_ = status;
        action_ = action;
    }

   private:
    std::ofstream file_{};
    std::optional<studio::WorkflowStatus> last_{};
    std::string action_{};
};
}  // namespace rhythm::testing
