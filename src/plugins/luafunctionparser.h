#ifndef LUAEDITORFUNCTIONPARSER_H
#define LUAEDITORFUNCTIONPARSER_H

#include <QString>
#include <QSharedPointer>
#include <QList>

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
    static FunctionList parseFunctionsInFile(const QString &path);
};


}

}


#endif
