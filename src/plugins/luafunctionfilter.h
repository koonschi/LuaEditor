#ifndef LUAFUNCTIONFILTER_H
#define LUAFUNCTIONFILTER_H

#include <coreplugin/locator/ilocatorfilter.h>

#include "luafunctionparser.h"

namespace Core { class IEditor; }

class LuaFunctionFilter : public Core::ILocatorFilter
{
    Q_OBJECT
public:
    typedef LuaEditor::Internal::FunctionParser::Function Function;

public:
    explicit LuaFunctionFilter();
    ~LuaFunctionFilter() {}

    QList<Core::LocatorFilterEntry> matchesFor(QFutureInterface<Core::LocatorFilterEntry> &future, const QString &entry);
    void accept(Core::LocatorFilterEntry selection, QString *newText, int *selectionStart, int *selectionLength) const;

    void refresh(QFutureInterface<void> &future);

private:
    void onDocumentUpdated();
    void onCurrentEditorChanged(Core::IEditor *currentEditor);
    void onEditorAboutToClose(Core::IEditor *currentEditor);

    QList<QSharedPointer<Function>> itemsOfCurrentDocument();

    QIcon m_functionIcon;

    mutable QMutex m_mutex;
    Core::IEditor *m_currentEditor = nullptr;
    QString m_currentFileName;
    QString m_currentContents;
    QList<QSharedPointer<Function>> m_itemsOfCurrentDoc;
};

Q_DECLARE_METATYPE(QSharedPointer<LuaFunctionFilter::Function>);

#endif // LUAFUNCTIONFILTER_H
