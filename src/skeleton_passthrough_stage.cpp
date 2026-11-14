/*
 * File:        skeleton_passthrough_stage.cpp
 * Module:      orc-stage-plugin-skeleton-passthrough
 * Purpose:     Passthrough stage that forwards one input artifact unchanged
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 <Your Name>
 */

#include "skeleton_passthrough_stage.h"

#include <cstdint>
#include <variant>

namespace orc::plugins::skeleton {

std::string SkeletonPassthroughStage::version() const
{
    return "1.0.0";
}

NodeTypeInfo SkeletonPassthroughStage::get_node_type_info() const
{
    return NodeTypeInfo{
        NodeType::TRANSFORM, "skeleton_passthrough", "Skeleton Passthrough",
        "Minimal external plugin stage that forwards one input artifact.",
        1, 1, 1, 1, VideoFormatCompatibility::ALL
    };
}

std::vector<ArtifactPtr> SkeletonPassthroughStage::execute(
    const std::vector<ArtifactPtr>& inputs,
    const std::map<std::string, ParameterValue>&,
    ObservationContext&)
{
    if (inputs.empty()) {
        cached_output_.reset();
        return {};
    }

    // A passthrough forwards the artifact unchanged; caching it as a
    // VideoFrameRepresentation is what enables the preview capability below.
    // Forwarding the input pointer itself means the host already sees one
    // representation, so its frame-content-keyed analysis is shared for free.
    //
    // Stages that modify sample data instead wrap the input in a
    // VideoFrameRepresentationWrapper subclass and forward that. Since host
    // ABI 11 such a wrapper should override
    // VideoFrameRepresentation::video_passthrough_source(FrameID) to return
    // the wrapped source for any frame whose CVBS video content it leaves
    // byte-identical (a no-op correction pass, or a stage that only appends
    // audio); the host then reuses the upstream frame's stored observations
    // instead of re-analysing identical samples. Returning nullptr — the
    // default — is always safe, and the answer must come from metadata alone.
    cached_output_ = std::dynamic_pointer_cast<const VideoFrameRepresentation>(inputs.at(0));
    return {inputs.at(0)};
}

std::vector<ParameterDescriptor> SkeletonPassthroughStage::get_parameter_descriptors(
    VideoSystem, SourceType) const
{
    ParameterDescriptor note;
    note.name = kNoteParameter;
    note.display_name = "Note";
    note.description =
        "Free text kept with the node. The stage does nothing with it; the "
        "Skeleton Inspector can write its last measurement here.";
    note.type = ParameterType::STRING;
    note.constraints.default_value = std::string{};
    return {note};
}

std::map<std::string, ParameterValue> SkeletonPassthroughStage::get_parameters() const
{
    return {{kNoteParameter, note_}};
}

bool SkeletonPassthroughStage::set_parameters(
    const std::map<std::string, ParameterValue>& params)
{
    // Check everything before changing anything, so a refused call leaves
    // the stage as it was.
    std::string note = note_;
    for (const auto& [name, value] : params) {
        if (name != kNoteParameter || !std::holds_alternative<std::string>(value)) {
            return false;
        }
        note = std::get<std::string>(value);
    }
    note_ = note;
    return true;
}

size_t SkeletonPassthroughStage::required_input_count() const
{
    return 1;
}

size_t SkeletonPassthroughStage::output_count() const
{
    return 1;
}

StagePreviewCapability SkeletonPassthroughStage::get_preview_capability() const
{
    // Plugins linked against the installed SDK package can return
    // PreviewHelpers::make_signal_preview_capability(cached_output_) instead.
    // The capability is composed by hand here so this skeleton also builds in
    // the header-only in-tree CI configuration, which links no host libraries.
    StagePreviewCapability capability{};
    if (!cached_output_ || cached_output_->frame_count() == 0) {
        return capability;
    }

    const auto params = cached_output_->get_video_parameters();
    if (!params.has_value() || !params->is_valid()) {
        return capability;
    }

    const bool is_yc = cached_output_->has_separate_channels();
    VideoDataType data_type;
    if (params->system == VideoSystem::NTSC || params->system == VideoSystem::PAL_M) {
        data_type = is_yc ? VideoDataType::YC_NTSC : VideoDataType::CompositeNTSC;
    } else {
        data_type = is_yc ? VideoDataType::YC_PAL : VideoDataType::CompositePAL;
    }

    const auto active_width =
        params->active_video_end > params->active_video_start
            ? static_cast<uint32_t>(params->active_video_end - params->active_video_start)
            : static_cast<uint32_t>(params->frame_width_nominal);
    const auto active_height =
        params->last_active_frame_line > params->first_active_frame_line
            ? static_cast<uint32_t>(params->last_active_frame_line - params->first_active_frame_line)
            : static_cast<uint32_t>(params->frame_height);

    double dar_correction = 1.0;
    if (active_width > 0 && active_height > 0) {
        const double active_ratio =
            static_cast<double>(active_width) / static_cast<double>(active_height);
        dar_correction = (4.0 / 3.0) / active_ratio;
    }

    capability.supported_data_types = {data_type};
    capability.navigation_extent.item_count = cached_output_->frame_count();
    capability.navigation_extent.granularity = 1;
    capability.navigation_extent.item_label = "frame";
    capability.geometry.active_width = active_width;
    capability.geometry.active_height = active_height;
    capability.geometry.display_aspect_ratio = 4.0 / 3.0;
    capability.geometry.dar_correction_factor = dar_correction;
    return capability;
}

std::vector<StageToolDescriptor> SkeletonPassthroughStage::get_stage_tools() const
{
    StageToolDescriptor inspector;
    inspector.tool_id = "skeleton_inspector";
    inspector.display_name = "Skeleton Inspector";
    inspector.description = "Shows what the passthrough last forwarded, in the plugin's own window";
    inspector.kind = StageToolKind::PluginView;
    inspector.contract_id = kInspectorContractId;
    return {inspector};
}

StageToolReply SkeletonPassthroughStage::handle_tool_request(
    const std::string& contract_id, const std::string& verb,
    const std::vector<uint8_t>&)
{
    StageToolReply reply;
    if (contract_id != kInspectorContractId || verb != kInspectorSummaryVerb) {
        reply.error = "Unknown request '" + verb + "' for " + contract_id;
        return reply;
    }

    std::string text;
    if (!cached_output_) {
        text = "Nothing forwarded yet. Preview the node, or a node after it, "
               "so the stage runs.";
    } else {
        const FrameIDRange range = cached_output_->frame_range();
        text = "Forwarding " + std::to_string(cached_output_->frame_count()) +
               " frames (" + std::to_string(range.first) + " to " +
               std::to_string(range.last) + ").";
    }
    if (!note_.empty()) {
        text += "\nNote: " + note_;
    }
    reply.data.assign(text.begin(), text.end());
    reply.ok = true;
    return reply;
}

StageToolReply SkeletonPassthroughStage::run_tool_job(
    const std::string& contract_id, const std::string& verb,
    const std::vector<uint8_t>&, IToolJobContext& context)
{
    StageToolReply reply;
    if (contract_id != kInspectorContractId || verb != kInspectorMeasureVerb) {
        reply.error = "Unknown job '" + verb + "' for " + contract_id;
        return reply;
    }

    // The inputs are what execute() would receive. This object is not the
    // one previews use, so cached_output_ is not set here: read the input.
    const auto& inputs = context.inputs();
    const auto video = inputs.empty()
                           ? nullptr
                           : std::dynamic_pointer_cast<const VideoFrameRepresentation>(inputs[0]);
    if (!video || video->frame_count() == 0) {
        reply.error = "The node's input carries no frames to measure.";
        return reply;
    }

    const FrameIDRange range = video->frame_range();
    const uint64_t total = range.count();
    uint64_t dropouts = 0;
    uint64_t frames_with_dropouts = 0;
    for (FrameID id = range.first; id <= range.last; ++id) {
        // Check at least once per frame. Returning a refusal once cancelled
        // is enough: the host replies "Cancelled" in its place.
        if (context.is_cancelled()) {
            reply.error = "Cancelled";
            return reply;
        }
        const uint64_t done = id - range.first;
        context.report_progress(static_cast<double>(done) / static_cast<double>(total),
                                "Measuring frame " + std::to_string(done + 1) + " of " +
                                    std::to_string(total));
        if (video->has_frame(id)) {
            const size_t runs = video->get_dropout_hints(id).size();
            dropouts += runs;
            frames_with_dropouts += runs > 0 ? 1 : 0;
        }
        if (id == range.last) {
            break;  // FrameID is unsigned: never step past the last frame
        }
    }

    const std::string text = std::to_string(dropouts) + " dropouts on " +
                             std::to_string(frames_with_dropouts) + " of " +
                             std::to_string(total) + " frames";
    reply.data.assign(text.begin(), text.end());
    reply.ok = true;
    return reply;
}

} // namespace orc::plugins::skeleton
