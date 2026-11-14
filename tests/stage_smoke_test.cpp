#include "skeleton_passthrough_stage.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

// What the host hands a job: here, no inputs at all.
class EmptyJobContext final : public orc::IToolJobContext {
public:
    const std::vector<orc::ArtifactPtr>& inputs() const override { return inputs_; }
    void report_progress(double, const std::string&) override {}
    bool is_cancelled() const override { return false; }

private:
    std::vector<orc::ArtifactPtr> inputs_;
};

} // namespace

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

    // The note parameter: declared, stored, and anything else refused
    // without changing what is stored.
    const auto descriptors = stage.get_parameter_descriptors();
    if (descriptors.size() != 1 || descriptors[0].name != orc::plugins::skeleton::kNoteParameter ||
        descriptors[0].type != orc::ParameterType::STRING) {
        std::cerr << "Expected one STRING parameter named note\n";
        return 1;
    }
    if (!stage.set_parameters({{orc::plugins::skeleton::kNoteParameter, std::string("kept")}})) {
        std::cerr << "Expected the note to be accepted\n";
        return 1;
    }
    if (stage.set_parameters({{orc::plugins::skeleton::kNoteParameter, std::string("other")},
                              {"no_such_parameter", std::string("x")}})) {
        std::cerr << "Expected an unknown parameter to be refused\n";
        return 1;
    }
    const auto stored = stage.get_parameters();
    if (std::get<std::string>(stored.at(orc::plugins::skeleton::kNoteParameter)) != "kept") {
        std::cerr << "A refused set_parameters() must leave the note alone\n";
        return 1;
    }

    // The measure job refuses, with a reason, a node with nothing to read,
    // and a verb it does not know.
    EmptyJobContext context;
    const auto measured = stage.run_tool_job(orc::plugins::skeleton::kInspectorContractId,
                                             orc::plugins::skeleton::kInspectorMeasureVerb, {},
                                             context);
    if (measured.ok || measured.error.empty()) {
        std::cerr << "Expected the job to refuse a node without frames\n";
        return 1;
    }
    const auto unknown_job = stage.run_tool_job(orc::plugins::skeleton::kInspectorContractId,
                                                "no-such-job", {}, context);
    if (unknown_job.ok || unknown_job.error.empty()) {
        std::cerr << "Expected an unknown job to be refused with a reason\n";
        return 1;
    }

    return 0;
}
