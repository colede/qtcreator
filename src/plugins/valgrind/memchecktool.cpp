/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
** Author: Nicolas Arnaud-Cormos, KDAB (nicolas.arnaud-cormos@kdab.com)
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "memchecktool.h"
#include "memcheckengine.h"
#include "memcheckerrorview.h"
#include "valgrindsettings.h"
#include "valgrindplugin.h"

#include <analyzerbase/analyzermanager.h>
#include <analyzerbase/analyzerconstants.h>

#include <valgrind/valgrindsettings.h>
#include <valgrind/xmlprotocol/errorlistmodel.h>
#include <valgrind/xmlprotocol/stackmodel.h>
#include <valgrind/xmlprotocol/error.h>
#include <valgrind/xmlprotocol/frame.h>
#include <valgrind/xmlprotocol/stack.h>
#include <valgrind/xmlprotocol/suppression.h>

#include <extensionsystem/iplugin.h>
#include <extensionsystem/pluginmanager.h>

#include <projectexplorer/deploymentdata.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/project.h>
#include <projectexplorer/runconfiguration.h>
#include <projectexplorer/target.h>
#include <projectexplorer/session.h>
#include <projectexplorer/buildconfiguration.h>

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/icore.h>
#include <coreplugin/id.h>

#include <utils/fancymainwindow.h>
#include <utils/styledbar.h>
#include <utils/qtcassert.h>

#include <QString>
#include <QLatin1String>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QDir>

#include <QDockWidget>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QSpinBox>
#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QToolButton>
#include <QCheckBox>
#include <utils/stylehelper.h>

using namespace Analyzer;
using namespace ProjectExplorer;
using namespace Valgrind::XmlProtocol;

namespace Valgrind {
namespace Internal {

// ---------------------------- MemcheckErrorFilterProxyModel
MemcheckErrorFilterProxyModel::MemcheckErrorFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent),
      m_filterExternalIssues(false)
{
}

void MemcheckErrorFilterProxyModel::setAcceptedKinds(const QList<int> &acceptedKinds)
{
    if (m_acceptedKinds != acceptedKinds) {
        m_acceptedKinds = acceptedKinds;
        invalidate();
    }
}

void MemcheckErrorFilterProxyModel::setFilterExternalIssues(bool filter)
{
    if (m_filterExternalIssues != filter) {
        m_filterExternalIssues = filter;
        invalidate();
    }
}

bool MemcheckErrorFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    // We only deal with toplevel items.
    if (sourceParent.isValid())
        return true;

    // Because toplevel items have no parent, we can't use sourceParent to find them. we just use
    // sourceParent as an invalid index, telling the model that the index we're looking for has no
    // parent.
    QAbstractItemModel *model = sourceModel();
    QModelIndex sourceIndex = model->index(sourceRow, filterKeyColumn(), sourceParent);
    if (!sourceIndex.isValid())
        return true;

    const Error error = sourceIndex.data(ErrorListModel::ErrorRole).value<Error>();

    // Filter on kind
    if (!m_acceptedKinds.contains(error.kind()))
        return false;

    // Filter non-project stuff
    if (m_filterExternalIssues && !error.stacks().isEmpty()) {
        // ALGORITHM: look at last five stack frames, if none of these is inside any open projects,
        // assume this error was created by an external library
        QSet<QString> validFolders;
        foreach (Project *project, SessionManager::projects()) {
            validFolders << project->projectDirectory().toString();
            foreach (Target *target, project->targets()) {
                foreach (const ProjectExplorer::DeployableFile &file,
                         target->deploymentData().allFiles()) {
                    if (file.isExecutable())
                        validFolders << file.remoteDirectory();
                }
                foreach (BuildConfiguration *config, target->buildConfigurations())
                    validFolders << config->buildDirectory().toString();
            }
        }

        const QVector< Frame > frames = error.stacks().first().frames();

        const int framesToLookAt = qMin(6, frames.size());

        bool inProject = false;
        for (int i = 0; i < framesToLookAt; ++i) {
            const Frame &frame = frames.at(i);
            foreach (const QString &folder, validFolders) {
                if (frame.object().startsWith(folder)) {
                    inProject = true;
                    break;
                }
            }
        }
        if (!inProject)
            return false;
    }

