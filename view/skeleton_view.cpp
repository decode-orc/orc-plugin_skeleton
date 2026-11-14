/*
 * File:        skeleton_view.cpp
 * Module:      orc-view_skeleton
 * Purpose:     The skeleton's view binary: the Inspector window, which asks
 *              the passthrough stage what it last forwarded, runs a job in
 *              it and writes the result back to the node
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 <Your Name>
 */

// A view binary is a separate shared library that links Qt (Core, Gui and
// Widgets only) and the <orc/view/...> SDK headers. orc-gui loads it; orc-cli
// never does. It reaches its stage only through the IStageToolSession it is
// opened with: never include a stage header or a host header here.

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
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
constexpr const char* kInspectorMeasureVerb = "measure";
constexpr const char* kNoteParameter = "note";

QString text_of(const orc::view::ToolReply& reply)
{
    return QString::fromUtf8(reinterpret_cast<const char*>(reply.data.data()),
                             static_cast<int>(reply.data.size()));
}

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

        // The measurement: a job, with progress and Cancel.
        progress_ = new QProgressBar(this);
        progress_->setRange(0, 1000);
        progress_->setTextVisible(false);
        layout->addWidget(progress_);
        status_ = new QLabel(this);
        status_->setWordWrap(true);
        layout->addWidget(status_);
        auto* buttons = new QHBoxLayout;
        measure_ = new QPushButton(tr("Measure"), this);
        cancel_ = new QPushButton(tr("Cancel"), this);
        keep_ = new QPushButton(tr("Keep as Note"), this);
        buttons->addWidget(measure_);
        buttons->addWidget(cancel_);
        buttons->addWidget(keep_);
        layout->addLayout(buttons);
        connect(measure_, &QPushButton::clicked, this, [this] { measure(); });
        connect(cancel_, &QPushButton::clicked, this, [this] {
            if (job_) {
                job_->cancel();
            }
        });
        connect(keep_, &QPushButton::clicked, this, [this] { keep(); });
        setRunning(false);
        keep_->setEnabled(false);

        // The host calls this after the node is triggered again.
        session_->set_results_changed_handler([this] { ask(); });
        // Writing the note rebuilds the graph, which replaces the stage
        // object. Without this handler the host would close the window then;
        // with it the window stays and asks the new stage object.
        session_->set_graph_rebuilt_handler([this] { ask(); });
        ask();
    }

private:
    void ask()
    {
        summary_->setText(tr("Asking the stage..."));
        // The reply arrives later on the GUI thread, in request order. None
        // arrives once this dialog is gone, so capturing `this` is safe.
        session_->request(kInspectorSummaryVerb, {}, [this](orc::view::ToolReply reply) {
            summary_->setText(reply.ok ? text_of(reply) : QString::fromStdString(reply.error));
        });
    }

    void measure()
    {
        // The job runs on a job worker, so previews and requests carry on.
        // Progress always arrives before the reply; closing the window
        // cancels the job.
        job_ = session_->run_job(
            kInspectorMeasureVerb, {},
            [this](orc::view::ToolJobProgress progress) {
                if (progress.fraction >= 0.0) {
                    progress_->setValue(static_cast<int>(progress.fraction * 1000.0));
                }
                status_->setText(QString::fromStdString(progress.text));
            },
            [this](orc::view::ToolReply reply) {
                setRunning(false);
                progress_->setValue(reply.ok ? progress_->maximum() : 0);
                measured_ = reply.ok ? text_of(reply) : QString();
                status_->setText(reply.ok ? measured_ : QString::fromStdString(reply.error));
                keep_->setEnabled(reply.ok);
            });
        if (!job_) {
            status_->setText(tr("A measurement is already running."));
            return;
        }
        setRunning(true);
        keep_->setEnabled(false);
        status_->setText(tr("Measuring..."));
    }

    void keep()
    {
        // The same edit as the node's parameter editor: values are text, as
        // on the orc-cli command line, and the host checks them against the
        // parameters the stage declares.
        session_->apply_parameters({{kNoteParameter, measured_.toStdString()}},
                                   "Keep measurement as note",
                                   [this](orc::view::ToolReply reply) {
                                       if (!reply.ok) {
                                           status_->setText(QString::fromStdString(reply.error));
                                       }
                                   });
        keep_->setEnabled(false);
    }

    void setRunning(bool running)
    {
        measure_->setEnabled(!running);
        cancel_->setEnabled(running);
    }

    std::shared_ptr<orc::view::IStageToolSession> session_;
    std::shared_ptr<orc::view::IToolJob> job_;
    QString measured_;
    QLabel* summary_ = nullptr;
    QLabel* status_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QPushButton* measure_ = nullptr;
    QPushButton* cancel_ = nullptr;
    QPushButton* keep_ = nullptr;
};

QWidget* make_inspector(const orc::view::ViewOpenRequest& request)
{
    return new InspectorDialog(request.session, request.parent);
}

constexpr orc::view::ViewPluginDescriptor kViewDescriptor =
    ORC_VIEW_PLUGIN_DESCRIPTOR("org.decodeorc.stage.skeleton_passthrough", ORC_VIEW_PLUGIN_VERSION);

} // namespace

ORC_DEFINE_VIEW_PLUGIN(kViewDescriptor, {kInspectorContractId, &make_inspector})
