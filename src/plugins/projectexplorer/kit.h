/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#ifndef KIT_H
#define KIT_H

#include "projectexplorer_export.h"
#include "task.h"

#include <coreplugin/id.h>

#include <QVariant>

namespace Utils { class Environment; }

namespace ProjectExplorer {

namespace Internal {
class KitManagerPrivate;
class KitPrivate;
} // namespace Internal

/**
 * @brief The Kit class
 *
 * The kit holds a set of values defining a system targeted by the software
 * under development.
 */
class PROJECTEXPLORER_EXPORT Kit
{
public:
    Kit();
    ~Kit();

    bool isValid() const;
    QList<Task> validate();

    QString displayName() const;
    void setDisplayName(const QString &name);

    bool isAutoDetected() const;
    Core::Id id() const;

    QIcon icon() const;
    QString iconPath() const;
    void setIconPath(const QString &path);

    QVariant value(const Core::Id &key, const QVariant &unset = QVariant()) const;
    bool hasValue(const Core::Id &key) const;
    void setValue(const Core::Id &key, const QVariant &value);
    void removeKey(const Core::Id &key);

    bool operator==(const Kit &other) const;

    void addToEnvironment(Utils::Environment &env) const;

    QString toHtml();
    Kit *clone(bool keepName = false) const;

private:
    // Unimplemented.
    Kit(const Kit &other);
    void operator=(const Kit &other);

    void setAutoDetected(bool detected);
    void setValid(bool valid);

    void kitUpdated();

    QVariantMap toMap() const;
    bool fromMap(const QVariantMap &value);

    Internal::KitPrivate *d;

    friend class KitManager;
    friend class Internal::KitManagerPrivate;
};

} // namespace ProjectExplorer

Q_DECLARE_METATYPE(ProjectExplorer::Kit *)

#endif // KIT_H