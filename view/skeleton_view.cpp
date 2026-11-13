/*
 * File:        skeleton_view.cpp
 * Module:      orc-view_skeleton
 * Purpose:     The skeleton's view binary: the Inspector window, which asks
 *              the passthrough stage what it last forwarded
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 <Your Name>
 */

// A view binary is a separate shared library that links Qt (Core, Gui and
// Widgets only) and the <orc/view/...> SDK headers. orc-gui loads it; orc-cli
// never does. It reaches its stage only through the IStageToolSession it is
// opened with: never include a stage header or a host header here.

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <orc/view/orc_view_registration.h>

#include <memory>
#include <string>
#include <utility>

namespace {

// Shared with the stage (src/skeleton_passthrough_stage.h). Kept as literals
// rather than an include: the view must not depend on stage headers.
constexpr const char* kInspectorContractId = "org.decodeorc.skeleton.inspector.v1";
constexpr const char* kInspectorSummaryVerb = "summary";

class InspectorDialog final : public QDialog {
public:
    InspectorDialog(std::shared_ptr<orc::view::IStageToolSession> session, QWidget* parent)
        : QDialog(parent), session_(std::move(session))
    {
        setWindowTitle(QString::fromStdString(session_->title()));
        auto* layout = new QVBoxLayout(this);
        summary_ = new QLabel(this);
        summary_->setWordWrap(true);
        summary_->setMinimumWidth(360);
        layout->addWidget(summary_);
        auto* refresh = new QPushButton(tr("Refresh"), this);
        layout->addWidget(refresh);
        connect(refresh, &QPushButton::clicked, this, [this] { ask(); });

        // The host calls this after the node is triggered again.
        session_->set_results_changed_handler([this] { ask(); });
        ask();
    }

private:
    void ask()
    {
        summary_->setText(tr("Asking the stage..."));
        // The reply arrives later on the GUI thread, in request order. None
        // arrives once this dialog is gone, so capturing `this` is safe.
        session_->request(kInspectorSummaryVerb, {}, [this](orc::view::ToolReply reply) {
            if (!reply.ok) {
                summary_->setText(QString::fromStdString(reply.error));
                return;
            }
            summary_->setText(QString::fromUtf8(reinterpret_cast<const char*>(reply.data.data()),
                                                static_cast<int>(reply.data.size())));
        });
    }

    std::shared_ptr<orc::view::IStageToolSession> session_;
    QLabel* summary_ = nullptr;
};

QWidget* make_inspector(const orc::view::ViewOpenRequest& request)
{
    return new InspectorDialog(request.session, request.parent);
}

constexpr orc::view::ViewPluginDescriptor kViewDescriptor =
    ORC_VIEW_PLUGIN_DESCRIPTOR("org.decodeorc.stage.skeleton_passthrough", ORC_VIEW_PLUGIN_VERSION);

} // namespace

ORC_DEFINE_VIEW_PLUGIN(kViewDescriptor, {kInspectorContractId, &make_inspector})
