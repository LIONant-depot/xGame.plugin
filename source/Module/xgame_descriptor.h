#ifndef XGAME_DESCRIPTOR_H
#define XGAME_DESCRIPTOR_H
#pragma once

// The Game resource: the game's code, as the list of the script modules it is made of. It replaces the project's Script.config.txt list: the resource pipeline
// compiles it (see xgame_compiler) into the CMake project of the game (<project>/Cache/Script/<Game guid>/CMakeLists.txt) from the CMake files that the ScriptModule compiler wrote for
// each module, so a cleared cache, a new checkout or a build without the editor all get the project from the same rule that makes every other compiled resource.
//
//      <resource>.desc/info.txt         what every resource has
//      <resource>.desc/Descriptor.txt   this descriptor: the modules, in the order they are built
//
// No UI and no editor in here: the compiler, the editor and the pipe commands all include this one header.
#include "plugins/xscript_module.plugin/source/Module/xscript_module_files.h"

namespace xgame
{
    inline constexpr auto type_guid_v = xresource::type_guid(0xA3F1D6C0452E9B17ull);      // "Game": must match the TypeGUID of the plugin's resource_pipeline.config.txt
    using game_ref = xresource::def_guid<type_guid_v>;

    struct descriptor : xresource_pipeline::descriptor::base
    {
        void SetupFromSource(std::string_view) override {}
        void Validate(std::vector<std::string>& Errors) const noexcept override;

        std::vector<xscript::module::module_ref> m_Modules;         // the script modules of the game, in the order they are built

        XPROPERTY_VDEF
        ( "Game", descriptor
        , obj_member<"Modules", &descriptor::m_Modules, member_ui_open<true>, member_flags<flags::SMALL_RESOURCE>>
        )
    };
    XPROPERTY_VREG(descriptor)

    inline void descriptor::Validate(std::vector<std::string>& Errors) const noexcept
    {
        for (std::size_t i = 0; i < m_Modules.size(); ++i)
        {
            if (m_Modules[i].empty()) { Errors.push_back(std::format("Module #{} is not assigned", i + 1)); continue; }
            for (std::size_t j = 0; j < i; ++j)
                if (m_Modules[j] == m_Modules[i]) { Errors.push_back(std::format("Module #{} is listed twice", i + 1)); break; }
        }
    }

    struct factory final : xresource_pipeline::factory_base
    {
        using xresource_pipeline::factory_base::factory_base;
        std::unique_ptr<xresource_pipeline::descriptor::base> CreateDescriptor() const noexcept override { return std::make_unique<descriptor>(); }
        xresource::type_guid ResourceTypeGUID() const noexcept override { return type_guid_v; }
        const char* ResourceTypeName() const noexcept override { return "Game"; }
        const xproperty::type::object& ResourceXPropertyObject() const noexcept override { return *xproperty::getObjectByType<descriptor>(); }
    };
    namespace details { struct factory_holder { inline static factory s_Instance{}; }; }

    // {Project}\Project.config\Script.config.txt: the project's old default Game. Nothing reads it any more (the editor does not, the compiler never did): a Level names the Game it runs under, and
    // every Game resource of the project is compiled into its own game project (Cache/Script/<guid>/). Kept so that the file of an old project still has a reader that says what it was.
    struct script_config
    {
        // The Game resource the editor loads at startup.
        game_ref                            m_Game = {};

        // What this file said before the Game resource existed: the script modules of the project, as a bare list of guids. Read once and moved into a Game resource by the editor
        // (MigrateScriptConfig), then empty. Not shown in the settings page.
        std::vector<xresource::full_guid>   m_ModuleRefs = {};

        XPROPERTY_DEF
        ( "ScriptConfig", script_config
        , obj_member<"Game",       &script_config::m_Game,       member_help<"The project's default Game: the one the editor builds and loads when it starts. A Level runs under the Game it names (SetLevelGame), never under this one by default. Create Games in the Asset Browser (type Game).">>
        , obj_member<"ModuleRefs", &script_config::m_ModuleRefs, member_flags<flags::DONT_SHOW>>
        )
    };
    XPROPERTY_REG(script_config)

    inline xerr SaveScriptConfig(const std::wstring& ProjectPath, const script_config& Config) noexcept
    {
        const auto ConfigFolder = std::format(L"{}\\Project.config", ProjectPath);
        if (!std::filesystem::exists(ConfigFolder)) std::filesystem::create_directories(ConfigFolder);

        xtextfile::stream Stream;
        if (auto Err = Stream.Open(false, std::format(L"{}\\Script.config.txt", ConfigFolder), xtextfile::file_type::TEXT); Err)
            return Err;

        xproperty::settings::context Context;
        return xproperty::sprop::serializer::Stream(Stream, const_cast<script_config&>(Config), Context);
    }

    // Missing file (nothing saved yet) is not an error - Config is left default (empty).
    inline xerr LoadScriptConfig(const std::wstring& ProjectPath, script_config& Config) noexcept
    {
        xtextfile::stream Stream;
        if (auto Err = Stream.Open(true, std::format(L"{}\\Project.config\\Script.config.txt", ProjectPath), xtextfile::file_type::TEXT); Err)
            return {};

        xproperty::settings::context Context;
        return xproperty::sprop::serializer::Stream(Stream, Config, Context);
    }

