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

#ifndef SETTINGSACCESSOR_H
#define SETTINGSACCESSOR_H

#include <utils/fileutils.h>

#include <QVariantMap>

namespace Utils { class PersistentSettingsWriter; }

namespace ProjectExplorer {

class Project;

namespace Internal { class VersionUpgrader; }

class SettingsAccessorPrivate;

class SettingsAccessor
{
public:
    SettingsAccessor(Project *project);
    virtual ~SettingsAccessor();

    Project *project() const;

    QVariantMap restoreSettings(QWidget *parent) const;
    bool saveSettings(const QVariantMap &data, QWidget *parent) const;

    static QVariantMap setVersionInMap(const QVariantMap &data, int version);
    static int versionFromMap(const QVariantMap &data);
    static int originalVersionFromMap(const QVariantMap &data);
    static QVariantMap setOriginalVersionInMap(const QVariantMap &data, int version);

    int currentVersion() const;
    int firstSupportedVersion() const;

    bool addVersionUpgrader(Internal::VersionUpgrader *upgrader); // takes ownership of upgrader

protected:
    QVariantMap readFile(const Utils::FileName &path) const;
    QVariantMap upgradeSettings(const QVariantMap &data, int toVersion) const;
    virtual QVariantMap prepareSettings(const QVariantMap &data) const;

    virtual bool isBetterMatch(const QVariantMap &origData, const QVariantMap &newData) const;

private:
    QList<Utils::FileName> settingsFiles(const QString &suffix) const;
    static QByteArray creatorId();
    QString defaultFileName(const QString &suffix) const;
    void backupUserFile() const;

    QVariantMap readUserSettings(QWidget *parent) const;
    QVariantMap readSharedSettings(QWidget *parent) const;
    QVariantMap mergeSettings(const QVariantMap &userMap, const QVariantMap &sharedMap) const;

    static QByteArray environmentIdFromMap(const QVariantMap &data);

    QString m_userSuffix;
    QString m_sharedSuffix;

    Project *m_project;

    SettingsAccessorPrivate *d;

    friend class SettingsAccessorPrivate;
};

namespace Internal {
class UserFileAccessor : public SettingsAccessor
{
public:
    UserFileAccessor(Project *project);

protected:
    QVariantMap prepareSettings(const QVariantMap &data) const;
};

} // namespace Internal
} // namespace ProjectExplorer

#endif // SETTINGSACCESSOR_H
