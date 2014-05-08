/**************************************************************************
**
** Copyright (c) 2014 BogDan Vatra <bog_dan_ro@yahoo.com>
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

#include "avddialog.h"
#include "androidconfigurations.h"

#include <QMessageBox>

using namespace Android;
using namespace Android::Internal;

AvdDialog::AvdDialog(int minApiLevel, const QString &targetArch, const AndroidConfig *config, QWidget *parent) :
    QDialog(parent), m_config(config), m_minApiLevel(minApiLevel)
{
    m_avdDialog.setupUi(this);

    if (targetArch.isEmpty())
        m_avdDialog.abiComboBox->addItems(QStringList()
                                        << QLatin1String("armeabi-v7a")
                                        << QLatin1String("armeabi")
                                        << QLatin1String("x86")
                                        << QLatin1String("mips"));
    else
        m_avdDialog.abiComboBox->addItems(QStringList(targetArch));

    QRegExp rx(QLatin1String("\\S+"));
    QRegExpValidator v(rx, 0);
    m_avdDialog.nameLineEdit->setValidator(&v);
    m_avdDialog.warningIcon->setPixmap(QPixmap(QLatin1String(":/projectexplorer/images/compile_warning.png")));

    updateApiLevelComboBox();

    connect(m_avdDialog.abiComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(updateApiLevelComboBox()));
}

bool AvdDialog::isValid() const
{
    return !name().isEmpty() && !target().isEmpty() && !abi().isEmpty();
}

QString AvdDialog::target() const
{
    return m_avdDialog.targetComboBox->currentText();
}

QString AvdDialog::name() const
{
    return m_avdDialog.nameLineEdit->text();
}

QString AvdDialog::abi() const
{
    return m_avdDialog.abiComboBox->currentText();
}

int AvdDialog::sdcardSize() const
{
    return m_avdDialog.sizeSpinBox->value();
}

void AvdDialog::updateApiLevelComboBox()
{
    QList<SdkPlatform> filteredList;
    QList<SdkPlatform> platforms = m_config->sdkTargets(m_minApiLevel);
    foreach (const SdkPlatform &platform, platforms) {
        if (platform.abis.contains(abi()))
            filteredList << platform;
    }

    m_avdDialog.targetComboBox->clear();
    m_avdDialog.targetComboBox->addItems(AndroidConfig::apiLevelNamesFor(filteredList));

    if (platforms.isEmpty()) {
        m_avdDialog.warningIcon->setVisible(true);
        m_avdDialog.warningText->setVisible(true);
        m_avdDialog.warningText->setText(tr("Cannot create a new AVD. No sufficiently recent Android SDK available.\n"
                                            "Please install an SDK of at least API version %1.")
                                         .arg(m_minApiLevel));
    } else if (filteredList.isEmpty()) {
        m_avdDialog.warningIcon->setVisible(true);
        m_avdDialog.warningText->setVisible(true);
        m_avdDialog.warningText->setText(tr("Cannot create a AVD for ABI %1. Please install a image for it.")
                                         .arg(abi()));
    } else {
        m_avdDialog.warningIcon->setVisible(false);
        m_avdDialog.warningText->setVisible(false);
    }
}
