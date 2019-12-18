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
**	File created on: 22/08/2015
*/

#include "luaindenter.h"
#include "scanner/luascanner.h"

#include <texteditor/tabsettings.h>
#include <QSet>
#include <QString>

namespace LuaEditor { namespace Internal {

static QSet<QString> const g_increaseKeywords = {
	QStringLiteral("function"),
	QStringLiteral("do"),
	QStringLiteral("then"),
	QStringLiteral("else"),
	QStringLiteral("repeat")
};
static QSet<QString> const g_decreaseKeywords = {
	QStringLiteral("end"),
	QStringLiteral("until"),
	QStringLiteral("elseif"),
	QStringLiteral("else"),
};

LuaIndenter::LuaIndenter(QTextDocument *doc)
	: TextEditor::TextIndenter(doc)
{}

LuaIndenter::~LuaIndenter(){}

bool LuaIndenter::isElectricCharacter(QChar const& ch) const {
    for(const QString &decreaseKeyword : g_decreaseKeywords){
        if(decreaseKeyword.at(decreaseKeyword.length()-1) == ch)
            return true;
    }
    return false;
}

void LuaIndenter::indentBlock(const QTextBlock &block,
                              const QChar &typedChar,
                              const TextEditor::TabSettings &tabSettings,
                              int /*cursorPositionInEditor*/)
{
	Q_UNUSED(typedChar);

    // If unindent keyword detected, do an unindentation run
    for(const QString &decreaseKeyword: g_decreaseKeywords){
        if(block.text().endsWith(decreaseKeyword)){
            unindentBlockIfNecessary(block, tabSettings);
            return;
        }
    }

	QTextBlock previousBlock = block.previous();

    // Iterate until we find a previous line that contains text
    while(previousBlock.isValid() &&
          (previousBlock.text().trimmed().isEmpty() ||
           previousBlock.text().trimmed().startsWith("--")
          )){
        previousBlock = previousBlock.previous();
    }

    // If we didn't find one, assume indentation of 0
	if(!previousBlock.isValid())
	{
		tabSettings.indentLine(block, 0);
		return;
	}

    QString const& previousBlockText = previousBlock.text();

    // Get the indentation depth of the previous line
	int previousIndentation = tabSettings.indentationColumn(previousBlockText);

	tabSettings.indentLine(block,
		qMax<int>(0,
            previousIndentation
            + qMax(0, getLineDelta(previousBlockText)) * tabSettings.m_indentSize
		)
	);
}

void LuaIndenter::unindentBlockIfNecessary(const QTextBlock &block,
                        const TextEditor::TabSettings &tabSettings){

    // Skip if it wasn't actually a real keyword (e.g. keyword-like text in a string)
    if(!g_decreaseKeywords.contains(getLastKeyword(block.text())))
        return;

    // Get the minimum depth in the current block
    bool isSelfContained = true;
    int depth = 0;
    for(const QString &keyword : getAllKeywords(block.text())){
        // Check decrease first to catch 'else'
        if(g_decreaseKeywords.contains(keyword)){
            depth--;
            if(depth < 0){
                isSelfContained = false;
            }
        }
        if(g_increaseKeywords.contains(keyword)){
            depth++;
        }
    }

    // Don't unindent if the corresponding opening keyword was in the same line
    if(isSelfContained)
        return;

    // Reset depth
    depth = 0;

    // Iterate through previous lines to find starting keyword that is connected
    // to the current ending keyword
    QTextBlock nextBlock = block.previous();
    while(nextBlock.isValid()){
        QVector<QString> keywords = getAllKeywords(nextBlock.text());

        bool isCorrespondingBlock = false;

        for(auto it = keywords.rbegin(); it != keywords.rend(); it++){
            if(g_increaseKeywords.contains(*it)){
                depth--;
                if(depth < 0){
                    isCorrespondingBlock = true;
                }
            }
            if(g_decreaseKeywords.contains(*it)){
                depth++;
            }
        }

        if(isCorrespondingBlock)
            break;

        nextBlock = nextBlock.previous();
    }

    // If we didn't find the corresponding keyword, do nothing
    if(!nextBlock.isValid())
        return;

    QString const& startingBlockText = nextBlock.text();
    int startingBlockIndentation = tabSettings.indentationColumn(startingBlockText);

    int newIndentation = startingBlockIndentation;
    tabSettings.indentLine(block, newIndentation);
}

QVector<QString> LuaIndenter::getAllKeywords(const QString &line) const
{
    QVector<QString> keywords;

    FormatToken thisToken;
	Scanner scannerA(line.constData(),line.size());
	while((thisToken = scannerA.read()).format() != Format_EndOfBlock)
	{
		if(thisToken.format() == Format_Keyword)
			keywords.push_back(scannerA.value(thisToken));
	}

    return keywords;
}

QString LuaIndenter::getLastKeyword(const QString &line) const
{
	QString lastKeyword;
	
	FormatToken thisToken;
	Scanner scannerA(line.constData(),line.size());
	while((thisToken = scannerA.read()).format() != Format_EndOfBlock)
	{
		if(thisToken.format() == Format_Keyword)
			lastKeyword = scannerA.value(thisToken);
	}
	return lastKeyword;
}

int LuaIndenter::getLineDelta(const QString& line) const
{
    if (line.length() == 0) return 0;

    int delta = 0;
    int minDelta = 0;

    FormatToken thisToken;
    Scanner scannerA(line.constData(),line.size());
    while((thisToken = scannerA.read()).format() != Format_EndOfBlock)
    {
        if(thisToken.format() == Format_Keyword){
            const QString keyword = scannerA.value(thisToken);
            // Decrease first, to catch 'else'
            if(g_decreaseKeywords.contains(keyword)){
                delta--;
                if(delta < minDelta)
                    minDelta = delta;
            }
            if(g_increaseKeywords.contains(keyword))
                delta++;
        }
    }

    // Return how much the end depth is higher than the minimum depth.
    // examples:
    // 'else' => 1
    // 'do' => 1
    // 'if a then b end' => 0
    return delta-minDelta;
}

} }