    return true;
}

static void initKindFilterAction(QAction *action, const QList<int> &kinds)
{
    action->setCheckable(true);
    QVariantList data;
    foreach (int kind, kinds)
        data << kind;
    action->setData(data);
}

MemcheckTool::MemcheckTool(QObject *parent)
  : ValgrindTool(parent)
{
    m_settings = 0;
    m_errorModel = 0;
    m_errorProxyModel = 0;
    m_errorView = 0;
    m_filterMenu = 0;
    setObjectName(QLatin1String("MemcheckTool"));

    m_filterProjectAction = new QAction(tr("External Errors"), this);
    m_filterProjectAction->setToolTip(
        tr("Show issues originating outside currently opened projects."));
    m_filterProjectAction->setCheckable(true);

    m_suppressionSeparator = new QAction(tr("Suppressions"), this);
    m_suppressionSeparator->setSeparator(true);
    m_suppressionSeparator->setToolTip(
        tr("These suppression files were used in the last memory analyzer run."));

    QAction *a = new QAction(tr("Definite Memory Leaks"), this);
    initKindFilterAction(a, QList<int>() << Leak_DefinitelyLost << Leak_IndirectlyLost);
    m_errorFilterActions.append(a);

    a = new QAction(tr("Possible Memory Leaks"), this);
    initKindFilterAction(a, QList<int>() << Leak_PossiblyLost << Leak_StillReachable);
    m_errorFilterActions.append(a);

    a = new QAction(tr("Use of Uninitialized Memory"), this);
    initKindFilterAction(a, QList<int>() << InvalidRead << InvalidWrite << InvalidJump << Overlap
                         << InvalidMemPool << UninitCondition << UninitValue
                         << SyscallParam << ClientCheck);
    m_errorFilterActions.append(a);

    a = new QAction(tr("Invalid Calls to \"free()\""), this);
    initKindFilterAction(a, QList<int>() << InvalidFree << MismatchedFree);
    m_errorFilterActions.append(a);
}

void MemcheckTool::settingsDestroyed(QObject *settings)
{
    QTC_ASSERT(m_settings == settings, return);
    m_settings = ValgrindPlugin::globalSettings();
}

void MemcheckTool::updateFromSettings()
{
    foreach (QAction *action, m_errorFilterActions) {
        bool contained = true;
        foreach (const QVariant &v, action->data().toList()) {
            bool ok;
            int kind = v.toInt(&ok);
            if (ok && !m_settings->visibleErrorKinds().contains(kind))
                contained = false;
        }
        action->setChecked(contained);
    }

    m_filterProjectAction->setChecked(!m_settings->filterExternalIssues());
    m_errorView->settingsChanged(m_settings);

    connect(m_settings, SIGNAL(visibleErrorKindsChanged(QList<int>)),
            m_errorProxyModel, SLOT(setAcceptedKinds(QList<int>)));
    m_errorProxyModel->setAcceptedKinds(m_settings->visibleErrorKinds());

    connect(m_settings, SIGNAL(filterExternalIssuesChanged(bool)),
            m_errorProxyModel, SLOT(setFilterExternalIssues(bool)));
    m_errorProxyModel->setFilterExternalIssues(m_settings->filterExternalIssues());
}

void MemcheckTool::maybeActiveRunConfigurationChanged()
{
    ValgrindBaseSettings *settings = 0;
    if (Project *project = SessionManager::startupProject())
        if (Target *target = project->activeTarget())
            if (RunConfiguration *rc = target->activeRunConfiguration())
                if (IRunConfigurationAspect *aspect = rc->extraAspect(ANALYZER_VALGRIND_SETTINGS))
                    settings = qobject_cast<ValgrindBaseSettings *>(aspect->currentSettings());

    if (!settings) // fallback to global settings
        settings = ValgrindPlugin::globalSettings();

    if (m_settings == settings)
        return;

    // disconnect old settings class if any
    if (m_settings) {
        m_settings->disconnect(this);
        m_settings->disconnect(m_errorProxyModel);
    }

    // now make the new settings current, update and connect input widgets
    m_settings = settings;
    QTC_ASSERT(m_settings, return);
    connect(m_settings, SIGNAL(destroyed(QObject*)), SLOT(settingsDestroyed(QObject*)));

    updateFromSettings();
}

