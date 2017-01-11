#include "predefineddocumentationparser.h"

#include <QFile>
#include <QString>
#include <QFileInfo>
#include <QFileInfo>
#include <QDateTime>

namespace LuaEditor { namespace Internal {

void PredefinedDocumentationParser::readMembers(QStringList &words, QMap<QString, QStringList> &members, QString path)
{
    QFile ifile(path);
    if (!ifile.exists())
        return;

    static QMap<QString, std::tuple<QDateTime, QStringList, QMap<QString, QStringList>>> dates;

    auto it = dates.find(path);
    if (it != dates.end())
    {
        QFileInfo info(path);
        const QDateTime &read = std::get<0>(*it);

        // if the read happened later than the last modification, it's up to date
        if (info.lastModified() < read)
        {
            words = std::get<1>(*it);
            members = std::get<2>(*it);
            return;
        }
    }

    words.clear();
    members.clear();

    // read whole content
    ifile.open(QIODevice::ReadOnly | QIODevice::Text);
    QString content = QString::fromLatin1(ifile.readAll());
    ifile.close();

    // parse function signatures
    auto lines = content.split(QString::fromLatin1("\n"));
    lines.removeAll(QString(""));

    for (const QString &str : lines)
    {
        QStringList parts = str.split(" ");
        if (parts.isEmpty()) continue;

        // first item is the object type
        QString type = parts.front();
        parts.pop_front();

        // rest are the members
        members[type] = parts;

        for (QString &str : parts)
        {
            words.push_back(str);
        }
    }


    // update cache
    dates[path] = std::make_tuple(QDateTime::currentDateTime(), words, members);
}

void PredefinedDocumentationParser::readCalls(QStringList &words, QMap<QString, QVector<Function>> &functionsByFunction, QMap<QString, QVector<Function>> &functionsByObject, QString path)
{
    QFile ifile(path);
    if (!ifile.exists())
        return;

    static QMap<QString, std::tuple<QDateTime, QStringList, QMap<QString, QVector<Function>>, QMap<QString, QVector<Function>>>> dates;

    auto it = dates.find(path);
    if (it != dates.end())
    {
        QFileInfo info(path);
        const QDateTime &read = std::get<0>(*it);

        // if the read happened later than the last modification, it's up to date
        if (info.lastModified() < read)
        {
            words = std::get<1>(*it);
            functionsByFunction = std::get<2>(*it);
            functionsByObject = std::get<3>(*it);
            return;
        }
    }

    words.clear();
    functionsByFunction.clear();

    // read whole content
    ifile.open(QIODevice::ReadOnly | QIODevice::Text);
    QString content = QString::fromLatin1(ifile.readAll());
    ifile.close();

    // parse function signatures
    auto lines = content.split(QString::fromLatin1("\n"));
    lines.removeAll(QString(""));

    for (const QString &str : lines)
    {
        QStringList parts = str.split("|");
        if (parts.isEmpty()) continue;

        Function function;
        function.m_functionName = parts.at(0);
        if (parts.size() > 1)
            function.m_returnType = parts.at(1);

        for (int i = 2; i < parts.size(); ++i)
            function.m_arguments.push_back(parts.at(i));

        QString functionNameNoObject = parts.at(0).split(":").back();

        functionsByFunction[functionNameNoObject].push_back(function);

        QString object = parts.at(0).split(":").front();
        if (object.contains(" ")) object = object.split(" ").back();

        function.m_functionName = functionNameNoObject;
        functionsByObject[object].push_back(function);

        words.push_back(functionNameNoObject);
    }


    // update cache
    dates[path] = std::make_tuple(QDateTime::currentDateTime(), words, functionsByFunction, functionsByObject);
}

void PredefinedDocumentationParser::readWords(QStringList &out, QString path)
{
    QFile ifile(path);
    if (!ifile.exists())
        return;

    static QMap<QString, std::pair<QDateTime, QStringList>> dates;

    auto it = dates.find(path);
    if (it != dates.end())
    {
        QFileInfo info(path);
        const QDateTime &read = it->first;

        // if the read happened later than the last modification, it's up to date
        if (info.lastModified() < read)
        {
            out = it->second;
            return;
        }
    }

    ifile.open(QIODevice::ReadOnly | QIODevice::Text);

    // read whole content
    QString content = QString::fromLatin1(ifile.readAll());

    ifile.close();

    // get words
    out = content.split(QString::fromLatin1("\n"));
    out.removeAll(QString(""));

    // update cache
    dates[path] = std::make_pair(QDateTime::currentDateTime(), out);
}

} }
