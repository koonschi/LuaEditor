#include "luafunctionparser.h"

#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QMap>
#include <QSet>
#include <QDir>

#include <iostream>

namespace LuaEditor { namespace Internal {


QString FunctionParser::requireExists(QDir directory, QStringList packagePaths, QString require)
{
    QFileInfo test = directory.absoluteFilePath(require);
    if (test.exists() && test.isFile())
        return test.canonicalFilePath();

    test = directory.absoluteFilePath(require + ".lua");
    if (test.exists() && test.isFile())
        return test.canonicalFilePath();

    for (QString packagePath : packagePaths)
    {
        QString str = packagePath.replace("?", require);

        QFileInfo test = directory.absoluteFilePath(str);
        if (test.exists() && test.isFile())
            return test.canonicalFilePath();

        test = directory.absoluteFilePath(str + ".lua");
        if (test.exists() && test.isFile())
            return test.canonicalFilePath();
    }

    return QString();
}

QStringList FunctionParser::parseRequiredFiles(QFile &ifile)
{
    static QMap<QString, std::pair<QDateTime, QStringList>> cache;

    auto it = cache.find(ifile.fileName());
    if (it != cache.end())
    {
        QFileInfo info(ifile.fileName());
        const QDateTime &read = it->first;

        // if the read happened later than the last modification, it's up to date
        if (info.lastModified() < read)
        {
            return it->second;
        }
    }

    // read whole content
    ifile.open(QIODevice::ReadOnly | QIODevice::Text);
    QString content = QString::fromLatin1(ifile.readAll());
    ifile.close();

    QStringList parts = content.split(QChar('\n'));

    QStringList result;

    QString functionExpression(R"(.*function\s*((.*)\s*(\(.*\))).*)");

    // search package paths
    QStringList packagePaths;
    QString packagePathExpression(R"EXPR(.*package\s*.\s*path\s*.*"(.*)".*)EXPR");

    for (int i = 0; i < parts.size(); ++i)
    {
        QRegExp regex(packagePathExpression);
        regex.setMinimal(true);

        if (regex.indexIn(parts[i]) != -1)
        {
            QStringList capturedTexts = regex.capturedTexts();
            QString packagePath = capturedTexts[1];

            QStringList pathParts = packagePath.split(';');
            for (const QString &part : pathParts)
            {
                QString trimmed = part.trimmed();

                if (!trimmed.isEmpty())
                    packagePaths.push_back(trimmed);
            }
        }
    }

    // search require("") expressions
    QStringList requires;
    QString requireExpression(R"EXPR(.*require\s*.*"(.*)".*)EXPR");

    for (int i = 0; i < parts.size(); ++i)
    {
        QRegExp regex(requireExpression);
        regex.setMinimal(true);

        if (regex.indexIn(parts[i]) != -1)
        {
            QStringList capturedTexts = regex.capturedTexts();
            QString require = capturedTexts[1];

            requires.push_back(require);
        }
    }

    // resolve all require expressions with the help of package paths
    QFileInfo path = ifile.fileName();
    QDir directory = path.dir();

    // walk up
    do
    {
        QStringList found;
        for (QString require : requires)
        {
            QString path = requireExists(directory, packagePaths, require);
            if (!path.isEmpty())
            {
                result.push_back(path);
                found.push_back(require);
            }
        }

        for (QString &foundRequire : found)
            requires.removeAll(foundRequire);

        if (requires.empty())
            break;
    }
    while(directory.cdUp());

    for (QString str : requires)
    {
        std::cout << "not found: " << str.toStdString() << std::endl;
    }

    // update cache
    cache[ifile.fileName()] = std::make_pair(QDateTime::currentDateTime(), result);

    return result;
}


QStringList FunctionParser::findDependencies(const QString &path)
{
    QSet<QString> scanned;

    QStringList dependencies;
    QStringList todo = {path};

    while(!todo.isEmpty())
    {
        QString current = todo.front();
        todo.pop_front();

        if (scanned.contains(current)) continue;
        scanned.insert(current);

        QFile ifile(current);
        if (!ifile.exists())
            continue;

        dependencies.push_back(current);

        todo.append(parseRequiredFiles(ifile));
    }

    return dependencies;
}

QList<QSharedPointer<FunctionParser::Function>> FunctionParser::parseFunctionsInFileNoRecursion(const QString &path)
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

    for (QString file : findDependencies(path))
    {
        // get words
        result.append(parseFunctionsInFileNoRecursion(file));
    }

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