    // Reading and writing the descriptor of a Game resource (its .desc folder).
    inline std::filesystem::path DescriptorFile(const std::filesystem::path& DescFolder) noexcept { return DescFolder / L"Descriptor.txt"; }

    inline bool Read(const std::filesystem::path& DescFolder, descriptor& Out, std::string* pError = nullptr) noexcept
    {
        std::error_code Ec;
        const auto File = DescriptorFile(DescFolder);
        if (!std::filesystem::exists(File, Ec)) { if (pError) *pError = "no Descriptor.txt"; return false; }
        descriptor Fresh;
        std::string Why;
        const bool bRead = xscript::module::Retry([&]                       // the pipeline looks at Descriptor.txt for a moment now and then: see Retry
        {
            xproperty::settings::context Context;
            if (auto Err = Fresh.Serialize(true, File.wstring(), Context); Err) { Why = std::string(Err.getMessage()); return false; }
            return true;
        });
        if (!bRead) { if (pError) *pError = Why; return false; }
        Out = std::move(Fresh);
        return true;
    }

    inline bool Write(const std::filesystem::path& DescFolder, descriptor& D, std::string* pError = nullptr) noexcept
    {
        std::error_code Ec;
        std::filesystem::create_directories(DescFolder, Ec);
        std::string Why;
        const bool bWritten = xscript::module::Retry([&]                    // the pipeline looks at Descriptor.txt for a moment now and then: see Retry
        {
            xproperty::settings::context Context;
            if (auto Err = D.Serialize(false, DescriptorFile(DescFolder).wstring(), Context); Err) { Why = std::string(Err.getMessage()); return false; }
            return true;
        });
        if (!bWritten && pError) *pError = Why;
        return bWritten;
    }

    // Where the ScriptModule compiler leaves the module: its CMake file and its log, from the project root. The same names the resource manager uses
    // ("<Type>/<low byte>/<second byte>/<guid>"), written here because the compiler of the game has no resource manager to ask.
    inline std::string RelativeModulePath(const xscript::module::module_ref& Module, const char* pPrefix) noexcept
    {
        const std::uint64_t V = Module.m_Instance.m_Value;
        return std::format("{}/ScriptModule/{:02X}/{:02X}/{:X}", pPrefix, V & 0xFF, (V >> 8) & 0xFF, V);          // the guid as the pipeline names the folder: no leading zeros
    }
    inline std::string ModuleCMakeRelative(const xscript::module::module_ref& Module) noexcept { return RelativeModulePath(Module, "Cache/Resources/Platforms/WINDOWS"); }
    inline std::string ModuleLogRelative(const xscript::module::module_ref& Module)   noexcept { return RelativeModulePath(Module, "Cache/Resources/Logs") + ".log/Log.txt"; }
    // The game project of a Game resource: <project>/Cache/Script/<guid>/ (CMakeLists.txt, and the build the editor makes from it). Every Game has its own, so Games that share script modules
    // do not disturb each other's configure and build. And the folder of its DLL, in the compiled resources (not Game/<lo>/<hi>/<guid>, the stamp of the compiled resource: for a small guid the two collide).
    inline std::string ScriptFolderRelative(const game_ref& Game) noexcept  { return std::format("Cache/Script/{:X}", Game.m_Instance.m_Value); }
    inline std::string GameDllFolderRelative(const game_ref& Game) noexcept { return std::format("Cache/Resources/Platforms/WINDOWS/GameDll/{:X}", Game.m_Instance.m_Value); }

    // The folder of a Game resource (user descriptors, then the system ones): found from the project's path and the guid alone, no resource manager needed.
    inline std::filesystem::path FindGameFolder(const std::filesystem::path& Project, const game_ref& Game) noexcept
    {
        const std::uint64_t V = Game.m_Instance.m_Value;
        const std::string Rest = std::format("Game/{:02X}/{:02X}/{:X}.desc", V & 0xFF, (V >> 8) & 0xFF, V);
        std::error_code Ec;
        for (const char* pRoot : { "Descriptors", "Cache/Descriptors" })
            if (auto Folder = Project / pRoot / Rest; std::filesystem::is_directory(Folder, Ec)) return Folder;
        return {};
    }
    // The folder of the module's resource (user descriptors, then the system ones).
    inline std::filesystem::path FindModuleFolder(const std::filesystem::path& Project, const xscript::module::module_ref& Module) noexcept
    {
        const std::uint64_t V = Module.m_Instance.m_Value;
        const std::string Rest = std::format("ScriptModule/{:02X}/{:02X}/{:X}.desc", V & 0xFF, (V >> 8) & 0xFF, V);
        std::error_code Ec;
        for (const char* pRoot : { "Descriptors", "Cache/Descriptors" })
            if (auto Folder = Project / pRoot / Rest; std::filesystem::is_directory(Folder, Ec)) return Folder;
        return {};
    }
}

#endif // XGAME_DESCRIPTOR_H
