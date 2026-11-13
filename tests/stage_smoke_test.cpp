#include "skeleton_passthrough_stage.h"

#include <iostream>

int main()
{
    orc::plugins::skeleton::SkeletonPassthroughStage stage;

    const auto info = stage.get_node_type_info();
    if (info.stage_name != "skeleton_passthrough") {
        std::cerr << "Unexpected stage_name: " << info.stage_name << '\n';
        return 1;
    }

    if (info.type != orc::NodeType::TRANSFORM) {
        std::cerr << "Expected TRANSFORM node type\n";
        return 1;
    }

    // ABI 12: the Add Stage menu category is derived from the node type by
    // the SDK, never declared by the plugin.
    if (info.category() != orc::StageCategory::TRANSFORM) {
        std::cerr << "Expected the derived stage category to be Transform\n";
        return 1;
    }

    if (stage.required_input_count() != 1 || stage.output_count() != 1) {
        std::cerr << "Expected one input and one output\n";
        return 1;
    }

    // Before execute() has cached any frame data the preview capability
    // must report is_valid() == false (Decode-Orc 2.x preview contract).
    if (stage.get_preview_capability().is_valid()) {
        std::cerr << "Preview capability must be invalid before execute()\n";
        return 1;
    }

    // The Inspector tool is a plugin view, served by this plugin's view
    // binary under the contract id the stage answers on.
    const auto tools = stage.get_stage_tools();
    if (tools.size() != 1 || tools[0].kind != orc::StageToolKind::PluginView ||
        tools[0].contract_id != orc::plugins::skeleton::kInspectorContractId) {
        std::cerr << "Expected one PluginView tool on the Inspector contract\n";
        return 1;
    }

    // Before execute() the stage still answers, saying there is nothing yet.
    const auto summary = stage.handle_tool_request(
        orc::plugins::skeleton::kInspectorContractId,
        orc::plugins::skeleton::kInspectorSummaryVerb, {});
    if (!summary.ok || summary.data.empty()) {
        std::cerr << "Expected a summary before execute(): " << summary.error << '\n';
        return 1;
    }

    // A verb it does not know is refused with a reason, never ignored.
    const auto unknown = stage.handle_tool_request(
        orc::plugins::skeleton::kInspectorContractId, "no-such-verb", {});
    if (unknown.ok || unknown.error.empty()) {
        std::cerr << "Expected an unknown verb to be refused with a reason\n";
        return 1;
    }

    return 0;
}
