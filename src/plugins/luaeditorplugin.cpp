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

#include "luaeditorplugin.h"
#include "luaeditorfactory.h"
#include "luaeditorconstants.h"
#include "luafunctionfilter.h"

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/fileiconprovider.h>
#include <utils/mimetypes/mimedatabase.h>
#include <texteditor/texteditorconstants.h>

namespace LuaEditor { namespace Internal {

static LuaEditorPlugin* m_instance = nullptr;

class LuaEditorPluginPrivate : public QObject {
    
    LuaEditorFactory luaEditorFactory;
    LuaFunctionFilter luaFunctionFilter;

    //LuaCompletionAssistProvider luaCompletionAssistProvider;
};

LuaEditorPlugin::LuaEditorPlugin()
{
    m_instance = this;
}

LuaEditorPlugin::~LuaEditorPlugin()
{
    delete d;
    d = nullptr;
    m_instance = nullptr;
}

bool LuaEditorPlugin::initialize(const QStringList &arguments, QString *errorString)
{
    Q_UNUSED(arguments)
    Q_UNUSED(errorString)

    d = new LuaEditorPluginPrivate;

    QString fileName = QLatin1String(":/LuaEditor/LuaEditor.mimetypes.xml");

    QFile file(fileName);
    file.open(QIODevice::ReadOnly);
    QByteArray blob = file.readAll();

    Utils::addMimeTypes(fileName, blob);

    Core::ActionContainer* contextMenu = Core::ActionManager::createMenu(Constants::M_CONTEXT);
#if 0
    ActionContainer* luaToolsMenu = ActionManager::createMenu(Id(Constants::M_TOOLS_LUA));
    luaToolsMenu->setOnAllDisabledBehavior(ActionContainer::Show);

    QMenu* menu = luaToolsMenu->menu();
    menu->setTitle(tr("Lua"));
    ActionManager::actionContainer(Core::Constants::M_TOOLS)->addMenu(luaToolsMenu);
#endif

    contextMenu->addAction(Core::ActionManager::command(TextEditor::Constants::UN_COMMENT_SELECTION));

    if(errorString)
        errorString->clear();
    return true;
}

void LuaEditorPlugin::extensionsInitialized()
{
    Core::FileIconProvider::registerIconOverlayForMimeType(":/LuaEditor/images/luafile.png", Constants::LUA_SOURCE_MIMETYPE);
    Core::FileIconProvider::registerIconOverlayForMimeType(":/LuaEditor/images/luafile.png", Constants::LUA_HEADER_MIMETYPE);
}

ExtensionSystem::IPlugin::ShutdownFlag LuaEditorPlugin::aboutToShutdown()
{
    return IPlugin::aboutToShutdown();
}


} }
