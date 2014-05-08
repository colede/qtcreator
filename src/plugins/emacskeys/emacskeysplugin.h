/**************************************************************************
**
** Copyright (c) nsf <no.smile.face@gmail.com>
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

#ifndef EMACSKEYS_H
#define EMACSKEYS_H

#include <extensionsystem/iplugin.h>

#include <QTextCursor>

// forward declarations
QT_FORWARD_DECLARE_CLASS(QAction)
QT_FORWARD_DECLARE_CLASS(QPlainTextEdit)

namespace Core {
class Id;
class IEditor;
}
namespace TextEditor {
class BaseTextEditorWidget;
}

namespace EmacsKeys {
namespace Internal {

class EmacsKeysState;

class EmacsKeysPlugin : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "EmacsKeys.json")

public:
    EmacsKeysPlugin();
    ~EmacsKeysPlugin();

    bool initialize(const QStringList &arguments, QString *errorString);
    void extensionsInitialized();
    ShutdownFlag aboutToShutdown();

private slots:
    void editorAboutToClose(Core::IEditor *editor);
    void currentEditorChanged(Core::IEditor *editor);

    void deleteCharacter();       // C-d
    void killWord();              // M-d
    void killLine();              // C-k
    void insertLineAndIndent();   // C-j

    void gotoFileStart();         // M-<
    void gotoFileEnd();           // M->
    void gotoLineStart();         // C-a
    void gotoLineEnd();           // C-e
    void gotoNextLine();          // C-n
    void gotoPreviousLine();      // C-p
    void gotoNextCharacter();     // C-f
    void gotoPreviousCharacter(); // C-b
    void gotoNextWord();          // M-f
    void gotoPreviousWord();      // M-b

    void mark();                  // C-SPC
    void exchangeCursorAndMark(); // C-x C-x
    void copy();                  // M-w
    void cut();                   // C-w
    void yank();                  // C-y

    void scrollHalfDown();        // C-v
    void scrollHalfUp();          // M-v

private:
    QAction *registerAction(const Core::Id &id, const char *slot,
        const QString &title);
    void genericGoto(QTextCursor::MoveOperation op, bool abortAssist = true);
    void genericVScroll(int direction);

    QHash<QPlainTextEdit*, EmacsKeysState*> m_stateMap;
    QPlainTextEdit *m_currentEditorWidget;
    EmacsKeysState *m_currentState;
    TextEditor::BaseTextEditorWidget *m_currentBaseTextEditorWidget;
};

} // namespace Internal
} // namespace EmacsKeys

#endif // EMACSKEYS_H
