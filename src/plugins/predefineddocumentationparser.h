#ifndef LUAEDITORPREDEFINEDDOCUMENTATIONPARSER_H
#define LUAEDITORPREDEFINEDDOCUMENTATIONPARSER_H

#include <QMap>
#include <QVector>

#include "luafunctionhintproposalmodel.h"

namespace LuaEditor { namespace Internal {

class PredefinedDocumentationParser
{
public:
    typedef LuaFunctionHintProposalModel::Function Function;

    static void readMembers(QStringList &words, QMap<QString, QStringList> &members, QString path);
    static void readCalls(QStringList &words, QMap<QString, QVector<Function>> &functionsByFunction, QMap<QString, QVector<Function>> &functionsByObject, QString path);
    static void readWords(QStringList &out, QString path);


};

} }

#endif
