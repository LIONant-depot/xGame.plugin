// The compiler of the Game resource: the Game's list of script modules in, the CMake project of the game out.
//
// The project (<project>/Cache/Script/<Game guid>/CMakeLists.txt, one per Game resource) includes the CMake file that the ScriptModule compiler wrote for each module (the Game compiles after the modules: its plugin
// says RunAfter ScriptModule), so what a module is made of is decided in one place, its own compile. This one only puts them together.
//
// What this compile depends on is the descriptor of the Game and the log of each module's compile (dependencies.txt): the log is written when the module's compile ENDS, so the project
// is made after the module's CMake file is, never before it. The files of the modules are not inputs of it: a .h or a .cpp edit changes nothing here.
// CMakeLists.txt is only written when its text changes (it says a hash of each module's file), so its time says when the project has to be configured again.
#include "dependencies/xstrtool/source/xstrtool.h"
#include "plugins/xgame.plugin/source/Module/xgame_descriptor.h"
#include "plugins/xgame.plugin/source/Module/xgame_cmake.h"
#include "source/Compiler/xgame_compiler.h"

#include "dependencies/xproperty/source/xcore/my_properties.cpp"

namespace xgame_compiler
{
    struct implementation final : xgame_compiler::instance
    {
        xerr onCompile(void) override
        {
            const auto Start = std::filesystem::file_time_type::clock::now();           // before anything is read: see xscript::module::StampCompileOutput
            const std::filesystem::path Project    = m_ProjectPaths.m_Project;
            const std::filesystem::path DescFolder = Project / m_InputSrcDescriptorPath;

            //
            // Load and validate the descriptor
            //
            displayProgressBar("Reading the descriptor", 0.0f);
            xgame::descriptor Descriptor;
            {
                std::string Error;
                if (!xgame::Read(DescFolder, Descriptor, &Error))
                {
                    LogMessage(xresource_pipeline::msg_type::ERROR, std::format("The descriptor cannot be read: {}", Error));
                    return xerr::create_f<state, "Game: Descriptor.txt cannot be read">();
                }
                std::vector<std::string> Errors;
                Descriptor.Validate(Errors);
                if (!Errors.empty())
                {
                    for (const auto& E : Errors) LogMessage(xresource_pipeline::msg_type::ERROR, E);
                    return xerr::create_f<state, "Game failed validation">();
                }
            }

            //
            // The CMake file of every module has to be there, and made from the module's current descriptor
            //
            displayProgressBar("Reading the modules", 0.3f);
            std::vector<xgame::cmake::module_file> Files;
            bool bFailed = false;
            for (const auto& Module : Descriptor.m_Modules)
            {
                const std::string Guid     = std::format("{:016X}", Module.m_Instance.m_Value);
                const auto        Relative = xgame::ModuleCMakeRelative(Module);
                const auto        File     = Project / Relative;

                const auto ModuleFolder = xgame::FindModuleFolder(Project, Module);
                if (ModuleFolder.empty())
                {
                    LogMessage(xresource_pipeline::msg_type::ERROR, std::format("The script module {} is not in the project: remove it from the Game or add it back", Guid));
                    bFailed = true; continue;
                }
                std::string Text;
                if (!xscript::module::ReadAll(File, Text))
                {
                    LogMessage(xresource_pipeline::msg_type::ERROR, std::format("The script module {} has no CMake file ({}): its compile has not run or has failed, see its log", Guid, Relative));
                    bFailed = true; continue;
                }
                std::error_code Ec;
                const auto DescriptorTime = std::filesystem::last_write_time(xscript::module::DescriptorFile(ModuleFolder), Ec);
                if (!Ec && std::filesystem::last_write_time(File, Ec) < DescriptorTime)
                {
                    LogMessage(xresource_pipeline::msg_type::ERROR, std::format("The CMake file of the script module {} is older than its Descriptor.txt: its last compile failed, see its log", Guid));
                    bFailed = true; continue;
                }
                Files.push_back({ Guid, Relative, xgame::cmake::Hash(Text) });

                // the log of the module's compile is what this compile waits for: it is the last thing a compile of the module writes
                m_Dependencies.m_Resources.push_back(xresource::full_guid{ Module.m_Instance, Module.m_Type });
                m_Dependencies.m_Assets.push_back(xstrtool::To(xgame::ModuleLogRelative(Module)));
            }
            if (bFailed) return xerr::create_f<state, "Game: a script module is not ready">();

            //
            // The project: <project>/Cache/Script/<this Game>/CMakeLists.txt, written when it changed. Every Game has its own project (Games share the CMake files of the modules, and
            // nothing else), so the one the editor runs and the ones it does not are compiled and checked the same way.
            //
            displayProgressBar("Writing the project", 0.7f);
            const std::string Cmake = xgame::cmake::Assemble(Files, m_ResourceGuid.m_Value);
            {
                const auto CMakeLists = Project / xstrtool::To(xgame::ScriptFolderRelative(xgame::game_ref{ m_ResourceGuid })) / L"CMakeLists.txt";
                std::string Old;
                if (!(xscript::module::ReadAll(CMakeLists, Old) && Old == Cmake))
                {
                    std::error_code Ec;
                    std::filesystem::create_directories(CMakeLists.parent_path(), Ec);
                    if (!xscript::module::WriteAll(CMakeLists, Cmake))
                    {
                        LogMessage(xresource_pipeline::msg_type::ERROR, std::format("'{}' could not be written", CMakeLists.string()));
                        return xerr::create_f<state, "Game: the project could not be written">();
                    }
                }
            }

            //
            // The compiled resource of the Game: what it was made from (a stamp, the project is the real output)
            //
            for (auto& T : m_Target)
            {
                if (!T.m_bValid) continue;
                const std::filesystem::path File = T.m_DataPath;
                if (!xscript::module::WriteAll(File, std::format("Game project {:016x}\n", xgame::cmake::Hash(Cmake))))
                {
                    LogMessage(xresource_pipeline::msg_type::ERROR, std::format("'{}' could not be written", File.string()));
                    return xerr::create_f<state, "Game: the compiled resource could not be written">();
                }
                xscript::module::StampCompileOutput(File, Start);
            }
            displayProgressBar("Writing the project", 1.0f);
            return {};
        }
    };
}

std::unique_ptr<xgame_compiler::instance> xgame_compiler::instance::Create(void)
{
    return std::make_unique<implementation>();
}