RunMode MemcheckTool::runMode() const
{
    return MemcheckRunMode;
}

IAnalyzerTool::ToolMode MemcheckTool::toolMode() const
{
    return DebugMode;
}

class FrameFinder : public ErrorListModel::RelevantFrameFinder
{
public:
    Frame findRelevant(const Error &error) const
    {
        const QVector<Stack> stacks = error.stacks();
        if (stacks.isEmpty())
            return Frame();
        const Stack &stack = stacks[0];
        const QVector<Frame> frames = stack.frames();
        if (frames.isEmpty())
            return Frame();

        //find the first frame belonging to the project
        if (!m_projectFiles.isEmpty()) {
            foreach (const Frame &frame, frames) {
                if (frame.directory().isEmpty() || frame.file().isEmpty())
                    continue;

                //filepaths can contain "..", clean them:
                const QString f = QFileInfo(frame.directory() + QLatin1Char('/') + frame.file()).absoluteFilePath();
                if (m_projectFiles.contains(f))
                    return frame;
            }
        }

        //if no frame belonging to the project was found, return the first one that is not malloc/new
        foreach (const Frame &frame, frames) {
            if (!frame.functionName().isEmpty() && frame.functionName() != QLatin1String("malloc")
                && !frame.functionName().startsWith(QLatin1String("operator new(")))
            {
                return frame;
            }
        }

        //else fallback to the first frame
        return frames.first();
    }
    void setFiles(const QStringList &files)
    {
        m_projectFiles = files;
    }
private:
    QStringList m_projectFiles;
};


QWidget *MemcheckTool::createWidgets()
{
    QTC_ASSERT(!m_errorView, return 0);

    Utils::FancyMainWindow *mw = AnalyzerManager::mainWindow();

    m_errorView = new MemcheckErrorView;
    m_errorView->setObjectName(QLatin1String("MemcheckErrorView"));
    m_errorView->setFrameStyle(QFrame::NoFrame);
    m_errorView->setAttribute(Qt::WA_MacShowFocusRect, false);
    m_errorModel = new ErrorListModel(m_errorView);
    m_frameFinder = new Internal::FrameFinder;
    m_errorModel->setRelevantFrameFinder(QSharedPointer<Internal::FrameFinder>(m_frameFinder));
    m_errorProxyModel = new MemcheckErrorFilterProxyModel(m_errorView);
    m_errorProxyModel->setSourceModel(m_errorModel);
    m_errorProxyModel->setDynamicSortFilter(true);
    m_errorView->setModel(m_errorProxyModel);
    m_errorView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // make m_errorView->selectionModel()->selectedRows() return something
    m_errorView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_errorView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_errorView->setAutoScroll(false);
    m_errorView->setObjectName(QLatin1String("Valgrind.MemcheckTool.ErrorView"));

    QDockWidget *errorDock = AnalyzerManager::createDockWidget
        (this, tr("Memory Issues"), m_errorView, Qt::BottomDockWidgetArea);
    errorDock->show();
    mw->splitDockWidget(mw->toolBarDockWidget(), errorDock, Qt::Vertical);

    connect(ProjectExplorerPlugin::instance(),
            SIGNAL(updateRunActions()), SLOT(maybeActiveRunConfigurationChanged()));

    //
    // The Control Widget.
    //
    QAction *action = 0;
    QHBoxLayout *layout = new QHBoxLayout;
    QToolButton *button = 0;

    layout->setMargin(0);
    layout->setSpacing(0);

    // Load external XML log file
    action = new QAction(this);
    action->setIcon(QIcon(QLatin1String(Core::Constants::ICON_OPENFILE)));
    action->setToolTip(tr("Load External XML Log File"));
    connect(action, SIGNAL(triggered(bool)), this, SLOT(loadExternalXmlLogFile()));
    button = new QToolButton;
    button->setDefaultAction(action);
    layout->addWidget(button);
    m_loadExternalLogFile = action;

    // Go to previous leak.
    action = new QAction(this);
    action->setDisabled(true);
    action->setIcon(QIcon(QLatin1String(Core::Constants::ICON_PREV)));
    action->setToolTip(tr("Go to previous leak."));
    connect(action, SIGNAL(triggered(bool)), m_errorView, SLOT(goBack()));
    button = new QToolButton;
    button->setDefaultAction(action);
    layout->addWidget(button);
    m_goBack = action;

    // Go to next leak.
    action = new QAction(this);
    action->setDisabled(true);
    action->setIcon(QIcon(QLatin1String(Core::Constants::ICON_NEXT)));
    action->setToolTip(tr("Go to next leak."));
    connect(action, SIGNAL(triggered(bool)), m_errorView, SLOT(goNext()));
    button = new QToolButton;
    button->setDefaultAction(action);
    layout->addWidget(button);
    m_goNext = action;

    QToolButton *filterButton = new QToolButton;
    filterButton->setIcon(QIcon(QLatin1String(Core::Constants::ICON_FILTER)));
    filterButton->setText(tr("Error Filter"));
    filterButton->setPopupMode(QToolButton::InstantPopup);

    m_filterMenu = new QMenu(filterButton);
    foreach (QAction *filterAction, m_errorFilterActions)
        m_filterMenu->addAction(filterAction);
    m_filterMenu->addSeparator();
    m_filterMenu->addAction(m_filterProjectAction);
    m_filterMenu->addAction(m_suppressionSeparator);
    connect(m_filterMenu, SIGNAL(triggered(QAction*)), SLOT(updateErrorFilter()));
    filterButton->setMenu(m_filterMenu);
    layout->addWidget(filterButton);

    layout->addStretch();
    QWidget *widget = new QWidget;
    widget->setObjectName(QLatin1String("MemCheckToolBarWidget"));
    widget->setLayout(layout);
    return widget;
}

