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

#include "luacompletionassistprocessor.h"
#include "luafunctionhintproposalmodel.h"
#include "luafunctionfilter.h"
#include "scanner/luascanner.h"
#include <texteditor/codeassist/assistinterface.h>
#include <texteditor/codeassist/assistproposalitem.h>
#include <texteditor/codeassist/genericproposal.h>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QByteArray>

#include <iostream>

#include <texteditor/codeassist/functionhintproposal.h>
#include <texteditor/codeassist/ifunctionhintproposalmodel.h>

namespace LuaEditor { namespace Internal {

static QStringList const g_special = {
	QStringLiteral("self")
};

static QStringList const g_types = {
	QStringLiteral("local")
};

static QStringList const g_keyword_beginning = {
	QStringLiteral("break"),
	QStringLiteral("do"),
	QStringLiteral("else"),
	QStringLiteral("elseif"),
	QStringLiteral("end"),
	QStringLiteral("for"),
	QStringLiteral("function"),
	QStringLiteral("goto"),
	QStringLiteral("if"),
	QStringLiteral("in"),
	QStringLiteral("repeat"),
	QStringLiteral("return"),
	QStringLiteral("then"),
	QStringLiteral("until"),
	QStringLiteral("while")
};

static QStringList const g_keyword_fcall = {
	QStringLiteral("true"),
	QStringLiteral("false"),
	QStringLiteral("nil"),
	QStringLiteral("and"),
	QStringLiteral("not"),
	QStringLiteral("or")
};

static QStringList const g_magics = {
	QStringLiteral("__index"),
	QStringLiteral("__newindex"),
	QStringLiteral("__concat"),
	QStringLiteral("__call"),
	QStringLiteral("__add"),
	QStringLiteral("__band"),
	QStringLiteral("__bnot"),
	QStringLiteral("__bor"),
	QStringLiteral("__bxor"),
	QStringLiteral("__div"),
	QStringLiteral("__eq"),
	QStringLiteral("__idiv"),
	QStringLiteral("__le"),
	QStringLiteral("__len"),
	QStringLiteral("__lt"),
	QStringLiteral("__mod"),
	QStringLiteral("__mul"),
	QStringLiteral("__pow"),
	QStringLiteral("__shl"),
	QStringLiteral("__shr"),
	QStringLiteral("__sub"),
	QStringLiteral("__unm")
};

LuaCompletionAssistProcessor::LuaCompletionAssistProcessor()
	: m_startPosition(0),
	  m_varIcon(QLatin1String(":/LuaEditor/images/var.png")),
	  m_functionIcon(QLatin1String(":/LuaEditor/images/func.png")),
	  m_memIcon(QLatin1String(":/LuaEditor/images/attributes.png")),
      m_keywordIcon(QLatin1String(":/LuaEditor/images/keyword.png"))
{
    readWords(predefinedMembers, QDir::homePath() + QString::fromLatin1("/.avorion/documentation/completion/members"));
    readWords(predefinedWords, QDir::homePath() + QString::fromLatin1("/.avorion/documentation/completion/words"));
    readCalls(predefinedCalls, predefinedFunctionInfos, QDir::homePath() + QString::fromLatin1("/.avorion/documentation/completion/calls"));
}

LuaCompletionAssistProcessor::~LuaCompletionAssistProcessor()
{
}

void LuaCompletionAssistProcessor::readCalls(QStringList &words, QMap<QString, QVector<Function>> &functions, QString path)
{
    QFile ifile(path);
    if (!ifile.exists())
        return;

    static QMap<QString, std::tuple<QDateTime, QStringList, QMap<QString, QVector<Function>>>> dates;

    auto it = dates.find(path);
    if (it != dates.end())
    {
        QFileInfo info(path);
        const QDateTime &read = std::get<0>(*it);

        // if the read happened later than the last modification, it's up to date
        if (info.lastModified() < read)
        {
            words = std::get<1>(*it);
            functions = std::get<2>(*it);
            return;
        }
    }

    words.clear();
    functions.clear();

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

        functions[functionNameNoObject].push_back(function);
        words.push_back(functionNameNoObject);
    }


