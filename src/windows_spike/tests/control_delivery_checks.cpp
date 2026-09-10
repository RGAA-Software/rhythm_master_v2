#include "control_delivery_checks.h"

#include <bgfx/bgfx.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <cmath>
#include <iostream>
#include <stdexcept>

#include "rhythm/graph/controls.h"
#include "rhythm/project/package.h"
#include "rhythm/project/store.h"

namespace rhythm::testing {
namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void Activate(const char* item) {
    // Borrowed ImGui windows never leave this synchronous native test adapter.
    const auto* window = ImGui::FindWindowByName("###graph");
    Require(window != nullptr, "control delivery graph window missing");
    ImGui::ActivateItemByID(ImHashStr(item, 0, window->ID));
}
ImVec2 SliderPosition(std::size_t index) {
    // Child geometry comes from the actual layout, including parent scrolling.
    const auto* inspector = ImGui::FindWindowByName("###inspector");
    Require(inspector != nullptr, "control delivery inspector missing");
    for (const auto* window : ImGui::GetCurrentContext()->Windows) {
        if (window->ParentWindow != inspector || !window->WasActive ||
            std::string_view(window->Name).find("sliders") == std::string_view::npos)
            continue;
        const ImVec2 point(window->DC.CursorStartPos.x + 20,
                           window->DC.CursorStartPos.y +
                                   static_cast<float>(index) * ImGui::GetFrameHeightWithSpacing() +
                                   ImGui::GetFrameHeight() / 2);
        Require(window->ClipRect.Contains(point), "control delivery slider is clipped");
        return point;
    }
    throw std::runtime_error("control delivery sliders missing");
}
}  // namespace
void CheckControlDelivery(studio::Studio& studio, const std::filesystem::path& project,
                          const std::filesystem::path& package,
                          const std::filesystem::path& captures,
                          const std::function<void()>& frame) {
    const auto original = project::Load(project).snapshot_;
    const auto bank = graph::DescribeControls(original.document_);
    Require(!bank.Definitions().empty(), "calibration work has no public controls");
    for (std::size_t index = 0; index < bank.Definitions().size(); ++index) {
        const auto control = bank.Definitions()[index];
        for (const bool maximum : {false, true}) {
            const auto expected = maximum ? control.maximum_ : control.minimum_;
            const auto before = studio.Workflow().requested_generation_;
            const auto point = SliderPosition(index);
            auto& io = ImGui::GetIO();
            io.AddMousePosEvent(point.x, point.y);
            frame();
            io.AddMouseButtonEvent(0, true);
            frame();
            Require(ImGui::GetActiveID() != 0, "control delivery mouse did not activate slider");
            io.AddMousePosEvent(point.x + (maximum ? 1000 : -1000), point.y);
            frame();
            io.AddMouseButtonEvent(0, false);
            frame();
            bool saved = false;
            for (int step = 0; step < 300 && !saved; ++step) {
                if (step % 20 == 0 && studio.HasValidPlan()) Activate("###save");
                frame();
                if (!studio.HasValidPlan() || studio.Workflow().requested_generation_ <= before)
                    continue;
                const auto current =
                        graph::DescribeControls(project::Load(project).snapshot_.document_);
                saved = std::abs(current.Resolve().at(control.id_) - expected) < 0.00001;
            }
            Require(saved, "slider extreme did not reach current compiled and saved graph");
            Require(!studio.Status().budget_limited_, "control extreme exceeded render budget");
            const auto capture = captures.string() + "-" + std::to_string(index) +
                                 (maximum ? "-maximum" : "-minimum");
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, capture.c_str());
            for (int settle = 0; settle < 3; ++settle) frame();
            const auto reopen_generation = studio.Workflow().requested_generation_;
            Activate("###reopen");
            bool reopened = false;
            for (int step = 0; step < 300 && !reopened; ++step) {
                frame();
                reopened = studio.HasValidPlan() &&
                           studio.Workflow().requested_generation_ > reopen_generation;
            }
            Require(reopened, "edited control graph did not reopen with a current plan");
            bool published = false;
            for (int step = 0; step < 300 && !published; ++step) {
                if (step % 20 == 0) Activate("###publish");
                frame();
                const auto current = project::LoadPackage(package);
                published = std::abs(current.program_.controls_.Resolve().at(control.id_) -
                                     expected) < 0.00001;
            }
            Require(published, "reopened control default did not reach publication");
            const auto published_work = project::LoadPackage(package);
            for (const auto& instruction : published_work.program_.instructions_) {
                if (instruction.operation_ != graph::Operation::kMotionPhase ||
                    !instruction.inputs_[0] ||
                    published_work.program_.instructions_[*instruction.inputs_[0]].node_.id_ !=
                            control.id_)
                    continue;
                for (const double seconds : {0.0, 4.0, 10.0, 15.0})
                    Require(std::abs(parameters::EvaluateControls(
                                             published_work.program_.controls_,
                                             published_work.program_.control_sequence_, seconds)
                                             .at(control.id_) -
                                     expected) < 0.00001,
                            "published Cue overrides the saved independent motion pace");
            }
            std::cout << "control " << control.id_ << " = " << expected
                      << ": mouse/current plan/save/reopen/publish passed" << std::endl;
        }
    }
}
}  // namespace rhythm::testing
