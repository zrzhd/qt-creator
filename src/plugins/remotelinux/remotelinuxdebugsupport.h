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

#pragma once

#include "abstractremotelinuxrunsupport.h"

#include <debugger/debuggerruncontrol.h>

namespace RemoteLinux {

namespace Internal { class LinuxDeviceDebugSupportPrivate; }

class REMOTELINUX_EXPORT LinuxDeviceDebugSupport : public Debugger::DebuggerRunTool
{
    Q_OBJECT

public:
    LinuxDeviceDebugSupport(ProjectExplorer::RunControl *runControl,
                            const Debugger::DebuggerStartParameters &sp,
                            QString *errorMessage = nullptr);
    ~LinuxDeviceDebugSupport() override;

protected:
    virtual ProjectExplorer::Runnable realRunnable() const;
    bool isCppDebugging() const;
    bool isQmlDebugging() const;
    Debugger::DebuggerRunControl *runControl() const;

private:
    void startExecution();
    void handleAdapterSetupFailed(const QString &error);

    void handleRemoteSetupRequested();
    void handleAppRunnerError(const QString &error);
    void handleRemoteOutput(const QByteArray &output);
    void handleRemoteErrorOutput(const QByteArray &output);
    void handleAppRunnerFinished(bool success);
    void handleProgressReport(const QString &progressOutput);

    void handleRemoteProcessStarted();
    void handleAdapterSetupDone();
    void handleDebuggingFinished();

    void showMessage(const QString &msg, int channel);

    AbstractRemoteLinuxRunSupport *targetRunner() const;
    AbstractRemoteLinuxRunSupport::State state() const;

    Internal::LinuxDeviceDebugSupportPrivate * const d;
};

} // namespace RemoteLinux