    // update cache
    dates[path] = std::make_tuple(QDateTime::currentDateTime(), words, functions);
}

void LuaCompletionAssistProcessor::readWords(QStringList &out, QString path)
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

static TextEditor::AssistProposalItem* createCompletionItem(QString const& text, QIcon const& icon, int order =0)
{
	TextEditor::AssistProposalItem* item = new TextEditor::AssistProposalItem;
    item->setText(text);
    item->setIcon(icon);
    item->setOrder(order);
	return item;
}

struct PriorityList {
    PriorityList(QString const& s, int pr)
        : m_str{s}, m_pr(pr) {}
    PriorityList(QStringList const& s, int pr)
        : m_str(s), m_pr(pr) {}

    QStringList m_str;
    int m_pr;
};

TextEditor::IAssistProposal* LuaCompletionAssistProcessor::perform(const TextEditor::AssistInterface *interface)
{
    if (TextEditor::IAssistProposal *proposal = tryCreateFunctionHintProposal(interface))
        return proposal;

    return createContentProposal(interface);
}

TextEditor::IAssistProposal *LuaCompletionAssistProcessor::tryCreateFunctionHintProposal(const TextEditor::AssistInterface *interface)
{
    int pos = interface->position() - 1;
    QChar ch = interface->characterAt(pos);

    while (ch.isSpace())
    {
        ch = interface->characterAt(--pos);
    }

    if (ch != QChar(',') && ch != QChar('('))
        return nullptr;

    QString functionName;
    bool isParameterList = false;
    int beginningOfFunctionName = pos;

    // check if we're inside a function parameter list
    // walk backwards, count parentheses
    {
        int p = pos;
        int parentheses = 0;
        int brackets = 0;
        int curly = 0;

        while (p >= 0)
        {
            QChar c = interface->characterAt(p);

            if (c == QChar(')')) parentheses++;
            else if (c == QChar('(')) parentheses--;
            else if (c == QChar(']')) brackets++;
            else if (c == QChar('[')) brackets--;
            else if (c == QChar('}')) curly++;
            else if (c == QChar('{')) curly--;

            p--;

            if (curly == 0 && brackets == 0 && parentheses == -1)
            {
                break;
            }
        }

        if (curly == 0 && brackets == 0 && parentheses == -1)
        {
            // we're now outside the parameter list
            // characters that are okay now are: . : spaces letters and numbers
            bool lastWasWord = false;
            QVector<QString> words;
            QString currentWord;

            while (p >= 0)
            {
                QChar c = interface->characterAt(p);
                p--;

                // everything else cancels
                if (!(c.isSpace() || c.isLetterOrNumber()
                        || c == QChar('_')
                        || c == QChar('.')
                        || c == QChar(':')))
                {
                    // push what we collected so far
                    if (words.isEmpty()) beginningOfFunctionName = p + 1;
                    words.push_back(currentWord);
                    break;
                }

                // whitespace indicates end of a word
                if (c.isSpace())
                {
                    if (!currentWord.isEmpty())
                    {
                        if (words.isEmpty()) beginningOfFunctionName = p + 1;
                        words.push_back(currentWord);
                        currentWord.clear();

                        if (lastWasWord) break;
                        lastWasWord = true;
                    }

                    // apart from ending words, skip whitespaces
                    continue;
                }

                if (c.isLetterOrNumber() || c == QChar('_'))
                {
                    currentWord.prepend(c);
                }
                else
                {
                    // separators end words as well
                    if (!currentWord.isEmpty())
                    {
                        if (words.isEmpty()) beginningOfFunctionName = p + 1;
                        words.push_back(currentWord);
                        currentWord.clear();

                        if (lastWasWord) break;
                        lastWasWord = true;
                    }

                    if (!lastWasWord) break;
                    lastWasWord = false;
                }
            }

            // if the first or last word is "function" we're inside a function declaration parameter list
            if (!words.isEmpty()
                    && words.front() != QString("function")
                    && words.back() != QString("function"))
            {
                functionName = words.front();
                isParameterList = true;
            }
        }
    }

    if (isParameterList)
    {
        QVector<LuaFunctionHintProposalModel::Function> functions;

        QList<QSharedPointer<LuaFunctionFilter::FunctionEntry>> parsedFunctions = LuaFunctionFilter::parseFunctions(interface->textDocument()->toPlainText());
        for (QSharedPointer<LuaFunctionFilter::FunctionEntry> &parsedFunction : parsedFunctions)
        {
            if (parsedFunction->functionName == functionName)
            {
                LuaFunctionHintProposalModel::Function function;

                if (parsedFunction->surroundingType == LuaFunctionFilter::SurroundingType::Module)
                    function.m_functionName = parsedFunction->surroundingName + QString(".") + parsedFunction->functionName;
                else if (parsedFunction->surroundingType == LuaFunctionFilter::SurroundingType::Object)
                    function.m_functionName = parsedFunction->surroundingName + QString(":") + parsedFunction->functionName;
                else
                    function.m_functionName = parsedFunction->functionName;

                QString trimmed = parsedFunction->arguments.trimmed();
                trimmed = trimmed.mid(1, trimmed.size() - 2);

                auto parts = trimmed.split(QChar(','));
                for (const QString &part : parts)
                {
                    function.m_arguments.push_back(part.trimmed());
                }

                functions.push_back(function);
            }
        }

        // check all predefined functions
        auto it = predefinedFunctionInfos.find(functionName);
        if (it != predefinedFunctionInfos.end())
        {
            for (const Function &function : *it)
            {
                functions.push_back(function);
            }
        }

        TextEditor::IFunctionHintProposalModel *model =
                new LuaFunctionHintProposalModel(std::move(functions));

        return new TextEditor::FunctionHintProposal(beginningOfFunctionName + 1, model);
    }

    return nullptr;
}

TextEditor::GenericProposal *LuaCompletionAssistProcessor::createContentProposal(const TextEditor::AssistInterface *interface)
{
    if(interface->reason() == TextEditor::IdleEditor && !acceptsIdleEditor())
        return 0;

    int pos = interface->position() - 1;
    QChar ch = interface->characterAt(pos);

    while(ch.isSpace())
    {
        ch = interface->characterAt(--pos);
    }

    bool isFunctionCall = (ch == QLatin1Char('(')) || (ch == QLatin1Char(','));
    bool isMemberCompletion = (ch == QLatin1Char('.'));
    bool isFunctionCompletion = (ch == QLatin1Char(':'));
    bool isWordCompletion = !isFunctionCall && !isMemberCompletion && !isFunctionCompletion;

    QString currentMember;

    {
        int cpos = pos-1;
        while((cpos >= 0) && interface->characterAt(cpos).isSpace())
            --cpos;
        int cpos_end = cpos;

        while((cpos >= 0) && (interface->characterAt(cpos).isLetterOrNumber() || interface->characterAt(cpos) == QLatin1Char('_')))
            --cpos;
        ++cpos; ++cpos_end;

        if (isWordCompletion)
            ++cpos_end;

        currentMember = interface->textAt(cpos, cpos_end-cpos);
    }

    QList<PriorityList> globVariables;
    QList<PriorityList> variables;
    QList<PriorityList> keywords;
    QList<PriorityList> magics;

    RecursiveClassMembers targetIds;
    Scanner::TakeBackwardsState(interface->textDocument()->findBlockByLineNumber(interface->textDocument()->findBlock(interface->position()).firstLineNumber()-1),&targetIds);

    if(isMemberCompletion || isFunctionCompletion)
    {
        RecursiveClassMembers writtenTargetId;
        Scanner::TakeBackwardsMember(interface->textDocument()->findBlock(interface->position()),writtenTargetId);

        RecursiveClassMembers* deepest = &writtenTargetId;
        for(;;)
        {
            RecursiveClassMembers::iterator it = deepest->begin();
            if(it == deepest->end())
                break;
            deepest = &(*it);
        }

        RecursiveClassMembers* mem = targetIds.matchesChilds(deepest->buildDirectory());

        if(mem)
        {
            for(auto it = mem->begin(); it != mem->end(); ++it)
            {
                variables.append({it->key(),4});
            }
        }
        for(auto it = targetIds.begin(); it != targetIds.end(); ++it)
        {
            globVariables.append({it->key(),3});
        }

        if (isFunctionCompletion)
            globVariables.append({predefinedCalls, 2});

        if (isMemberCompletion)
            globVariables.append({predefinedMembers, 2});

        globVariables.append({g_special,1});
        magics.append({g_magics,0});
    }
    else if (isWordCompletion)
    {
        QString lowerWord = currentMember.toLower();

        for (auto it = targetIds.begin(); it != targetIds.end(); ++it)
        {
            if (it->key().toLower().startsWith(lowerWord))
                variables.append({it->key(),5});
        }

        for (const QString &str : predefinedWords)
        {
            if (str.toLower().startsWith(lowerWord))
                variables.append({str, 4});
        }
        for (const QString &str : g_special)
        {
            if (str.toLower().startsWith(lowerWord))
                variables.append({str, 3});
        }
        for (const QString &str : g_types)
        {
            if (str.toLower().startsWith(lowerWord))
                variables.append({str, 2});
        }
        for (const QString &str : g_keyword_beginning)
        {
            if (str.toLower().startsWith(lowerWord))
                variables.append({str, 1});
        }
        for (const QString &str : g_keyword_fcall)
        {
            if (str.toLower().startsWith(lowerWord))
                variables.append({str, 0});
        }
    }
    else
    {
        for(auto it = targetIds.begin(); it != targetIds.end(); ++it)
        {
            variables.append({it->key(),4});
        }
        variables.append({g_special,3});
        keywords.append({g_types,2});
        keywords.append({g_keyword_beginning,1});
        keywords.append({g_keyword_fcall,0});
    }

    QList<TextEditor::AssistProposalItemInterface*> m_completions;

    QSet<QString> m_usedSuggestions;

    for(auto it = variables.begin(); it != variables.end(); ++it)
    {
        PriorityList const& plit = *it;
        for(auto itb = plit.m_str.begin(); itb != plit.m_str.end(); ++itb)
        {
            if(!m_usedSuggestions.contains(*itb))
            {
                m_completions << createCompletionItem(*itb, m_memIcon, plit.m_pr);
                m_usedSuggestions.insert(*itb);
            }
        }
    }
    for(auto it = globVariables.begin(); it != globVariables.end(); ++it)
    {
        PriorityList const& plit = *it;
        for(auto itb = plit.m_str.begin(); itb != plit.m_str.end(); ++itb)
        {
            if(!m_usedSuggestions.contains(*itb))
            {
                m_completions << createCompletionItem(*itb, m_varIcon, plit.m_pr);
                m_usedSuggestions.insert(*itb);
            }
        }
    }
    for(auto it = keywords.begin(); it != keywords.end(); ++it)
    {
        PriorityList const& plit = *it;
        for(auto itb = plit.m_str.begin(); itb != plit.m_str.end(); ++itb)
        {
            if(!m_usedSuggestions.contains(*itb))
            {
                m_completions << createCompletionItem(*itb, m_keywordIcon, plit.m_pr);
                m_usedSuggestions.insert(*itb);
            }
        }
    }
    for(auto it = magics.begin(); it != magics.end(); ++it)
    {
        PriorityList const& plit = *it;
        for(auto itb = plit.m_str.begin(); itb != plit.m_str.end(); ++itb)
        {
            if(!m_usedSuggestions.contains(*itb))
            {
                m_completions << createCompletionItem(*itb, m_functionIcon, plit.m_pr);
                m_usedSuggestions.insert(*itb);
            }
        }
    }

    m_startPosition = pos+1;

    if (isWordCompletion)
    {
        m_startPosition -= currentMember.length();
    }

    return new TextEditor::GenericProposal(m_startPosition, m_completions);
}


bool LuaCompletionAssistProcessor::acceptsIdleEditor() const { return false; }

} }
