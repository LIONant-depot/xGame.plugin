#ifndef XGAME_EDITOR_H
#define XGAME_EDITOR_H
#pragma once

// Game editor: the generic descriptor editor (inspector, undoable edits, save, compile) with the list of script modules the game is made of, each one picked from the project's
// script modules. Compiling the Game (the Compile button, or just saving: the resource pipeline sees the descriptor change) makes the CMake project of the game from the CMake files
// of its modules; the editor builds Game.dll from it.
#include "source/Tools/Editor/xeditor_descriptor_editor.h"
#include "dependencies/xresource_pipeline_v2/source/editor/xresource_editor_inspector_pickers.h"
#include "plugins/xgame.plugin/source/Module/xgame_descriptor.h"

namespace xgame_editor
{
    struct session : xeditor::descriptor_editor
    {
        session(xresource::full_guid Guid, xresource_editor::library::guid LibraryGuid, xgpu::device* pDevice) noexcept
            : descriptor_editor("Game", Guid, LibraryGuid, pDevice)
        {
            m_Document.Load();
            BindDescriptorInspector();
            xresource_editor::WireResourcePickerCallbacks(m_DescriptorInspector.m_Inspector);
            AddPanel("Modules", dock::left, [this] { m_DescriptorInspector.Show(); });
        }
    };

    inline const xeditor::auto_register_resource_editor g_Registration
    { xgame::type_guid_v
    , [](xresource::full_guid Guid, xresource_editor::library::guid LibraryGuid, xgpu::device* pDevice) -> std::unique_ptr<xeditor::resource_editor>
      { return std::make_unique<session>(Guid, LibraryGuid, pDevice); }
    };
}

#endif // XGAME_EDITOR_H
