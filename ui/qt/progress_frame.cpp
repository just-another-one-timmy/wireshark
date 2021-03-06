/* progress_frame.cpp
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include "progress_frame.h"
#include <ui_progress_frame.h>

#include "ui/progress_dlg.h"

#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

#include "stock_icon.h"
#include "wireshark_application.h"

// To do:
// - Use a different icon?
// - Add an NSProgressIndicator to the dock icon on OS X.
// - Start adding the progress bar to dialogs.
// - Don't complain so loudly when the user stops a capture.

progdlg_t *create_progress_dlg(const gpointer top_level_window, const gchar *, const gchar *,
                               gboolean terminate_is_stop, gboolean *stop_flag) {
    ProgressFrame *pf;
    QWidget *main_window;

    if (!top_level_window) {
        return NULL;
    }

    main_window = qobject_cast<QWidget *>((QObject *)top_level_window);

    if (!main_window) {
        return NULL;
    }

    pf = main_window->findChild<ProgressFrame *>();

    if (!pf) {
        return NULL;
    }
    return pf->showProgress(true, terminate_is_stop, stop_flag, 0);
}

progdlg_t *
delayed_create_progress_dlg(const gpointer top_level_window, const gchar *task_title, const gchar *item_title,
                            gboolean terminate_is_stop, gboolean *stop_flag,
                            const GTimeVal *, gfloat progress)
{
    progdlg_t *progress_dialog = create_progress_dlg(top_level_window, task_title, item_title, terminate_is_stop, stop_flag);
    update_progress_dlg(progress_dialog, progress, item_title);
    return progress_dialog;
}

/*
 * Update the progress information of the progress bar box.
 */
void
update_progress_dlg(progdlg_t *dlg, gfloat percentage, const gchar *)
{
    if (!dlg) return;

    dlg->progress_frame->setValue(percentage * 100);

    /*
     * Flush out the update and process any input events.
     */
    WiresharkApplication::processEvents();
}

/*
 * Destroy the progress bar.
 */
void
destroy_progress_dlg(progdlg_t *dlg)
{
    dlg->progress_frame->hide();
}

ProgressFrame::ProgressFrame(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::ProgressFrame)
  , terminate_is_stop_(false)
  , stop_flag_(NULL)
#if !defined(Q_OS_MAC) || QT_VERSION > QT_VERSION_CHECK(5, 0, 0)
  , show_timer_(-1)
#endif
#ifdef QWINTASKBARPROGRESS_H
  , taskbar_progress_(NULL)
#endif
{
    ui->setupUi(this);

    progress_dialog_.progress_frame = this;
    progress_dialog_.top_level_window = window();

    ui->progressBar->setStyleSheet(QString(
            "QProgressBar {"
            "  max-width: 20em;"
            "  min-height: 0.8em;"
            "  max-height: 1em;"
            "  border-bottom: 0px;"
            "  border-top: 0px;"
            "  background: transparent;"
            "}"));

    int one_em = fontMetrics().height();
    ui->pushButton->setIconSize(QSize(one_em, one_em));
    ui->pushButton->setStyleSheet(QString(
            "QPushButton {"
            "  image: url(:/dfilter/dfilter_erase_normal.png) center;"
            "  min-height: 0.8em;"
            "  max-height: 1em;"
            "  min-width: 0.8em;"
            "  max-width: 1em;"
            "  border: 0px;"
            "  padding: 0px;"
            "  margin: 0px;"
            "  background: transparent;"
            "}"
            "QPushButton:hover {"
            "  image: url(:/dfilter/dfilter_erase_active.png) center;"
            "}"
            "QPushButton:pressed {"
            "  image: url(:/dfilter/dfilter_erase_selected.png) center;"
            "}"));
    hide();
}

ProgressFrame::~ProgressFrame()
{
    delete ui;
}

struct progdlg *ProgressFrame::showProgress(bool animate, bool terminate_is_stop, gboolean *stop_flag, int value)
{
    ui->progressBar->setMaximum(100);
    ui->progressBar->setValue(value);
    return show(animate, terminate_is_stop, stop_flag);
}

progdlg *ProgressFrame::showBusy(bool animate, bool terminate_is_stop, gboolean *stop_flag)
{
    ui->progressBar->setMaximum(0);
    return show(animate, terminate_is_stop, stop_flag);
}

void ProgressFrame::setValue(int value)
{
    ui->progressBar->setValue(value);
}

#if !defined(Q_OS_MAC) || QT_VERSION > QT_VERSION_CHECK(5, 0, 0)
void ProgressFrame::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == show_timer_) {
        killTimer(show_timer_);
        show_timer_ = -1;

        QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(this);
        this->setGraphicsEffect(effect);

        QPropertyAnimation *animation = new QPropertyAnimation(effect, "opacity");

        animation->setDuration(750);
        animation->setStartValue(0.1);
        animation->setEndValue(1.0);
        animation->setEasingCurve(QEasingCurve::InOutQuad);
        animation->start();

        QFrame::show();
    }
}
#endif

void ProgressFrame::hide()
{
#if !defined(Q_OS_MAC) || QT_VERSION > QT_VERSION_CHECK(5, 0, 0)
    show_timer_ = -1;
#endif
    QFrame::hide();
#ifdef QWINTASKBARPROGRESS_H
    if (taskbar_progress_) {
        taskbar_progress_->reset();
        taskbar_progress_->hide();
    }
#endif
}

void ProgressFrame::on_pushButton_clicked()
{
    emit stopLoading();
}

const int show_delay_ = 500; // ms
progdlg *ProgressFrame::show(bool animate, bool terminate_is_stop, gboolean *stop_flag)
{
    terminate_is_stop_ = terminate_is_stop;
    stop_flag_ = stop_flag;

    if (stop_flag) {
        ui->pushButton->show();
    } else {
        ui->pushButton->hide();
    }

#if !defined(Q_OS_MAC) || QT_VERSION > QT_VERSION_CHECK(5, 0, 0)
    if (animate) {
        show_timer_ = startTimer(show_delay_);
    } else {
        QFrame::show();
    }
#else
    Q_UNUSED(animate);
    QFrame::show();
#endif

#ifdef QWINTASKBARPROGRESS_H
    // windowHandle() is picky about returning a non-NULL value so we check it
    // each time.
    if (!taskbar_progress_ && window()->windowHandle()) {
        QWinTaskbarButton *taskbar_button = new QWinTaskbarButton(this);
        if (taskbar_button) {
            taskbar_button->setWindow(window()->windowHandle());
            taskbar_progress_ = taskbar_button->progress();
            connect(this, SIGNAL(valueChanged(int)), taskbar_progress_, SLOT(setValue(int)));
        }
    }
    if (taskbar_progress_) {
        taskbar_progress_->show();
    }
    taskbar_progress_->resume();
#endif

    return &progress_dialog_;
}

/*
 * Editor modelines
 *
 * Local Variables:
 * c-basic-offset: 4
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 *
 * ex: set shiftwidth=4 tabstop=8 expandtab:
 * :indentSize=4:tabSize=8:noTabs=true:
 */
