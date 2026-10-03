# The Game resource

The game's code is made of **script modules** (ScriptModule resources, see `xscript_module.plugin/documentation/editor.md`). The **Game** resource lists the modules a game is
built from, and the resource pipeline makes the CMake project of the game from it. The project has no Game of its own: every Level names the Game it runs under.

```
<resource>.desc/info.txt         what every resource has
<resource>.desc/Descriptor.txt   the descriptor: Modules - the script modules of the game, in the order they are built
```

## How the project is made

Everything goes through the resource pipeline, like every other compiled resource, so a cleared cache, a new checkout or a build without the editor all get the project from the same rule.

1. **A module is compiled** (`xscript_module_compiler`): `Descriptor.txt` in, the module's CMake file out (`Cache/Resources/Platforms/WINDOWS/ScriptModule/xx/yy/<guid>`). Its only input is the
   descriptor: editing a `.h` or a `.cpp` never compiles it.
2. **The Game is compiled** (`xgame_compiler`; the plugin says *RunAfter ScriptModule*): the project, `Cache/Script/CMakeLists.txt`, which includes the CMake file of each module and builds
   `Game.dll` (the game entry points of `xscript_module.plugin`, the engine's `LIONCore.lib`, the modules' sources, libraries and defines).
   - It depends on the log of each module's compile (`dependencies.txt` lists `Cache/Resources/Logs/ScriptModule/xx/yy/<guid>.log/Log.txt`), which a compile writes last, and on the
     Game's own descriptor. No source file is a dependency.
   - It refuses a module that is not in the project, one that has no CMake file, and one whose CMake file is older than its `Descriptor.txt` (its last compile failed); the Logs say which.
     When the module compiles again the pipeline compiles the Game again by itself.
   - `CMakeLists.txt` is written only when its text changes (it names each module's file and a hash of its text), so its time says when CMake has to be configured again. It names no place on the
     machine: the project is found from the file's own folder, and the two folders of the engine come on the command line of the configure
     (`cmake -S Cache/Script -B <build> -DXGPU_ROOT=<xLION checkout> -DXGPU_BIN_DIR=<xLION's build folder>`), which is what the editor does.
3. **The editor builds `Game.dll`** from it when it is stale (see the module documentation).

Every compile stamps its output with the time it *began*, so an edit made while it runs is compiled after it, never lost. The pipeline retries a compile that failed when one of its inputs changed
after that compile began.

## Using it

- Create a Game resource in the Asset Browser (type **Game**), add the modules in its editor (a list of module pickers), and give it to the Levels that run on it (the Level's `Game` in the Inspector, or `SetLevelGame`).
- A module added with `AddProjectModuleReference -Game <asset guid>` joins that Game.
- The modules have to be in the same project as the Game (the compiler finds them from the project's folder).

## Commands

| command | |
| --- | --- |
| `ListProjectModuleReferences -Game <asset guid>` | the Game (`Game: <asset guid>`) and its modules, one asset guid per line |
| `AddProjectModuleReference -Module <asset guid> -Game <asset guid>` | adds a module to the Game (undoable) |
| `RemoveProjectModuleReference -Module <asset guid> -Game <asset guid>` | removes it (undoable; refused when a scene that is open uses components that only that module brings) |
| `RegenerateProjectModuleSources [-Game <asset guid>]` | compiles the Game again, every Game of the project when none is given (recovery) |
| `CreateAsset -Type A3F1D6C0452E9B17 ...` | a new Game resource |
| the Game editor's `Compile`, `CompileStatus`, `SetProperty`, `Save`... | the generic descriptor editor commands |

## Building the compilers

`xscript_module_compiler.exe` and `xgame_compiler.exe` are built by each plugin's `build/CreateAndBuildProject.bat`, and by the root `CMakeLists.txt` of xLION when they are missing.

## Levels and Games

A Level names the Game it runs under (`Game` in its Descriptor.txt). A Level without a Game has no scripts, components or systems of any module: there is no fallback: the project has no Game of its own. The modules the Level's scenes need must all be listed by its Game, or the Level has an error: the toolbar and the Inspector show it in red, `OpenLevel` says ERROR, and Play and Step refuse (the Play
button is greyed) because the world would run without what its scenes use. A Game's levels are a query (`ListLevels -Game <asset guid>`), not a list the Game holds.
`SetLevelGame -Level <hex16> [-Game <asset guid>]` sets it (undoable, written at once; without -Game the Level names none) and refuses a Game (or none) that does not list a module the Level's scenes need, naming the module;
`GetLevelGame -Level <hex16>` says which Game a Level names. Every Level runs on its own engine copies and the Game.dll of the Game it names (see `documentation/Editors/engine_copies.md` of xLION), so Levels of different Games open and play at the same time;
the right-click menu of the Level in the Level tree has Game > the Games of the project. Clicking the Level row in the Level tree selects the Level (`SelectLevel`): the Inspector then shows the Level's own descriptor
in the property inspector - its Scenes (read-only: the tree adds and removes them) and its Game, a resource reference you pick or drag a Game onto (undone with the Level's own Ctrl+Z, via the session's `SetLevelGame`) -
and under it the modules the Game lists and what each scene needs from it; `DescribeLevel` says the same as text. Selecting an entity brings the entity's properties back. `AddProjectModuleReference`,
`RemoveProjectModuleReference` and `ListProjectModuleReferences` take `-Game <asset guid>` (required); the module editor's Overview says which Games list the module.

## Scenes and Games

A Game is compatible with a scene when it lists every script module the scene's components come from. See `xscript_module.plugin/documentation/editor.md` ("Scenes need modules") for `ListSceneModules`,
`CheckGameCompatibility` and `ListScenesUsingModule`; a Game does not store its levels or its scenes.
