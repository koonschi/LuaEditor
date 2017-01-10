#ifndef LUAFUNCTIONFILTER_H
#define LUAFUNCTIONFILTER_H

#include <coreplugin/locator/ilocatorfilter.h>

namespace Core { class IEditor; }

class LuaFunctionFilter : public Core::ILocatorFilter
{
    Q_OBJECT
public:
    enum SurroundingType
    {
        None = 0,
        Object = 1,
        Module = 2,
    };

    struct FunctionEntry
    {
        QString fullFunction;
        QString functionName;
        QString surroundingName;
        QString arguments;

        SurroundingType surroundingType = SurroundingType::None;

        QString fileName;
        int line = 0;
    };

    static QList<QSharedPointer<FunctionEntry>> parseFunctions(const QString &text);

public:
    explicit LuaFunctionFilter();
    ~LuaFunctionFilter() {}

    QList<Core::LocatorFilterEntry> matchesFor(QFutureInterface<Core::LocatorFilterEntry> &future, const QString &entry);
    void accept(Core::LocatorFilterEntry selection) const;
    void refresh(QFutureInterface<void> &future);

private:
    void onDocumentUpdated();
    void onCurrentEditorChanged(Core::IEditor *currentEditor);
    void onEditorAboutToClose(Core::IEditor *currentEditor);

    QList<QSharedPointer<FunctionEntry>> itemsOfCurrentDocument();

    QIcon m_functionIcon;

    mutable QMutex m_mutex;
    Core::IEditor *m_currentEditor = nullptr;
    QString m_currentFileName;
    QString m_currentContents;
    QList<QSharedPointer<FunctionEntry>> m_itemsOfCurrentDoc;
};

Q_DECLARE_METATYPE(QSharedPointer<LuaFunctionFilter::FunctionEntry>);

#endif // LUAFUNCTIONFILTER_H
