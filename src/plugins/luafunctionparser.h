#ifndef LUAEDITORFUNCTIONPARSER_H
#define LUAEDITORFUNCTIONPARSER_H

#include <QString>
#include <QSharedPointer>
#include <QList>
#include <QFile>
#include <QDir>

namespace LuaEditor { namespace Internal {

class FunctionParser
{
public:
    struct Function
    {
        enum SurroundingType
        {
            None = 0,
            Object = 1,
            Module = 2,
        };

        Function() = default;
        Function(QString functionName,
                 QString arguments,
                 QString surroundingName = QString(),
                 SurroundingType surroundingType = SurroundingType::None,
                 QString fileName = QString(),
                 int line = 0);

        QString fullFunction;
        QString functionName;
        QString surroundingName;
        QString arguments;

        SurroundingType surroundingType = SurroundingType::None;

        QString fileName;
        int line = 0;
    };

    typedef QList<QSharedPointer<Function>> FunctionList;

    static FunctionList parseFunctions(const QString &text);
    static FunctionList parseFunctionsInFileNoRecursion(const QString &path);
    static FunctionList parseFunctionsInFile(const QString &path);

    static QStringList findDependencies(const QString &path);
    static QStringList parseRequiredFiles(QFile &ifile);
    static QString requireExists(QDir directory, QStringList packagePaths, QString require);

    static void addLuaLibraryCalls(FunctionList &list);
};


}

}


#endif
