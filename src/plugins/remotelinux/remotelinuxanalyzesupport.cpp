/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "remotelinuxanalyzesupport.h"

#include "remotelinuxrunconfiguration.h"

#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/project.h>
#include <projectexplorer/target.h>
#include <projectexplorer/toolchain.h>
#include <projectexplorer/kitinformation.h>
#include <projectexplorer/runnables.h>

#include <utils/qtcassert.h>
#include <utils/qtcprocess.h>
#include <qmldebug/qmloutputparser.h>
#include <qmldebug/qmldebugcommandlinearguments.h>

#include <QPointer>

using namespace QSsh;
using namespace ProjectExplorer;
using namespace Utils;

namespace RemoteLinux {
namespace Internal {

class RemoteLinuxAnalyzeSupportPrivate
{
public:
    RemoteLinuxAnalyzeSupportPrivate(RunControl *runControl)
    {
        if (runControl->runMode() != ProjectExplorer::Constants::PERFPROFILER_RUN_MODE)
            return;
        RunConfiguration *runConfiguration = runControl->runConfiguration();
        QTC_ASSERT(runConfiguration, return);
        IRunConfigurationAspect *perfAspect =
                runConfiguration->extraAspect("Analyzer.Perf.Settings");
        QTC_ASSERT(perfAspect, return);
        perfRecordArguments =
                perfAspect->currentSettings()->property("perfRecordArguments").toStringList()
                .join(' ');
    }

    Utils::Port qmlPort;
    QString remoteFifo;
    QString perfRecordArguments;

