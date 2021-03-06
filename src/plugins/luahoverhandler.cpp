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
**	File created on: 16/08/2015
*/

#include "luahoverhandler.h"
#include <texteditor/texteditor.h>
#include <utils/executeondestruction.h>

namespace LuaEditor { namespace Internal {

void LuaHoverHandler::identifyMatch(TextEditor::TextEditorWidget *editorWidget, int pos, ReportPriority report)
{
    Utils::ExecuteOnDestruction reportPriority([this, report](){ report(priority()); });

	if(!editorWidget->extraSelectionTooltip(pos).isEmpty())
		setToolTip(editorWidget->extraSelectionTooltip(pos));
}

} }
