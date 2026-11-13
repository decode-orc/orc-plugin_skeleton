/*
 * File:        skeleton_passthrough_stage.h
 * Module:      orc-stage-plugin-skeleton-passthrough
 * Purpose:     Passthrough stage that forwards one input artifact unchanged
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 <Your Name>
 */

#pragma once

#include <orc/abi/orc_plugin_sdk.h>
#include <orc/plugin/orc_stage_tooling.h>
#include <orc/stage/tooling/stage_tool_requests.h>

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace orc::plugins::skeleton {

// The Inspector tool: a window drawn by this plugin's view binary
// (view/skeleton_view.cpp), opened from the stage's Stage Tools menu in
// orc-gui. The view and the stage agree on the contract id and on the verbs
// and bytes they exchange; the host passes them through untouched.
inline constexpr const char* kInspectorContractId =
    "org.decodeorc.skeleton.inspector.v1";
// The one verb: the reply is UTF-8 text describing what the stage last saw.
inline constexpr const char* kInspectorSummaryVerb = "summary";

// Minimal passthrough transform for the Decode-Orc 2.x plugin SDK
// (host ABI 20 / plugin API 4). Demonstrates the two contracts every
// preview-capable transform implements:
//
// - DAGStage::execute() over frame-based artifacts: inputs carrying video
//   data are VideoFrameRepresentation artifacts in the CVBS_U10_4FSC domain.
// - IStagePreviewCapability: the structured preview contract that replaced
//   the legacy PreviewableStage interface in Decode-Orc 2.0. The stage only
//   declares what data it can supply (data types, navigable range, active
//   picture geometry); all image rendering is performed by the host.
//
// It also shows a plugin view: StageToolProvider advertises the Inspector
// tool, and IStageToolRequestHandler answers the view's requests.
class SkeletonPassthroughStage : public DAGStage,
                                 public IStagePreviewCapability,
                                 public StageToolProvider,
                                 public IStageToolRequestHandler {
public:
    std::string version() const override;
    NodeTypeInfo get_node_type_info() const override;

    // Implements DAGStage::get_instructions() (the stage help shown by the
    // host) by reading the instructions.md that orc_add_stage_plugin() copies
    // alongside the plugin shared library. Returns "" if the file is missing.
    ORC_STAGE_INSTRUCTIONS_MD

    std::vector<ArtifactPtr> execute(
        const std::vector<ArtifactPtr>& inputs,
        const std::map<std::string, ParameterValue>& parameters,
        ObservationContext& observation_context) override;

    size_t required_input_count() const override;
    size_t output_count() const override;

    // IStagePreviewCapability. Returns an is_valid() == false capability
    // until execute() has cached a frame representation.
    StagePreviewCapability get_preview_capability() const override;

    // StageToolProvider: the Inspector tool (StageToolKind::PluginView).
    std::vector<StageToolDescriptor> get_stage_tools() const override;

    // IStageToolRequestHandler: called on the host's render worker with the
    // view's requests. Answers from what execute() last cached; never runs
    // anything itself.
    StageToolReply handle_tool_request(const std::string& contract_id,
                                       const std::string& verb,
                                       const std::vector<uint8_t>& payload) override;

private:
    mutable std::shared_ptr<const VideoFrameRepresentation> cached_output_;
};

} // namespace orc::plugins::skeleton