    ApplicationLauncher outputGatherer;
    QmlDebug::QmlOutputParser outputParser;
};

} // namespace Internal

using namespace Internal;

RemoteLinuxAnalyzeSupport::RemoteLinuxAnalyzeSupport(RunControl *runControl)
    : ToolRunner(runControl),
      d(new RemoteLinuxAnalyzeSupportPrivate(runControl))
{
    connect(runControl, &RunControl::starting,
            this, &RemoteLinuxAnalyzeSupport::handleRemoteSetupRequested);
    connect(&d->outputParser, &QmlDebug::QmlOutputParser::waitingForConnectionOnPort,
            this, &RemoteLinuxAnalyzeSupport::remoteIsRunning);
    connect(runControl, &RunControl::finished,
            this, &RemoteLinuxAnalyzeSupport::handleProfilingFinished);

    connect(qobject_cast<AbstractRemoteLinuxRunSupport *>(runControl->targetRunner()),
            &AbstractRemoteLinuxRunSupport::executionStartRequested,
            this, &RemoteLinuxAnalyzeSupport::startExecution);
    connect(qobject_cast<AbstractRemoteLinuxRunSupport *>(runControl->targetRunner()),
            &AbstractRemoteLinuxRunSupport::adapterSetupFailed,
            this, &RemoteLinuxAnalyzeSupport::handleAdapterSetupFailed);
}

RemoteLinuxAnalyzeSupport::~RemoteLinuxAnalyzeSupport()
{
    delete d;
}

void RemoteLinuxAnalyzeSupport::showMessage(const QString &msg, Utils::OutputFormat format)
{
    if (targetRunner()->state() != AbstractRemoteLinuxRunSupport::Inactive)
        appendMessage(msg, format);
    d->outputParser.processOutput(msg);
}

void RemoteLinuxAnalyzeSupport::handleRemoteSetupRequested()
{
    QTC_ASSERT(targetRunner()->state() == AbstractRemoteLinuxRunSupport::Inactive, return);

    const Core::Id runMode = runControl()->runMode();
    if (runMode == ProjectExplorer::Constants::QML_PROFILER_RUN_MODE) {
        showMessage(tr("Checking available ports...") + QLatin1Char('\n'),
                    Utils::NormalMessageFormat);
        targetRunner()->startPortsGathering();
    } else if (runMode == ProjectExplorer::Constants::PERFPROFILER_RUN_MODE) {
        showMessage(tr("Creating remote socket...") + QLatin1Char('\n'),
                    Utils::NormalMessageFormat);
        targetRunner()->createRemoteFifo();
    }
}

void RemoteLinuxAnalyzeSupport::startExecution()
{
    QTC_ASSERT(targetRunner()->state() == AbstractRemoteLinuxRunSupport::GatheringResources, return);

    const Core::Id runMode = runControl()->runMode();
    if (runMode == ProjectExplorer::Constants::QML_PROFILER_RUN_MODE) {
        d->qmlPort = targetRunner()->findPort();
        if (!d->qmlPort.isValid()) {
            handleAdapterSetupFailed(tr("Not enough free ports on device for profiling."));
            return;
        }
    } else if (runMode == ProjectExplorer::Constants::PERFPROFILER_RUN_MODE) {
        d->remoteFifo = targetRunner()->fifo();
        if (d->remoteFifo.isEmpty()) {
            handleAdapterSetupFailed(tr("FIFO for profiling data could not be created."));
            return;
        }
    }

    targetRunner()->setState(AbstractRemoteLinuxRunSupport::StartingRunner);

    ApplicationLauncher *runner = targetRunner()->applicationLauncher();
    connect(runner, &ApplicationLauncher::remoteStderr,
            this, &RemoteLinuxAnalyzeSupport::handleRemoteErrorOutput);
    connect(runner, &ApplicationLauncher::remoteStdout,
            this, &RemoteLinuxAnalyzeSupport::handleRemoteOutput);
    connect(runner, &ApplicationLauncher::remoteProcessStarted,
            this, &RemoteLinuxAnalyzeSupport::handleRemoteProcessStarted);
    connect(runner, &ApplicationLauncher::finished,
            this, &RemoteLinuxAnalyzeSupport::handleAppRunnerFinished);
    connect(runner, &ApplicationLauncher::reportProgress,
            this, &RemoteLinuxAnalyzeSupport::handleProgressReport);
    connect(runner, &ApplicationLauncher::reportError,
            this, &RemoteLinuxAnalyzeSupport::handleAppRunnerError);

    auto r = runControl()->runnable().as<StandardRunnable>();

    if (runMode == ProjectExplorer::Constants::QML_PROFILER_RUN_MODE) {
        if (!r.commandLineArguments.isEmpty())
            r.commandLineArguments.append(QLatin1Char(' '));
        r.commandLineArguments += QmlDebug::qmlDebugTcpArguments(QmlDebug::QmlProfilerServices,
                                                                 d->qmlPort);
    } else if (runMode == ProjectExplorer::Constants::PERFPROFILER_RUN_MODE) {
        r.commandLineArguments = QLatin1String("-c 'perf record -o - ") + d->perfRecordArguments
                + QLatin1String(" -- ") + r.executable + QLatin1String(" ")
                + r.commandLineArguments + QLatin1String(" > ") + d->remoteFifo
                + QLatin1String("'");
        r.executable = QLatin1String("sh");

        connect(&d->outputGatherer, SIGNAL(remoteStdout(QByteArray)),
                runControl(), SIGNAL(analyzePerfOutput(QByteArray)));
        connect(&d->outputGatherer, SIGNAL(finished(bool)),
                runControl(), SIGNAL(perfFinished()));

        StandardRunnable outputRunner;
        outputRunner.executable = QLatin1String("sh");
        outputRunner.commandLineArguments =
                QString::fromLatin1("-c 'cat %1 && rm -r `dirname %1`'").arg(d->remoteFifo);
        d->outputGatherer.start(outputRunner, device());
        remoteIsRunning();
    }
    runner->start(r, device());
}

void RemoteLinuxAnalyzeSupport::handleAppRunnerError(const QString &error)
{
    if (targetRunner()->state() == AbstractRemoteLinuxRunSupport::Running)
        showMessage(error, Utils::ErrorMessageFormat);
    else if (targetRunner()->state() != AbstractRemoteLinuxRunSupport::Inactive)
        handleAdapterSetupFailed(error);
}

void RemoteLinuxAnalyzeSupport::handleAppRunnerFinished(bool success)
{
    // reset needs to be called first to ensure that the correct state is set.
    targetRunner()->reset();
    if (!success)
        showMessage(tr("Failure running remote process."), Utils::NormalMessageFormat);
    runControl()->notifyRemoteFinished();
}

void RemoteLinuxAnalyzeSupport::handleProfilingFinished()
{
    targetRunner()->setFinished();
}

void RemoteLinuxAnalyzeSupport::remoteIsRunning()
{
    runControl()->notifyRemoteSetupDone(d->qmlPort);
}

AbstractRemoteLinuxRunSupport *RemoteLinuxAnalyzeSupport::targetRunner() const
{
    return qobject_cast<AbstractRemoteLinuxRunSupport *>(runControl()->targetRunner());
}

void RemoteLinuxAnalyzeSupport::handleRemoteOutput(const QByteArray &output)
{
    QTC_ASSERT(targetRunner()->state() == AbstractRemoteLinuxRunSupport::Inactive
            || targetRunner()->state() == AbstractRemoteLinuxRunSupport::Running, return);

    showMessage(QString::fromUtf8(output), Utils::StdOutFormat);
}

void RemoteLinuxAnalyzeSupport::handleRemoteErrorOutput(const QByteArray &output)
{
    QTC_ASSERT(targetRunner()->state() != AbstractRemoteLinuxRunSupport::GatheringResources, return);

    showMessage(QString::fromUtf8(output), Utils::StdErrFormat);
}

void RemoteLinuxAnalyzeSupport::handleProgressReport(const QString &progressOutput)
{
    showMessage(progressOutput + QLatin1Char('\n'), Utils::NormalMessageFormat);
}

void RemoteLinuxAnalyzeSupport::handleAdapterSetupFailed(const QString &error)
{
    showMessage(tr("Initial setup failed: %1").arg(error), Utils::NormalMessageFormat);
}

void RemoteLinuxAnalyzeSupport::handleRemoteProcessStarted()
{
    QTC_ASSERT(targetRunner()->state() == AbstractRemoteLinuxRunSupport::StartingRunner, return);

    targetRunner()->setState(AbstractRemoteLinuxRunSupport::Running);
}

} // namespace RemoteLinux
