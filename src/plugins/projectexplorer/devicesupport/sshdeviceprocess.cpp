/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
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

#include "sshdeviceprocess.h"

#include "idevice.h"

#include <ssh/sshconnection.h>
#include <ssh/sshconnectionmanager.h>
#include <ssh/sshremoteprocess.h>
#include <utils/environment.h>
#include <utils/qtcassert.h>

#include <QString>

namespace ProjectExplorer {

class SshDeviceProcess::SshDeviceProcessPrivate
{
public:
    SshDeviceProcessPrivate(SshDeviceProcess *q) : q(q) {}

    SshDeviceProcess * const q;
    bool serverSupportsSignals;
    QSsh::SshConnection *connection;
    QSsh::SshRemoteProcess::Ptr process;
    QString executable;
    QStringList arguments;
    QString errorMessage;
    QSsh::SshRemoteProcess::ExitStatus exitStatus;
    QByteArray stdOut;
    QByteArray stdErr;
    int exitCode;
    enum State { Inactive, Connecting, Connected, ProcessRunning } state;
    Utils::Environment environment;

    void setState(State newState);
    void doSignal(QSsh::SshRemoteProcess::Signal signal);
};

SshDeviceProcess::SshDeviceProcess(const IDevice::ConstPtr &device, QObject *parent)
    : DeviceProcess(device, parent), d(new SshDeviceProcessPrivate(this))
{
    d->connection = 0;
    d->state = SshDeviceProcessPrivate::Inactive;
    setSshServerSupportsSignals(false);
}

SshDeviceProcess::~SshDeviceProcess()
{
    d->setState(SshDeviceProcessPrivate::Inactive);
    delete d;
}

void SshDeviceProcess::start(const QString &executable, const QStringList &arguments)
{
    QTC_ASSERT(d->state == SshDeviceProcessPrivate::Inactive, return);
    d->setState(SshDeviceProcessPrivate::Connecting);

    d->errorMessage.clear();
    d->exitCode = -1;
    d->executable = executable;
    d->arguments = arguments;
    d->connection
            = QSsh::SshConnectionManager::instance().acquireConnection(device()->sshParameters());
    connect(d->connection, SIGNAL(error(QSsh::SshError)), SLOT(handleConnectionError()));
    connect(d->connection, SIGNAL(disconnected()), SLOT(handleDisconnected()));
    if (d->connection->state() == QSsh::SshConnection::Connected) {
        handleConnected();
    } else {
        connect(d->connection, SIGNAL(connected()), SLOT(handleConnected()));
        if (d->connection->state() == QSsh::SshConnection::Unconnected)
            d->connection->connectToHost();
    }
}

void SshDeviceProcess::interrupt()
{
    QTC_ASSERT(d->state == SshDeviceProcessPrivate::ProcessRunning, return);
    d->doSignal(QSsh::SshRemoteProcess::IntSignal);
}

void SshDeviceProcess::terminate()
{
    d->doSignal(QSsh::SshRemoteProcess::TermSignal);
}

void SshDeviceProcess::kill()
{
    d->doSignal(QSsh::SshRemoteProcess::KillSignal);
}

QString SshDeviceProcess::executable() const
{
    return d->executable;
}

QStringList SshDeviceProcess::arguments() const
{
    return d->arguments;
}

QProcess::ProcessState SshDeviceProcess::state() const
{
    switch (d->state) {
    case SshDeviceProcessPrivate::Inactive:
        return QProcess::NotRunning;
    case SshDeviceProcessPrivate::Connecting:
    case SshDeviceProcessPrivate::Connected:
        return QProcess::Starting;
    case SshDeviceProcessPrivate::ProcessRunning:
        return QProcess::Running;
    default:
        QTC_CHECK(false);
        return QProcess::NotRunning;
    }
}

QProcess::ExitStatus SshDeviceProcess::exitStatus() const
{
    return d->exitStatus == QSsh::SshRemoteProcess::NormalExit
            ? QProcess::NormalExit : QProcess::CrashExit;
}

int SshDeviceProcess::exitCode() const
{
    return d->exitCode;
}

QString SshDeviceProcess::errorString() const
{
    return d->errorMessage;
}

Utils::Environment SshDeviceProcess::environment() const
{
    return d->environment;
}

void SshDeviceProcess::setEnvironment(const Utils::Environment &env)
{
    d->environment = env;
}

QByteArray SshDeviceProcess::readAllStandardOutput()
{
    const QByteArray data = d->stdOut;
    d->stdOut.clear();
    return data;
}

QByteArray SshDeviceProcess::readAllStandardError()
{
    const QByteArray data = d->stdErr;
    d->stdErr.clear();
    return data;
}

void SshDeviceProcess::setSshServerSupportsSignals(bool signalsSupported)
{
    d->serverSupportsSignals = signalsSupported;
}

void SshDeviceProcess::handleConnected()
{
    QTC_ASSERT(d->state == SshDeviceProcessPrivate::Connecting, return);
    d->setState(SshDeviceProcessPrivate::Connected);

    d->process = d->connection->createRemoteProcess(fullCommandLine().toUtf8());
    connect(d->process.data(), SIGNAL(started()), SLOT(handleProcessStarted()));
    connect(d->process.data(), SIGNAL(closed(int)), SLOT(handleProcessFinished(int)));
    connect(d->process.data(), SIGNAL(readyReadStandardOutput()), SLOT(handleStdout()));
    connect(d->process.data(), SIGNAL(readyReadStandardError()), SLOT(handleStderr()));

    d->process->clearEnvironment();
    const Utils::Environment env = environment();
    for (Utils::Environment::const_iterator it = env.constBegin(); it != env.constEnd(); ++it)
        d->process->addToEnvironment(env.key(it).toUtf8(), env.value(it).toUtf8());
    d->process->start();
}

void SshDeviceProcess::handleConnectionError()
{
    QTC_ASSERT(d->state != SshDeviceProcessPrivate::Inactive, return);

    d->errorMessage = d->connection->errorString();
    handleDisconnected();
}

void SshDeviceProcess::handleDisconnected()
{
    QTC_ASSERT(d->state != SshDeviceProcessPrivate::Inactive, return);
    const SshDeviceProcessPrivate::State oldState = d->state;
    d->setState(SshDeviceProcessPrivate::Inactive);
    switch (oldState) {
    case SshDeviceProcessPrivate::Connecting:
    case SshDeviceProcessPrivate::Connected:
        emit error(QProcess::FailedToStart);
        break;
    case SshDeviceProcessPrivate::ProcessRunning:
        emit finished();
    default:
        break;
    }
}

void SshDeviceProcess::handleProcessStarted()
{
    QTC_ASSERT(d->state == SshDeviceProcessPrivate::Connected, return);

    d->setState(SshDeviceProcessPrivate::ProcessRunning);
    emit started();
}

void SshDeviceProcess::handleProcessFinished(int exitStatus)
{
    d->exitStatus = static_cast<QSsh::SshRemoteProcess::ExitStatus>(exitStatus);
    switch (d->exitStatus) {
    case QSsh::SshRemoteProcess::FailedToStart:
        QTC_ASSERT(d->state == SshDeviceProcessPrivate::Connected, return);
        break;
    case QSsh::SshRemoteProcess::CrashExit:
        QTC_ASSERT(d->state == SshDeviceProcessPrivate::ProcessRunning, return);
        break;
    case QSsh::SshRemoteProcess::NormalExit:
        QTC_ASSERT(d->state == SshDeviceProcessPrivate::ProcessRunning, return);
        d->exitCode = d->process->exitCode();
        break;
    default:
        QTC_ASSERT(false, return);
    }
    d->errorMessage = d->process->errorString();
    d->setState(SshDeviceProcessPrivate::Inactive);
    emit finished();
}

void SshDeviceProcess::handleStdout()
{
    d->stdOut += d->process->readAllStandardOutput();
    emit readyReadStandardOutput();
}

void SshDeviceProcess::handleStderr()
{
    d->stdErr += d->process->readAllStandardError();
    emit readyReadStandardError();
}

QString SshDeviceProcess::fullCommandLine() const
{
    QString cmdLine = executable();
    if (!arguments().isEmpty())
        cmdLine.append(QLatin1Char(' ')).append(arguments().join(QLatin1String(" ")));
    return cmdLine;
}

void SshDeviceProcess::SshDeviceProcessPrivate::doSignal(QSsh::SshRemoteProcess::Signal signal)
{
    switch (state) {
    case SshDeviceProcessPrivate::Inactive:
        QTC_ASSERT(false, return);
        break;
    case SshDeviceProcessPrivate::Connecting:
        errorMessage = tr("Terminated by request.");
        setState(SshDeviceProcessPrivate::Inactive);
        emit q->error(QProcess::FailedToStart);
        break;
    case SshDeviceProcessPrivate::Connected:
    case SshDeviceProcessPrivate::ProcessRunning:
        if (serverSupportsSignals) {
            process->sendSignal(signal);
        } else {
            const DeviceProcessSupport::Ptr processSupport = q->device()->processSupport();
            QString signalCommandLine = signal == QSsh::SshRemoteProcess::IntSignal
                    ? processSupport->interruptProcessByNameCommandLine(executable)
                    : processSupport->killProcessByNameCommandLine(executable);
            const QSsh::SshRemoteProcess::Ptr signalProcess
                    = connection->createRemoteProcess(signalCommandLine.toUtf8());
            signalProcess->start();
        }
        break;
    }
}

void SshDeviceProcess::SshDeviceProcessPrivate::setState(SshDeviceProcess::SshDeviceProcessPrivate::State newState)
{
    if (state == newState)
        return;

    state = newState;
    if (state != Inactive)
        return;

    if (process)
        process->disconnect(q);
    if (connection) {
        connection->disconnect(q);
        QSsh::SshConnectionManager::instance().releaseConnection(connection);
        connection = 0;
    }
}

qint64 SshDeviceProcess::write(const QByteArray &data)
{
    return d->process->write(data);
}

} // namespace ProjectExplorer
