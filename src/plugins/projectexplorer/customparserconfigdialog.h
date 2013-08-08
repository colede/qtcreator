/****************************************************************************
**
** Copyright (C) 2013 Andre Hartmann.
** Contact: aha_1980@gmx.de
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

#ifndef CUSTOMPARSERCONFIGDIALOG_H
#define CUSTOMPARSERCONFIGDIALOG_H

#include "customparser.h"

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class CustomParserConfigDialog; }
QT_END_NAMESPACE

class CustomParserConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CustomParserConfigDialog(QDialog *parent = 0);
    ~CustomParserConfigDialog();

    void setExampleSettings();
    void setSettings(const ProjectExplorer::CustomParserSettings &settings);
    ProjectExplorer::CustomParserSettings settings() const;
    void setErrorPattern(const QString &errorPattern);
    QString errorPattern() const;
    void setFileNameCap(int fileNameCap);
    int fileNameCap() const;
    void setLineNumberCap(int lineNumberCap);
    int lineNumberCap() const;
    void setMessageCap(int messageCap);
    int messageCap() const;
    bool isDirty() const;

private slots:
    void changed();

private:
    Ui::CustomParserConfigDialog *ui;
    bool m_dirty;
};

#endif // CUSTOMPARSERCONFIGDIALOG_H