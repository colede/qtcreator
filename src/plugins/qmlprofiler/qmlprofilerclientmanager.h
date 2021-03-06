/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#ifndef QMLPROFILERCLIENTMANAGER_H
#define QMLPROFILERCLIENTMANAGER_H

#include "qmlprofilerstatemanager.h"
#include <qmldebug/qmlprofilereventlocation.h>

#include <QObject>
#include <QStringList>

namespace QmlProfiler {
class QmlProfilerModelManager;

namespace Internal {

class QmlProfilerClientManager : public QObject
{
    Q_OBJECT
public:
    explicit QmlProfilerClientManager(QObject *parent = 0);
    ~QmlProfilerClientManager();

    void registerProfilerStateManager(QmlProfilerStateManager *profilerState);

    void setTcpConnection(QString host, quint64 port);
    void setOstConnection(QString ostDevice);

    void clearBufferedData();
    void discardPendingData();
    bool isConnected() const;

    void setModelManager(QmlProfilerModelManager *m);
signals:
    void connectionFailed();
    void connectionClosed();
    void dataReadyForProcessing();

public slots:
    void connectClient(quint16 port);
    void disconnectClient();

private slots:
    void tryToConnect();
    void qmlDebugConnectionOpened();
    void qmlDebugConnectionClosed();
    void logState(const QString &);

    void retryMessageBoxFinished(int result);

    void qmlComplete(qint64 maximumTime);
    void v8Complete();

    void profilerStateChanged();
    void clientRecordingChanged();
    void serverRecordingChanged();

private:
    class QmlProfilerClientManagerPrivate;
    QmlProfilerClientManagerPrivate *d;

    void connectToClient();

    void enableServices();
    void connectClientSignals();
    void disconnectClientSignals();

    void stopClientsRecording();
};

}
}

#endif // QMLPROFILERCLIENTMANAGER_H