AnalyzerRunControl *MemcheckTool::createRunControl(const AnalyzerStartParameters &sp,
                                            RunConfiguration *runConfiguration)
{
    m_frameFinder->setFiles(runConfiguration ? runConfiguration->target()
        ->project()->files(Project::AllFiles) : QStringList());

    MemcheckRunControl *engine = new MemcheckRunControl(sp, runConfiguration);

    connect(engine, SIGNAL(starting(const Analyzer::AnalyzerRunControl*)),
            this, SLOT(engineStarting(const Analyzer::AnalyzerRunControl*)));
    connect(engine, SIGNAL(parserError(Valgrind::XmlProtocol::Error)),
            this, SLOT(parserError(Valgrind::XmlProtocol::Error)));
    connect(engine, SIGNAL(internalParserError(QString)),
            this, SLOT(internalParserError(QString)));
    connect(engine, SIGNAL(finished()), this, SLOT(engineFinished()));
    return engine;
}

void MemcheckTool::engineStarting(const AnalyzerRunControl *engine)
{
    setBusyCursor(true);
    clearErrorView();
    m_loadExternalLogFile->setDisabled(true);

    QString dir;
    if (RunConfiguration *rc = engine->runConfiguration())
        dir = rc->target()->project()->projectDirectory().toString() + QDir::separator();

    const MemcheckRunControl *mEngine = dynamic_cast<const MemcheckRunControl *>(engine);
    QTC_ASSERT(mEngine, return);
    const QString name = QFileInfo(mEngine->executable()).fileName();

    m_errorView->setDefaultSuppressionFile(dir + name + QLatin1String(".supp"));

    foreach (const QString &file, mEngine->suppressionFiles()) {
        QAction *action = m_filterMenu->addAction(QFileInfo(file).fileName());
        action->setToolTip(file);
        action->setData(file);
        connect(action, SIGNAL(triggered(bool)),
                this, SLOT(suppressionActionTriggered()));
        m_suppressionActions.append(action);
    }
}

void MemcheckTool::suppressionActionTriggered()
{
    QAction *action = qobject_cast<QAction *>(sender());
    QTC_ASSERT(action, return);
    const QString file = action->data().toString();
    QTC_ASSERT(!file.isEmpty(), return);

    Core::EditorManager::openEditorAt(file, 0);
}

