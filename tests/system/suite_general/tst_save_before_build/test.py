#############################################################################
##
## Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
## Contact: http://www.qt-project.org/legal
##
## This file is part of Qt Creator.
##
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and Digia.  For licensing terms and
## conditions see http://qt.digia.com/licensing.  For further information
## use the contact form at http://qt.digia.com/contact-us.
##
## GNU Lesser General Public License Usage
## Alternatively, this file may be used under the terms of the GNU Lesser
## General Public License version 2.1 as published by the Free Software
## Foundation and appearing in the file LICENSE.LGPL included in the
## packaging of this file.  Please review the following information to
## ensure the GNU Lesser General Public License version 2.1 requirements
## will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
##
## In addition, as a special exception, Digia gives you certain additional
## rights.  These rights are described in the Digia Qt LGPL Exception
## version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
##
#############################################################################

source("../../shared/qtcreator.py")

def ensureSaveBeforeBuildChecked(shouldBeChecked):
    invokeMenuItem("Tools", "Options...")
    waitForObjectItem(":Options_QListView", "Build & Run")
    clickItem(":Options_QListView", "Build & Run", 14, 15, 0, Qt.LeftButton)
    clickOnTab(":Options.qt_tabwidget_tabbar_QTabBar", "General")
    if test.compare(waitForObject(":Build and Run.Save all files before build_QCheckBox").checked,
                    shouldBeChecked, "'Save all files before build' should be %s" % str(shouldBeChecked)):
        clickButton(waitForObject(":Options.Cancel_QPushButton"))
    else:
        ensureChecked(":Build and Run.Save all files before build_QCheckBox", shouldBeChecked)
        clickButton(waitForObject(":Options.OK_QPushButton"))

def main():
    startApplication("qtcreator" + SettingsPath)
    if not startedWithoutPluginError():
        return
    ensureSaveBeforeBuildChecked(False)
    # create qt quick application
    createNewQtQuickApplication(tempDir(), "SampleApp", qtQuickVersion="2.2")
    for expectDialog in [True, False]:
        files = ["SampleApp.SampleApp\\.pro",
                 "SampleApp.deployment.deployment\\.pri",
                 "SampleApp.Sources.main\\.cpp",
                 "SampleApp.Resources.qml\.qrc./.main\\.qml"]
        for i, file in enumerate(files):
            if not openDocument(file):
                test.fatal("Could not open file '%s'" % simpleFileName(file))
                continue
            test.log("Changing file '%s'" % simpleFileName(file))
            typeLines(getEditorForFileSuffix(file), "")
            # try to compile
            clickButton(waitForObject(":*Qt Creator.Build Project_Core::Internal::FancyToolButton"))
            try:
                ensureChecked(":Save Changes.Always save files before build_QCheckBox",
                              i == len(files) - 1, 5000) # At the last iteration, check the box
                clickButton(waitForObject(":Save Changes.Save All_QPushButton"))
                test.verify(expectDialog, "The 'Save Changes' dialog was shown.")
            except:
                test.verify(not expectDialog, "The 'Save Changes' dialog was not shown.")
            waitForCompile()
    ensureSaveBeforeBuildChecked(True)
    invokeMenuItem("File", "Exit")
