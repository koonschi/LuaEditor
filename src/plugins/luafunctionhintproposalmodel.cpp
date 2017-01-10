/*	Copyright (c) 2015 SGH
**	
**	Permission is granted to use, modify and redistribute this software.
**	Modified versions of this software MUST be marked as such.
**	
**	This software is provided "AS IS". In no event shall
**	the authors or copyright holders be liable for any claim,
**	damages or other liability. The above copyright notice
**	and this permission notice shall be included in all copies
**	or substantial portions of the software.
**	
**	File created on: 23/08/2015
*/

#include "luafunctionhintproposalmodel.h"
#include "scanner/luascanner.h"

#include <iostream>

namespace LuaEditor { namespace Internal {


inline std::ostream &operator<<(std::ostream &os, const QString &str)
{
    os << str.toStdString();
    return os;
}


LuaFunctionHintProposalModel::Function::Function(const QString &functionName, const QString &returnType, QVector<QString> &&arguments) :
    m_functionName(functionName),
    m_returnType(returnType),
    m_arguments(std::move(arguments))
{

}

LuaFunctionHintProposalModel::LuaFunctionHintProposalModel(QVector<LuaFunctionHintProposalModel::Function> &&functions) :
    m_functions(std::move(functions))
{

}

void LuaFunctionHintProposalModel::reset()
{

}

int LuaFunctionHintProposalModel::size() const
{
    return m_functions.size();
}

QString LuaFunctionHintProposalModel::text(int index) const
{
    const Function &hintedFunction = m_functions.at(index);

    QString hintText;

    hintText += hintedFunction.m_returnType.toHtmlEscaped();
    hintText += QString(" ");
    hintText += hintedFunction.m_functionName.toHtmlEscaped();
    hintText += QString("(");

    for (int i = 0; i < hintedFunction.m_arguments.size(); ++i)
    {
        if (i > 0)
            hintText += QString(", ");

        if (i == m_currentArgument)
        {
            hintText += QString("<b>");
            hintText += hintedFunction.m_arguments[i].toHtmlEscaped();
            hintText += QString("</b>");
        }
        else
        {
            hintText += hintedFunction.m_arguments[i].toHtmlEscaped();
        }
    }

    hintText += QString(")");

    return hintText;
}

int LuaFunctionHintProposalModel::activeArgument(const QString &prefix) const
{
    int parentheses = -1; // start at -1, we'll enter the parameter list during the loop
    int brackets = 0;
    int curly = 0;
    bool string = false;

    int argument = 0;

    for (const QChar &c : prefix)
    {
        if (c == '\"') string = !string;

        if (string) continue;

        if (c == QChar('(')) parentheses++;
        else if (c == QChar(')')) parentheses--;
        else if (c == QChar('[')) brackets++;
        else if (c == QChar(']')) brackets--;
        else if (c == QChar('{')) curly++;
        else if (c == QChar('}')) curly--;
        else if (c == QChar(',')
                 && curly == 0
                 && brackets == 0
                 && parentheses == 0)
        {
            argument++;
        }
    }

    if (!(curly == 0 && brackets == 0 && parentheses == 0))
        argument = -1;

    m_currentArgument = argument;

    return argument;
}

} }
