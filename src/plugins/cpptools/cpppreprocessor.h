#ifndef CPPPREPROCESSOR_H
#define CPPPREPROCESSOR_H

#include "cppmodelmanagerinterface.h"

#include <cplusplus/PreprocessorEnvironment.h>
#include <cplusplus/pp-engine.h>

#include <QHash>
#include <QPointer>

QT_BEGIN_NAMESPACE
class QTextCodec;
QT_END_NAMESPACE

namespace CppTools {
namespace Internal {

class CppModelManager;

// Documentation inside.
class CPPTOOLS_EXPORT CppPreprocessor: public CPlusPlus::Client
{
    Q_DISABLE_COPY(CppPreprocessor)

public:
    static QString cleanPath(const QString &path);

    CppPreprocessor(QPointer<CppModelManager> modelManager, bool dumpFileNameWhileParsing = false);
    CppPreprocessor(QPointer<CppModelManager> modelManager, const CPlusPlus::Snapshot &snapshot,
                    bool dumpFileNameWhileParsing = false);
    virtual ~CppPreprocessor();

    void setRevision(unsigned revision);
    void setWorkingCopy(const CppTools::CppModelManagerInterface::WorkingCopy &workingCopy);
    void setIncludePaths(const QStringList &includePaths);
    void setFrameworkPaths(const QStringList &frameworkPaths);
    void setTodo(const QStringList &files);

    void run(const QString &fileName);
    void removeFromCache(const QString &fileName);
    void resetEnvironment();

    CPlusPlus::Snapshot snapshot() const
    { return m_snapshot; }

    const QSet<QString> &todo() const
    { return m_todo; }

    CppModelManager *modelManager() const
    { return m_modelManager.data(); }

    void setGlobalSnapshot(const CPlusPlus::Snapshot &snapshot) { m_globalSnapshot = snapshot; }

protected:
    CPlusPlus::Document::Ptr switchDocument(CPlusPlus::Document::Ptr doc);

    bool getFileContents(const QString &absoluteFilePath, QByteArray *contents,
                         unsigned *revision) const;
    bool checkFile(const QString &absoluteFilePath) const;
    QString resolveFile(const QString &fileName, IncludeType type);
    QString resolveFile_helper(const QString &fileName, IncludeType type);

    void mergeEnvironment(CPlusPlus::Document::Ptr doc);

    virtual void macroAdded(const CPlusPlus::Macro &macro);
    virtual void passedMacroDefinitionCheck(unsigned offset, unsigned line,
                                            const CPlusPlus::Macro &macro);
    virtual void failedMacroDefinitionCheck(unsigned offset, const CPlusPlus::ByteArrayRef &name);
    virtual void notifyMacroReference(unsigned offset, unsigned line,
                                      const CPlusPlus::Macro &macro);
    virtual void startExpandingMacro(unsigned offset,
                                     unsigned line,
                                     const CPlusPlus::Macro &macro,
                                     const QVector<CPlusPlus::MacroArgumentReference> &actuals);
    virtual void stopExpandingMacro(unsigned offset, const CPlusPlus::Macro &macro);
    virtual void markAsIncludeGuard(const QByteArray &macroName);
    virtual void startSkippingBlocks(unsigned offset);
    virtual void stopSkippingBlocks(unsigned offset);
    virtual void sourceNeeded(unsigned line, const QString &fileName, IncludeType type);

private:
    CppPreprocessor();
    void addFrameworkPath(const QString &frameworkPath);

    CPlusPlus::Snapshot m_snapshot;
    CPlusPlus::Snapshot m_globalSnapshot;
    QPointer<CppModelManager> m_modelManager;
    bool m_dumpFileNameWhileParsing;
    CPlusPlus::Environment m_env;
    CPlusPlus::Preprocessor m_preprocess;
    QStringList m_includePaths;
    CppTools::CppModelManagerInterface::WorkingCopy m_workingCopy;
    QStringList m_frameworkPaths;
    QSet<QString> m_included;
    CPlusPlus::Document::Ptr m_currentDoc;
    QSet<QString> m_todo;
    QSet<QString> m_processed;
    unsigned m_revision;
    QHash<QString, QString> m_fileNameCache;
    QTextCodec *m_defaultCodec;
};

} // namespace Internal
} // namespace CppTools

#endif // CPPPREPROCESSOR_H
