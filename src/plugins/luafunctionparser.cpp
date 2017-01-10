#include "luafunctionparser.h"

#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QMap>

#include <iostream>

namespace LuaEditor { namespace Internal {

QList<QSharedPointer<FunctionParser::Function>> FunctionParser::parseFunctionsInFile(const QString &path)
{
    FunctionList result;

    QFile ifile(path);
    if (!ifile.exists())
        return result;

    static QMap<QString, std::pair<QDateTime, FunctionList>> dates;

    auto it = dates.find(path);
    if (it != dates.end())
    {
        QFileInfo info(path);
        const QDateTime &read = it->first;

        // if the read happened later than the last modification, it's up to date
        if (info.lastModified() < read)
        {
            return it->second;
        }
    }

    ifile.open(QIODevice::ReadOnly | QIODevice::Text);
    // read whole content
    QString content = QString::fromLatin1(ifile.readAll());
    ifile.close();

    // get words
    result = parseFunctions(content);

    // update cache
    dates[path] = std::make_pair(QDateTime::currentDateTime(), result);

    return result;
}

QList<QSharedPointer<FunctionParser::Function> > FunctionParser::parseFunctions(const QString &text)
{
    FunctionList functions;

    QString searchExpression(R"(.*function\s*((.*)\s*(\(.*\))).*)");

    QStringList parts = text.split(QChar('\n'));
    for (int i = 0; i < parts.size(); ++i)
    {
        QRegExp regex(searchExpression);
        regex.setMinimal(true);

        if (regex.indexIn(parts[i]) != -1)
        {
            QSharedPointer<Function> entry(new Function());

            QStringList capturedTexts = regex.capturedTexts();
            QString functionName = capturedTexts[2];

            if (functionName.trimmed().isEmpty())
                continue;

            QChar splitChar('\0');
            if (functionName.contains(QChar(':')))
            {
                entry->surroundingType = Function::SurroundingType::Object;
                splitChar = QChar(':');
            }
            else if (functionName.contains(QChar('.')))
            {
                entry->surroundingType = Function::SurroundingType::Module;
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


}

}

