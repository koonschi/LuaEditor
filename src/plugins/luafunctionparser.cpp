#include "luafunctionparser.h"

#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QMap>
#include <QSet>
#include <QDir>

#include <iostream>

namespace LuaEditor { namespace Internal {

FunctionParser::Function::Function(QString functionName,
                                   QString arguments,
                                   QString surroundingName,
                                   FunctionParser::Function::SurroundingType surroundingType,
                                   QString fileName,
                                   int line) :
    functionName(functionName),
    surroundingName(surroundingName),
    arguments(arguments),
    surroundingType(surroundingType),
    fileName(fileName),
    line(line)
{
    // this is actually not necessary at the moment since this constructor is only used for library functions
    // and the fullFunction is only evaluated when searching for functions in the filter, where library functions should not be listed
    // but for the sake of completeness, I still put it in...
    if (!surroundingName.isEmpty() && surroundingType != SurroundingType::None)
    {
        fullFunction += surroundingName;
        if (surroundingType == SurroundingType::Module)
            fullFunction += QString(".");
        else if (surroundingType == SurroundingType::Object)
            fullFunction += QString(":");
    }

    if (!this->arguments.startsWith("(") && !this->arguments.endsWith(")"))
    {
        this->arguments = QString("(") + this->arguments + QString(")");
    }

    fullFunction += this->arguments;
}

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
    addLuaLibraryCalls(result);

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

void FunctionParser::addLuaLibraryCalls(FunctionList &list)
{
    // taken from the lua 5.3 documentation at https://www.lua.org/manual/5.3/manual.html
    // optional arguments are modified slighty for better readability when hinting
    // assert(v [, message]) -> assert(v, [message])
    // if the user doesn't know how to use them he should check the documentation anyways...
    static FunctionList libraryFunctions =
    {
        // Basic functions
        QSharedPointer<Function>(new Function("assert", "v, [message]")),
        QSharedPointer<Function>(new Function("collectgarbage", "[opt], [arg]")),
        QSharedPointer<Function>(new Function("dofile", "[filename]")),
        QSharedPointer<Function>(new Function("error", "message, [level]")),
        QSharedPointer<Function>(new Function("getmetatable", "object")),
        QSharedPointer<Function>(new Function("ipairs", "t")),
        QSharedPointer<Function>(new Function("pairs", "t")),
        QSharedPointer<Function>(new Function("load", "[chunk], [chunkname], [mode], [env]")),
        QSharedPointer<Function>(new Function("loadfile", "[filename], [mode], [env]")),
        QSharedPointer<Function>(new Function("next", "table, [index]")),
        QSharedPointer<Function>(new Function("pcall", "f, [arg1], [...]")),
        QSharedPointer<Function>(new Function("print", "...")),
        QSharedPointer<Function>(new Function("rawequal", "v1, v2")),
        QSharedPointer<Function>(new Function("rawget", "table, index")),
        QSharedPointer<Function>(new Function("rawlen", "v")),
        QSharedPointer<Function>(new Function("rawset", "table, index, value")),
        QSharedPointer<Function>(new Function("select", "index, ...")),
        QSharedPointer<Function>(new Function("setmetatable", "table, metatable")),
        QSharedPointer<Function>(new Function("tonumber", "e, [base]")),
        QSharedPointer<Function>(new Function("tostring", "v")),
        QSharedPointer<Function>(new Function("type", "v")),
        QSharedPointer<Function>(new Function("xpcall", "f, msgh, [arg1], [...]")),

        // Coroutine manipulation
        QSharedPointer<Function>(new Function("create", "f", "coroutine", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("isyieldable", "", "coroutine", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("resume", "co, [val1], [...]", "coroutine", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("running", "", "coroutine", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("status", "co", "coroutine", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("wrap", "f", "coroutine", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("yield", "...", "coroutine", Function::SurroundingType::Module)),

        // Modules
        QSharedPointer<Function>(new Function("require", "modname")),
        QSharedPointer<Function>(new Function("loadlib", "libname, funcname", "package", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("searchpath", "name, path, [sep], [rep]", "package", Function::SurroundingType::Module)),
        // todo: package.config package.cpath package.loaded package.path package.preload package.searchers

        // String Manipulation
        QSharedPointer<Function>(new Function("byte", "s, [i], [j]", "string", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("char", "...", "string", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("dump", "function, [strip]", "string", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("find", "s, pattern, [init], [plain]", "string", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("format", "formatstring, ...", "string", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("gmatch", "s, pattern", "string", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("gsub", "s, pattern, repl, [n]", "string", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("len", "s", "string", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("lower", "s", "string", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("match", "s, pattern, [init]", "string", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("pack", "fmt, v1, ...", "string", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("packsize", "fmt", "string", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("rep", "s, n, [sep]", "string", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("reverse", "s", "string", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("sub", "s, i, [j]", "string", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("unpack", "fmt, s, [pos]", "string", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("upper", "s", "string", Function::SurroundingType::Module)),

        // UTF8-Support
        QSharedPointer<Function>(new Function("char", "...", "utf8", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("codes", "s", "utf8", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("codepoint", "s, [i], [j]", "utf8", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("len", "s, [i], [j]", "utf8", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("offset", "s, [i], [j]", "utf8", Function::SurroundingType::Module)),
        // todo: utf8.charpattern

        // Table Manipulation
        QSharedPointer<Function>(new Function("concat", "list, [sep], [i], [j]", "table", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("insert", "list, [pos], value", "table", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("move", "a1, f, e, t, [a2]", "table", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("remove", "list, [comp]", "table", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("unpack", "list, [i], [j]", "table", Function::SurroundingType::Module)),

        // Mathematical Functions
        QSharedPointer<Function>(new Function("abs", "x", "math", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("acos", "x", "math", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("asin", "x", "math", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("atan", "x, [y]", "math", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("ceil", "x", "math", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("cos", "x", "math", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("deg", "x", "math", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("exp", "x", "math", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("floor", "x", "math", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("fmod", "x, y", "math", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("log", "x, [base]", "math", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("max", "x, ...", "math", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("modf", "x", "math", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("rad", "x", "math", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("random", "[m], [n]", "math", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("randomseed", "x", "math", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("sin", "x", "math", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("sqrt", "x", "math", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("tan", "x", "math", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("tointeger", "x", "math", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("ult", "m, n", "math", Function::SurroundingType::Module)),
        // todo: math.huge math.maxinteger math.mininteger math.pi

        // IO
        QSharedPointer<Function>(new Function("close", "[file]", "io", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("flush", "", "io", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("input", "[file]", "io", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("lines", "[filename], [...]", "io", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("open", "filename, [mode]", "io", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("output", "[file]", "io", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("popen", "prog, [model]", "io", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("read", "...", "io", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("tmpfile", "", "io", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("type", "obj", "io", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("write", "...", "io", Function::SurroundingType::Module)),

        QSharedPointer<Function>(new Function("close", "", "file", Function::SurroundingType::Object)),
        QSharedPointer<Function>(new Function("flush", "", "file", Function::SurroundingType::Object)),
        QSharedPointer<Function>(new Function("lines", "...", "file", Function::SurroundingType::Object)),
        QSharedPointer<Function>(new Function("read", "...", "file", Function::SurroundingType::Object)),
        QSharedPointer<Function>(new Function("seek", "[whence], [offset]", "file", Function::SurroundingType::Object)),
        QSharedPointer<Function>(new Function("setvbuf", "mode, [size]", "file", Function::SurroundingType::Object)),
        QSharedPointer<Function>(new Function("write", "...", "file", Function::SurroundingType::Object)),

        // OS
        QSharedPointer<Function>(new Function("clock", "", "os", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("date", "[format], [time]", "os", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("difftime", "t2, t1", "os", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("execute", "[command]", "os", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("exit", "[code], [close]", "os", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("getenv", "varname", "os", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("remove", "filename", "os", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("rename", "oldname, newname", "os", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("setlocale", "locale, [category]", "os", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("time", "[table]", "os", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("tmpname", "", "os", Function::SurroundingType::Module)),

        // Debug
        QSharedPointer<Function>(new Function("debug", "", "debug", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("gethook", "[thread]", "debug", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("getinfo", "[thread], f, [what]", "debug", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("getlocal", "[thread], f, local", "debug", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("getmetatable", "value", "debug", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("getregistry", "", "debug", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("getupvalue", "f, up", "debug", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("getuservalue", "u", "debug", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("sethook", "[thread], hook, mask, [count]", "debug", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("setlocal", "[thread], level, local, value", "debug", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("setmetatable", "value, table", "debug", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("setupvalue", "f, up, value", "debug", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("setuservalue", "udata, value", "debug", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("traceback", "[thread], [message], [level]", "debug", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("upvalueid", "f, n", "debug", Function::SurroundingType::Module)),
        QSharedPointer<Function>(new Function("upvaluejoin", "f1, n1, f2, n2", "debug", Function::SurroundingType::Module)),

    };

    list.append(libraryFunctions);
}



}

}

