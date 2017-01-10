#include "luafunctionfilter.h"

#include <coreplugin/idocument.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>

#include <QStringMatcher>

QList<QSharedPointer<LuaFunctionFilter::FunctionEntry> > LuaFunctionFilter::parseFunctions(const QString &text)
{
    QList<QSharedPointer<LuaFunctionFilter::FunctionEntry> > functions;

    QString searchExpression(R"(.*function\s*((.*)\s*(\(.*\))).*)");

    QStringList parts = text.split(QChar('\n'));
    for (int i = 0; i < parts.size(); ++i)
    {
        QRegExp regex(searchExpression);
        regex.setMinimal(true);

        if (regex.indexIn(parts[i]) != -1)
        {
            QSharedPointer<FunctionEntry> entry(new FunctionEntry());

            QStringList capturedTexts = regex.capturedTexts();
            QString functionName = capturedTexts[2];

            if (functionName.trimmed().isEmpty())
                continue;

            QChar splitChar('\0');
            if (functionName.contains(QChar(':')))
            {
                entry->surroundingType = SurroundingType::Object;
                splitChar = QChar(':');
            }
            else if (functionName.contains(QChar('.')))
            {
                entry->surroundingType = SurroundingType::Module;
                splitChar = QChar('.');
            }

            if (splitChar != QChar('\0'))
            {
                auto parts = functionName.split(splitChar);
                functionName = parts.last();
                for (int j = 0; j < parts.size() - 1; ++j)
                {
                    if (j > 0)
                        entry->surroundingName += QString(".");

                    entry->surroundingName += parts[j].trimmed();
                }
            }

            entry->line = i + 1;
            entry->fullFunction = capturedTexts[1].trimmed();
            entry->functionName = functionName.trimmed();
            entry->arguments = capturedTexts[3].trimmed();

            functions.push_back(entry);
        }
    }

    return functions;
}

LuaFunctionFilter::LuaFunctionFilter()
    : m_functionIcon(QLatin1String(":/LuaEditor/images/func.png"))
{
    setId("Functions in current Document");
    setDisplayName(tr("Lua Functions in Current Document"));
    setShortcutString(QString(QLatin1Char('.')));
    setPriority(High);
    setIncludedByDefault(false);

    connect(Core::EditorManager::instance(), &Core::EditorManager::currentEditorChanged,
            this, &LuaFunctionFilter::onCurrentEditorChanged);
    connect(Core::EditorManager::instance(), &Core::EditorManager::editorAboutToClose,
            this, &LuaFunctionFilter::onEditorAboutToClose);
    connect(Core::EditorManager::instance(), &Core::EditorManager::currentDocumentStateChanged,
            this, &LuaFunctionFilter::onDocumentUpdated);

}

QList<Core::LocatorFilterEntry> LuaFunctionFilter::matchesFor(
        QFutureInterface<Core::LocatorFilterEntry> &future, const QString & origEntry)
{
    QString entry = trimWildcards(origEntry);

    QList<Core::LocatorFilterEntry> goodEntries;
    QList<Core::LocatorFilterEntry> betterEntries;

    QStringMatcher matcher(entry, Qt::CaseInsensitive);
    const QChar asterisk = QLatin1Char('*');
    QRegExp regexp(asterisk + entry + asterisk, Qt::CaseInsensitive, QRegExp::Wildcard);
    if (!regexp.isValid())
        return betterEntries;

    bool hasWildcard = (entry.contains(asterisk) || entry.contains(QLatin1Char('?')));
    const Qt::CaseSensitivity caseSensitivityForPrefix = caseSensitivity(entry);

    QSet<QString> functions;

    foreach (QSharedPointer<FunctionEntry> info, itemsOfCurrentDocument())
    {
        if (future.isCanceled())
            break;

        QString matchString = info->fullFunction;

        if ((hasWildcard && regexp.exactMatch(matchString))
            || (!hasWildcard && matcher.indexIn(matchString) != -1))
        {
            QVariant id = qVariantFromValue(info);
            QString name = info->functionName + info->arguments;
            QString extraInfo = info->surroundingName;

            if (functions.contains(info->functionName) && !info->surroundingName.isEmpty())
            {
                if (info->surroundingType == SurroundingType::Module)
                    name = info->surroundingName + QString(".") + info->functionName;
                else if (info->surroundingType == SurroundingType::Object)
                    name = info->surroundingName + QString(":") + info->functionName;
            }

            Core::LocatorFilterEntry filterEntry(this, name, id, m_functionIcon);
            filterEntry.extraInfo = extraInfo;

            if (matchString.startsWith(entry, caseSensitivityForPrefix))
                betterEntries.append(filterEntry);
            else
                goodEntries.append(filterEntry);

            functions.insert(info->functionName);
        }
    }

    // entries are unsorted by design!
    betterEntries += goodEntries;
    return betterEntries;
}

void LuaFunctionFilter::accept(Core::LocatorFilterEntry selection) const
{
    QSharedPointer<FunctionEntry> info = qvariant_cast<QSharedPointer<FunctionEntry>>(selection.internalData);
    Core::EditorManager::openEditorAt(info->fileName, info->line);
}

void LuaFunctionFilter::refresh(QFutureInterface<void> &future)
{
    Q_UNUSED(future)
}

void LuaFunctionFilter::onDocumentUpdated()
{
    QMutexLocker locker(&m_mutex);
    if (m_currentEditor)
    {
        m_currentFileName = m_currentEditor->document()->filePath().toString();
        if (m_currentFileName.endsWith(QString(".lua")))
        {
            m_currentContents = QString::fromUtf8(m_currentEditor->document()->contents());
        }
        else
        {
            m_currentContents.clear();
        }
    }
    else
    {
        m_currentFileName.clear();
        m_currentContents.clear();
    }

    m_itemsOfCurrentDoc.clear();
}

void LuaFunctionFilter::onCurrentEditorChanged(Core::IEditor *currentEditor)
{
    QMutexLocker locker(&m_mutex);
    m_currentEditor = currentEditor;

    if (m_currentEditor)
    {
        m_currentFileName = m_currentEditor->document()->filePath().toString();
        if (m_currentFileName.endsWith(QString(".lua")))
        {
            m_currentContents = QString::fromUtf8(m_currentEditor->document()->contents());
        }
        else
        {
            m_currentContents.clear();
        }
    }
    else
    {
        m_currentFileName.clear();
        m_currentContents.clear();
    }

    m_itemsOfCurrentDoc.clear();
}

void LuaFunctionFilter::onEditorAboutToClose(Core::IEditor *editorAboutToClose)
{
    if (!editorAboutToClose)
        return;

    QMutexLocker locker(&m_mutex);
    if (m_currentFileName == editorAboutToClose->document()->filePath().toString()) {
        m_currentFileName.clear();
        m_itemsOfCurrentDoc.clear();
        m_currentContents.clear();
    }

    if (m_currentEditor == editorAboutToClose)
        m_currentEditor = nullptr;
}

QList<QSharedPointer<LuaFunctionFilter::FunctionEntry> > LuaFunctionFilter::itemsOfCurrentDocument()
{
    QMutexLocker locker(&m_mutex);

    if (m_currentFileName.isEmpty())
        return QList<QSharedPointer<FunctionEntry>>();

    if (m_itemsOfCurrentDoc.isEmpty())
    {
        m_itemsOfCurrentDoc = parseFunctions(m_currentContents);

        for (QSharedPointer<LuaFunctionFilter::FunctionEntry> &function : m_itemsOfCurrentDoc)
        {
            function->fileName = m_currentFileName;
        }
    }

    return m_itemsOfCurrentDoc;
}