void MemcheckTool::loadExternalXmlLogFile()
{
    const QString filePath = QFileDialog::getOpenFileName(
                Core::ICore::mainWindow(),
                tr("Open Memcheck XML Log File"),
                QString(),
                tr("XML Files (*.xml);;All Files (*)"));
    if (filePath.isEmpty())
        return;

    QFile *logFile = new QFile(filePath);
    if (!logFile->open(QIODevice::ReadOnly | QIODevice::Text)) {
        delete logFile;
        QMessageBox::critical(m_errorView, tr("Internal Error"),
            tr("Failed to open file for reading: %1").arg(filePath));
        return;
    }

    setBusyCursor(true);
    clearErrorView();
    m_loadExternalLogFile->setDisabled(true);

    if (!m_settings || m_settings != ValgrindPlugin::globalSettings()) {
        m_settings = ValgrindPlugin::globalSettings();
        m_errorView->settingsChanged(m_settings);
        updateFromSettings();
    }

    ThreadedParser *parser = new ThreadedParser;
    connect(parser, SIGNAL(error(Valgrind::XmlProtocol::Error)),
            this, SLOT(parserError(Valgrind::XmlProtocol::Error)));
    connect(parser, SIGNAL(internalError(QString)),
            this, SLOT(internalParserError(QString)));
    connect(parser, SIGNAL(finished()), this, SLOT(loadingExternalXmlLogFileFinished()));
    connect(parser, SIGNAL(finished()), parser, SLOT(deleteLater()));

    parser->parse(logFile); // ThreadedParser owns the file
}

void MemcheckTool::parserError(const Valgrind::XmlProtocol::Error &error)
{
    m_errorModel->addError(error);
}

void MemcheckTool::internalParserError(const QString &errorString)
{
    QMessageBox::critical(m_errorView, tr("Internal Error"),
        tr("Error occurred parsing Valgrind output: %1").arg(errorString));
}

void MemcheckTool::clearErrorView()
{
    QTC_ASSERT(m_errorView, return);
    m_errorModel->clear();

    qDeleteAll(m_suppressionActions);
    m_suppressionActions.clear();
    //QTC_ASSERT(filterMenu()->actions().last() == m_suppressionSeparator, qt_noop());
}

void MemcheckTool::updateErrorFilter()
{
    QTC_ASSERT(m_errorView, return);
    QTC_ASSERT(m_settings, return);

    m_settings->setFilterExternalIssues(!m_filterProjectAction->isChecked());

    QList<int> errorKinds;
    foreach (QAction *a, m_errorFilterActions) {
        if (!a->isChecked())
            continue;
        foreach (const QVariant &v, a->data().toList()) {
            bool ok;
            int kind = v.toInt(&ok);
            if (ok)
                errorKinds << kind;
        }
    }
    m_settings->setVisibleErrorKinds(errorKinds);
}

int MemcheckTool::updateUiAfterFinishedHelper()
{
    const int issuesFound = m_errorModel->rowCount();
    m_goBack->setEnabled(issuesFound > 1);
    m_goNext->setEnabled(issuesFound > 1);
    m_loadExternalLogFile->setEnabled(true);
    setBusyCursor(false);
    return issuesFound;
}

void MemcheckTool::engineFinished()
{
    const int issuesFound = updateUiAfterFinishedHelper();
    AnalyzerManager::showStatusMessage(issuesFound > 0
        ? AnalyzerManager::tr("Memory Analyzer Tool finished, %n issues were found.", 0, issuesFound)
        : AnalyzerManager::tr("Memory Analyzer Tool finished, no issues were found."));
}

void MemcheckTool::loadingExternalXmlLogFileFinished()
{
    const int issuesFound = updateUiAfterFinishedHelper();
    AnalyzerManager::showStatusMessage(issuesFound > 0
        ? AnalyzerManager::tr("Log file processed, %n issues were found.", 0, issuesFound)
        : AnalyzerManager::tr("Log file processed, no issues were found."));
}

void MemcheckTool::setBusyCursor(bool busy)
{
    QCursor cursor(busy ? Qt::BusyCursor : Qt::ArrowCursor);
    m_errorView->setCursor(cursor);
}

} // namespace Internal
} // namespace Valgrind
