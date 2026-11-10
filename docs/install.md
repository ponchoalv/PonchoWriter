# Install PonchoWriter on Windows, macOS and Linux

## Trying the table-grid feature

Latest table/code scrolling work is on **`feature/table-grid-and-horizontal-overflow`**, based on `master` at `23f309d`. Save local changes before switching; from an existing checkout:

```sh
git fetch origin
git switch feature/table-grid-and-horizontal-overflow
# If only the remote branch exists, use:
# git switch --track origin/feature/table-grid-and-horizontal-overflow
```

Then build/test using the platform recipe below. Those stable recipes still clone/pull **`master`**; substitute **`feature/table-grid-and-horizontal-overflow`** in those Git commands to install this feature instead. Do not reset/clean local work. This branch adds native editable table grids, raw Markdown fallback and independent unwrapped code/table scrolling. **356 checks passed**, plus native Mac UI/render checks, and the 0.7.2 app rebuilt. Windows/Linux are still untested. [Behavior, safety and limits](tables-and-horizontal-overflow.md).

## Stable platform recipes

This guide builds the **[PonchoWriter project](https://github.com/ponchoalv/PonchoWriter)**, branch **`master`**, not an upstream binary or an Obsidian plugin. There is no prebuilt installer/release promised here. Clone this repository's default `master` branch explicitly; do not substitute a similarly named package or another repository.

## Current development branch

`master` follows incremental editing at `d35b347` (itself based on `master` at `f94d69e`); the old performance/cache experiment remains parked and unmerged. Current additions retain Edit/Read native documents, fix the distant-scroll caret, pair brackets/quotes and add Cmd/Ctrl+T / Shift+T code commands. See [mode behavior, measurement and limits](mode-transitions-and-code-shortcuts.md): **352** offscreen checks passed, separate native macOS UI/render/10,000-line checks passed, app rebuilt. Windows/Linux remain uncompiled/runtime-untested. The commands below select this branch; save local work before switching an existing checkout.

The current name is **PonchoWriter**, maintained by Alfonso Alvarez. Use `ponchowriter.pro`, `build/ponchowriter.app` and `PONCHOWRITER_*` configuration variables. Existing preferences/draft stores and cross-version locks are preserved; quit old copies before launching the renamed build. [Identity, copyright and upgrade details](ponchowriter-identity.md).

## Status and requirements

- **macOS:** development/testing host is Apple Silicon with Homebrew **Qt 6.12.0**. This guide does not claim every GUI/deployment scenario has been tested.
- **Windows and Linux:** commands are source-inspected instructions, **not built or runtime-tested on those platforms**. Validate on the target machine before relying on the app for important notes.
- Install **CMake 3.24+ before the first build** for the syntax-highlighting and spelling bootstrap, plus a matching compiler/build tool.
- Requires **Qt 6.7 or newer**, a matching desktop C++17 compiler, and Qt Core, GUI, **Network**, QML/Quick, Quick Controls, Quick Dialogs, Widgets, PrintSupport and Concurrent. Tests require Qt Test. These normally come from Qt Base and Qt Declarative plus their development tools/runtime modules.
- **QtDBus and XDG portal integration are Linux-only** in the portability changes accompanying this guide. Windows/macOS do not need QtDBus. If your checkout still unconditionally requests `dbus` or includes `QDBus` on those systems, obtain the updated feature branch before building; do not work around it by installing Linux integration on Windows/macOS.
- **No Qt WebEngine dependency.** Mermaid rendering is optional, external and Unix-only; see below.
- Git, adequate free build space and an internet connection for installing dependencies are needed. Build as your normal user, not administrator/root.

Keep personal notes outside the source/build tree. Back up existing `.md` files **and their `img/` folders** before trying a development fork. Standard Markdown and relative local image files remain the storage format.

## Syntax dependency (all platforms)

Language-tagged backtick/tilde fences now have native token colors in Edit and Read; see [syntax highlighting](syntax-highlighting.md) for examples and boundaries. This uses standalone **KSyntaxHighlighting 6.13.0**, not Kate/KTextEditor or WebEngine. No Node or runtime downloads are needed for code highlighting.

The first developer build downloads pinned official KDE archives, builds **static Release** KSyntaxHighlighting with embedded definitions/themes (`QRC_SYNTAX=ON`) and **Sonnet Core 6.13.0** with one system spelling backend, and installs privately under `.deps/install`. Sonnet's Widgets/QML components are disabled; macOS uses NSSpellChecker, Windows ISpellChecker, Linux system Hunspell. No spelling dictionaries are bundled or downloaded. See [spell-checking](spellchecking.md). `bin/build` and `bin/test` call `bin/build-dependencies` **when either private dependency header is missing**; they do not detect stale kits. On macOS/Linux, after selecting the matching Qt/compiler environment, you can bootstrap explicitly from the repository root:

```sh
cmake --version
JOBS=4 ./bin/build-dependencies
```

CMake defaults to Unix Makefiles on these Unix instructions; Ninja is optional. `.deps` is ignored by Git. When changing Qt kit, compiler or architecture, **delete `.deps` and the generated app/test build directories**, then rebuild; never delete personal notes or attachments. Alternatively, set `PONCHOWRITER_SYNTAX_PREFIX` and/or `PONCHOWRITER_SPELL_PREFIX` to existing static installation prefixes matching exactly the same Qt kit/compiler/architecture. The dependency and qmake app/tests must all use **Release**, to match the Windows CRT; **Debug is currently unsupported**.

The engine is MIT-licensed, but embedded definition data has mixed GPL/LGPL/other licenses: **not everything bundled is MIT**. See `third_party/ksyntaxhighlighting/README.md` and its `LICENSES/` files for upstream notices. A local build is **not a redistribution-ready release**; resource/license/source auditing remains required. Sonnet adds LGPL obligations, including static-library relinking requirements; see `third_party/sonnet/README.md`. Historical Arch packaging is disabled pending that audit. Deployment commands below describe local deployment mechanics, not permission or readiness to redistribute binaries.

Current print-layout checkpoint (2026-11-10): **381 passed, zero failures, four normal-run skips across thirteen suites**; eleven native Mac UI checks and six threaded-render checks/100 mode switches passed. App rebuilt as 0.7.2. The print feature is on **`feature/print-layout-and-table-appendices`**, based on `master` after the table PR merge; select that branch rather than default `master` to try it. No new dependency. See [print/PDF usage and limitations](printing.md): QtPDF was independently verified; no native printer engine was available, so physical/native drivers and Windows/Linux remain unverified. Existing `/Applications` copies were not replaced.

Historical verification on macOS arm64 / Qt 6.12.0 on 2026-11-08: **321 checks passed, 0 failed across twelve suites**, with 2 native-render bodies skipped offscreen. Separate native Mac runs passed OS-dictionary smoke, spelling suggestions/correction/undo, keyboard/menu image paste and 100 threaded rendered mode switches. The app rebuilt with static syntax/Sonnet resources and upstream notices. No native Learn or personal-dictionary changes were performed. See [test details](testing.md). Windows/Linux remain unverified.

## 1. Windows (first-time installation)

### 1.1 Install Git and one matching Qt/compiler kit

1. Install [Git for Windows](https://git-scm.com/downloads/win) and **CMake 3.24+**, available on PATH (`cmake --version`).
2. Download the official [Qt Online Installer](https://doc.qt.io/qt-6/qt-online-installation.html). Follow its account/license prompts and choose a Qt **6.7+** version that supports your Windows version and architecture. Consult [Qt's Windows support matrix](https://doc.qt.io/qt-6/windows.html).
3. Recommended: select that release's **desktop MinGW kit**, its Qt libraries (including Qt Declarative/Quick Controls/Quick Dialogs), Qt Creator, and the **matching MinGW compiler supplied for that kit** under development tools. Do not combine an arbitrary newer MinGW with older prebuilt Qt libraries. Embedded, Android and WebAssembly kits are not substitutes.
4. Record the actual kit and compiler directories. The examples use placeholders:

   ```text
   C:\Qt\<Qt-version>\<MinGW-kit>\bin
   C:\Qt\Tools\<matching-MinGW-toolchain>\bin
   ```

   Replace every `<...>` with your installation's real name. They intentionally do not prescribe a particular latest Qt or compiler version.

**Alternative:** choose an MSVC desktop Qt kit and install its supported Visual Studio C++ build tools and Windows SDK. Use the matching architecture's Developer Command Prompt and `nmake`, not MinGW. See §1.5.

### 1.2 Clone the fork

The Windows command blocks below are **Command Prompt (`cmd.exe`)**, not PowerShell, Git Bash or WSL. Open Command Prompt and choose a writable parent directory, for example:

```bat
mkdir "%USERPROFILE%\source"
cd /d "%USERPROFILE%\source"
git clone --branch master https://github.com/ponchoalv/PonchoWriter.git ponchowriter
cd ponchowriter
git branch --show-current
git rev-parse HEAD
```

The branch should be `master`. Save the commit hash for bug reports. If you already have a checkout, do not clone over it or discard local work; see Updating.

### 1.3 Build with Qt Creator (recommended GUI route)

**First complete the dependency configure/build in §1.4 (MinGW) or §1.5 (MSVC), from the repository root using the exact kit you will select in Qt Creator.** Stop after that dependency step if you want the GUI route. Opening/running qmake before the dependency exists will fail.

1. Open Qt Creator and use **File → Open File or Project** to open `ponchowriter.pro` from this checkout.
2. In project configuration, select only your matching **Qt 6.7+ desktop kit**. If unavailable, inspect **Preferences/Options → Kits**: Qt version, compiler, debugger and architecture must agree.
3. Choose **Release** and a fresh shadow-build directory, for example `build-win-mingw-qt6` outside the source files. Do not reuse a directory generated with another compiler/Qt kit.
4. Run qmake if prompted, then **Build Project**. Use Qt Creator's Run action for the first launch; its kit environment supplies Qt paths.
5. Locate the resulting `ponchowriter.exe` in the build directory. This fork forces Release and disables qmake's dual Debug/Release build; inspect both the build root and `release\ponchowriter.exe` rather than assuming a path.
6. For a standalone folder, follow §1.6 using the same kit's command prompt/tools and the actual build directory.

### 1.4 Build with MinGW in Command Prompt

Replace the placeholders first. This changes PATH only for the current terminal:

```bat
set "PATH=C:\Qt\<Qt-version>\<MinGW-kit>\bin;C:\Qt\Tools\<matching-MinGW-toolchain>\bin;%PATH%"
where qmake
qmake -v
where mingw32-make
where g++
g++ --version
```

Stop if qmake reports Qt older than 6.7 or the paths point to another kit. From the repository root, **build the dependency before qmake or Qt Creator**. Replace these example paths with your actual installed Qt and matching compiler versions (they are not interchangeable):

```bat
cmake -S dependencies -B .deps -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.x.y/mingw_64" -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw...._64/bin/g++.exe"
cmake --build .deps --config Release --parallel 4
```

Stop on failure. Then either use Qt Creator (§1.3), or build the app:

```bat
mkdir build-win-mingw
cd build-win-mingw
qmake ..\ponchowriter.pro CONFIG+=release
mingw32-make -j4
```

Inspect and launch the file that exists:

```bat
dir release\ponchowriter.exe
dir ponchowriter.exe
```

```bat
release\ponchowriter.exe
```

Or, if output is at the build root:

```bat
ponchowriter.exe
```

This fork's static-dependency configuration removes `debug_and_release` and forces Release; the executable may therefore be at the build root rather than under `release\`. Check both locations and use the actual generated path in the launch/deployment/test commands below. Do not continue with a stale executable after a failed build.

Open a document using a quoted path:

```bat
release\ponchowriter.exe "C:\Users\yourname\Documents\Notes\example.md"
```

Use `ponchowriter.exe` instead if that is the actual output. `bin/build`, `bin/test` and `bin/install` are **Unix shell scripts**, not native Windows build/test commands. Git Bash does not make their Unix assumptions into supported Windows instructions.

### 1.5 Optional MSVC command-line alternative

Use a fresh **matching x64/ARM64/etc. Visual Studio Developer Command Prompt**, with the architecture supported by your Qt kit. Add only the matching MSVC Qt kit to PATH (not MinGW). From the source root:

```bat
set "PATH=C:\Qt\<Qt-version>\<MSVC-kit>\bin;%PATH%"
where cl
where qmake
qmake -v
cmake -S dependencies -B .deps -G "NMake Makefiles" -DCMAKE_CXX_COMPILER=cl -DCMAKE_PREFIX_PATH="C:/Qt/<Qt-version>/msvc2022_64"
cmake --build .deps --config Release --parallel 4
```

The dependency step must succeed **before qmake/Qt Creator**. The example uses an **x64 Developer Command Prompt** and `msvc2022_64`; substitute your actual matching installed kit and supported compiler. Remove an old `.deps` when switching from MinGW or another kit. Then use Qt Creator (§1.3), or continue:

```bat
mkdir build-win-msvc
cd build-win-msvc
qmake ..\ponchowriter.pro CONFIG+=release
nmake
```

Check both `release\ponchowriter.exe` and `ponchowriter.exe`, as above. Never reuse a MinGW build directory for MSVC or mix their libraries/plugins.

### 1.6 Deploy a runnable Windows folder

Run from your **actual build directory**, with the same Qt kit and compiler environment used to build. Use a new/empty `dist` directory; old DLLs can hide mistakes.

```bat
mkdir dist
copy release\ponchowriter.exe dist\ponchowriter.exe
windeployqt --release --compiler-runtime --qmldir ..\src dist\ponchowriter.exe
```

If the EXE is at the build root, replace the copy command with `copy ponchowriter.exe dist\ponchowriter.exe`. If Qt Creator built elsewhere, replace `..\src` with the absolute source directory, e.g. `--qmldir "C:\Users\yourname\source\ponchowriter\src"`. The important pattern is:

```text
windeployqt --release --compiler-runtime --qmldir <repository>/src dist/ponchowriter.exe
```

- KSyntaxHighlighting and Sonnet Core plus the native Windows spelling backend are statically linked; there is no extra KDE DLL/plugin to deploy. Windows must provide a supported installed spelling language; an OS display-language pack alone is not a guarantee. This does not remove the license/resource/source and LGPL relinking requirements above.
- `--qmldir` scans QML imports; copying only the EXE or only a few Qt DLLs is insufficient.
- Keep **the entire `dist` folder**, including `platforms`, QML imports, styles and compiler runtime dependencies. Zip/share the folder, not the EXE alone. This is a folder deployment, not a fork installer.
- `windeployqt` must come from the same Qt kit. Qt's kit command prompt or its `qtenv2.bat` can establish the Qt environment; compiler tools must also be available.
- MinGW needs its matching compiler runtime DLLs. For MSVC distribution, use Microsoft's official supported Visual C++ Redistributable/installer as described by Qt; **do not manually redistribute developer-machine MSVC DLLs**. Verify the runtime on the target machine even when using `--compiler-runtime`.
- Test the complete folder on a clean machine without Qt installed, ideally also without build tools. At minimum, launch from a fresh Command Prompt with no Qt/compiler paths. One temporary test, after changing to `dist`, is:

  ```bat
  set "PATH=%SystemRoot%\System32;%SystemRoot%"
  ponchowriter.exe
  ```

  Close that terminal afterward. This is not as strong as a clean-machine test.

Native Windows Open/Save/Print dialogs are **expected through Qt**, but still need interactive verification. SmartScreen, antivirus or organization policy may flag unsigned builds. Verify the source and provenance, use only a per-app approval permitted by your policy, or ask your administrator. Do **not** disable security globally or bypass corporate controls.

### 1.7 Windows automated tests

Use the same configured CMD/compiler environment and the already-built Release dependency from §1.4/§1.5. Build each project below in its **own fresh directory**, then run its target. Example from the repository root:

```bat
set "REPO=%CD%"
mkdir build-tests-win-core
cd build-tests-win-core
qmake "%REPO%\tests\tests.pro" CONFIG+=release
mingw32-make -j4
set "QT_QPA_PLATFORM=offscreen"
release\tst_ponchowriter.exe
set "QT_QPA_PLATFORM="
cd /d "%REPO%"
```

If the executable is at the build root, use `tst_ponchowriter.exe` instead. For MSVC use `nmake` instead of `mingw32-make -j4`. Repeat the same pattern with a distinct directory for **every** row:

| Project relative to repository | Executable target (append `.exe`) |
| --- | --- |
| `tests/tests.pro` | `tst_ponchowriter` |
| `tests/shortcutlabels.pro` | `tst_shortcutlabels` |
| `tests/headings/headings.pro` | `tst_headings` |
| `tests/attachments/attachments.pro` | `tst_attachments` |
| `tests/previewblocks/previewblocks.pro` | `tst_previewblocks` |
| `tests/livepreview/livepreview.pro` | `tst_livepreview` |
| `tests/mermaid/mermaid.pro` | `tst_mermaid` |
| `tests/codehighlighting/codehighlighting.pro` | `tst_codehighlighting` |
| `tests/spellchecking/spellchecking.pro` | `tst_spellchecking` |
| `tests/spellchecking/integration.pro` | `tst_spellingintegration` |
| `tests/spellchecking/main.pro` | `tst_spelling_main` |
| `tests/instancebroker/instancebroker.pro` | `tst_instancebroker` |
| `tests/printing/printing.pro` | `tst_printing` |

The three spelling suites cover classification/controller, projection and Main.qml integration; the broker suite covers local IPC and subprocess forwarding, and the print suite covers native snapshot/PDF layout without sending a physical print job. Build each project in another fresh directory with the same pattern. For the eighth (code-highlighting) suite specifically, from the repository root in the same configured CMD environment:

```bat
set "REPO=%CD%"
mkdir build-tests-win-codehighlighting
cd build-tests-win-codehighlighting
qmake "%REPO%\tests\codehighlighting\codehighlighting.pro" CONFIG+=release
mingw32-make -j4
set "QT_QPA_PLATFORM=offscreen"
tst_codehighlighting.exe
set "QT_QPA_PLATFORM="
cd /d "%REPO%"
```

Use `nmake` for MSVC and `release\tst_codehighlighting.exe` if that is the actual generated path.

Check each build and test exit status (`echo %ERRORLEVEL%` immediately afterward in CMD; zero means success). Unix-only renderer tests can skip on Windows. An offscreen pass does **not** validate native dialogs, clipboard integration or Windows deployment. Clear `QT_QPA_PLATFORM` before launching the GUI normally.

### 1.8 If using native PowerShell instead

First complete the matching dependency bootstrap in CMD (§1.4/§1.5), or translate those CMake commands too, before this qmake example. Use the same kit and Release configuration.

Do not paste CMD's `set`, `%USERPROFILE%`, `copy` conventions or `cd /d` into PowerShell. Qt Creator's terminal may also default to PowerShell. Either open `cmd.exe` for all steps above or translate deliberately:

```powershell
$env:Path = 'C:\Qt\<Qt-version>\<MinGW-kit>\bin;C:\Qt\Tools\<matching-MinGW-toolchain>\bin;' + $env:Path
Set-Location "$env:USERPROFILE\source\ponchowriter"
Get-Command qmake
qmake -v
New-Item -ItemType Directory -Path build-win-mingw-ps
Set-Location build-win-mingw-ps
qmake ..\ponchowriter.pro 'CONFIG+=release'
mingw32-make -j4
.\release\ponchowriter.exe
```

Again inspect the actual output; use `.\ponchowriter.exe` if it is at the root. PowerShell requires `&` to invoke a quoted executable path, e.g. `& 'C:\Qt\<Qt-version>\<MinGW-kit>\bin\qmake.exe' -v`. For tests use `$env:QT_QPA_PLATFORM = 'offscreen'`, run `.\release\tst_ponchowriter.exe`, then `Remove-Item Env:QT_QPA_PLATFORM`. `$LASTEXITCODE` reports the last native tool's exit status. Use `Copy-Item .\release\ponchowriter.exe .\dist\ponchowriter.exe` for deployment after creating `dist`; the `windeployqt` arguments remain the same.

## 2. macOS

### 2.1 Install the tools

1. Install Apple's command-line tools and finish any installer/license prompts:

   ```sh
   xcode-select --install
   ```

   If already installed, verify with `xcode-select -p` and `clang++ --version`.
2. Install [Homebrew](https://brew.sh/) using its official instructions if needed. Then:

   ```sh
   brew install qt git cmake
   export PATH="$(brew --prefix qt)/bin:$PATH"
   command -v qmake6 || command -v qmake
   qmake -v
   ```

   Confirm Qt **6.7+**. `brew --prefix qt` works for both Apple Silicon and Intel Homebrew locations. Keep compiler, Qt and build architecture consistent; do not mix Rosetta/x86_64 and arm64 artifacts. Export this PATH in each new terminal or add it to your own shell startup file if desired.

### 2.2 Clone, build, test and run

Choose a writable parent directory:

```sh
mkdir -p ~/source
cd ~/source
git clone --branch master https://github.com/ponchoalv/PonchoWriter.git ponchowriter
cd ponchowriter
git branch --show-current
git rev-parse HEAD
./bin/build
./bin/test
open build/ponchowriter.app
```

Stop and resolve any build/test failure before continuing. The scripts prefer `qmake6` over `qmake`: check both if you have multiple installations. They build into `build/` and `build-tests/`; do not reuse those directories across incompatible kits.

Launch directly to inherit the shell environment, open a file or capture diagnostic output:

```sh
./build/ponchowriter.app/Contents/MacOS/ponchowriter
./build/ponchowriter.app/Contents/MacOS/ponchowriter "/absolute/path/to/notes.md"
./build/ponchowriter.app/Contents/MacOS/ponchowriter --version
```

The `.app` includes the original PonchoWriter journal/P icon in multiple resolutions. The committed `assets/ponchowriter.icns` needs no icon-conversion tools at build time. `bin/build` refreshes generated bundle metadata, including the icon, even in an existing build directory. Quit/reopen the app if macOS still shows an older cached icon.

Native macOS dialogs are expected. **Do not install Linux portals or QtDBus for macOS.** Personal development builds do not require a Developer ID/notarization workflow.

### 2.3 Optional self-contained app bundle

From the source root, using the same Qt installation:

```sh
macdeployqt build/ponchowriter.app -qmldir=src
open build/ponchowriter.app
```

The `-qmldir=src` option deploys QML imports; Qt frameworks and plugins are also needed. Keep the entire `.app`. Verify on a Mac without Homebrew Qt and test the relevant CPU architecture. For sharing beyond your own development machine, Developer ID signing, hardened runtime/notarization and distribution policy are separate steps; modern `macdeployqt` uses ad-hoc signing by default, which is not a notarized release. Do not disable Gatekeeper globally to run a development build.

## 3. Linux

These are source-based instructions, **not a Linux build/runtime test report**.

### 3.1 Recommended: official Qt desktop kit

1. Use a distribution supported by the chosen [Qt release](https://doc.qt.io/qt-6/supported-platforms.html) and install Git, **CMake 3.24+**, a C++17 compiler and `make` with its package manager (Ninja is optional).
2. Install **pkg-config and Hunspell development files**, plus the dictionaries you need, using your distribution (examples below). These are required even with an official Qt kit; check `pkg-config --modversion hunspell` before bootstrapping. Then use the [Qt Online Installer](https://doc.qt.io/qt-6/qt-online-installation.html) to install a **Qt 6.7+ Linux desktop kit**, including Base, Declarative/Quick Controls/Quick Dialogs and development tools/Qt Test. Linux additionally needs QtDBus. Follow that kit's documented system-library requirements.
3. Add the actual kit's `bin` directory to PATH (replace placeholders):

   ```sh
   export PATH="$HOME/Qt/<Qt-version>/<desktop-kit>/bin:$PATH"
   command -v qmake6 || command -v qmake
   ```

   Run the selected `qmake6 -v` or `qmake -v`. Check both if installed: the scripts prefer `qmake6`, so an older system `qmake6` can override a newer `qmake` on PATH. Ensure the selected tools all belong to the intended kit.
4. For desktop-native dialogs/settings, install **`xdg-desktop-portal` plus the provider appropriate to your desktop**, such as KDE, GNOME, GTK or wlroots, following your distribution's setup. A running desktop session and correctly selected provider matter; installing only the portal service is not a guarantee. These dependencies apply to Linux, not Windows/macOS.

### 3.2 Distribution package alternative (verify Qt version first)

Package names and available Qt versions vary. **Ubuntu 24.04's Qt 6.4 is too old** for this fork. Installing its default development packages does not meet Qt 6.7+. Prefer the official kit rather than mixing repositories or upgrading system Qt blindly.

Examples to look for on a distribution whose repositories already provide Qt 6.7+:

- Debian/Ubuntu: `build-essential`, `git`, `cmake`, `pkg-config`, `libhunspell-dev`, dictionaries such as `hunspell-en-us` / `hunspell-es`, `qmake6`, `qt6-base-dev`, `qt6-declarative-dev`, `qt6-tools-dev-tools`. QML runtime packages commonly include `qml6-module-qtquick`, `qml6-module-qtquick-controls`, `qml6-module-qtquick-dialogs`, `qml6-module-qtquick-layouts`, `qml6-module-qtquick-window`, `qml6-module-qtquick-templates` and `qml6-module-qtqml-workerscript`. Check available names/dependencies for your release.
- Fedora: `gcc-c++`, `make`, `git`, `cmake`, `pkgconf-pkg-config`, `hunspell-devel`, dictionaries such as `hunspell-en-US` / `hunspell-es`, `qt6-qtbase-devel`, `qt6-qtdeclarative-devel`, `qt6-qttools-devel`; verify the release's Qt version and runtime QML module packaging.
- Arch Linux (rolling packages; verify current Qt version):

  ```sh
  sudo pacman -S --needed base-devel git cmake pkgconf hunspell hunspell-en_us hunspell-es_es qt6-base qt6-declarative qt6-tools
  qmake6 -v
  ```

Add your desktop's portal/provider packages separately. Qt Quick runtime imports are necessary even if compilation succeeds. Do not run `sudo make install`; running from the build tree is sufficient. `bin/install` is the historical Arch `makepkg` helper, not a general installer; packaging is disabled pending the embedded-resource/license/source audit. Use local builds instead.

### 3.3 Build, test and launch

```sh
mkdir -p ~/source
cd ~/source
git clone --branch master https://github.com/ponchoalv/PonchoWriter.git ponchowriter
cd ponchowriter
git branch --show-current
git rev-parse HEAD
./bin/build
./bin/test
./build/ponchowriter
./build/ponchowriter "/absolute/path/to/notes.md"
```

Stop on failure rather than launching an older executable. Run in your normal graphical desktop session. The test script uses `QT_QPA_PLATFORM=offscreen` for its processes only; that does not test Wayland/X11 integration or portal dialogs. If the Qt platform plugin fails, install the kit's documented distro runtime dependencies rather than copying arbitrary plugins from another Qt version.

### 3.4 Manual kit-specific builds (macOS or Linux)

The Unix helper scripts use fixed build-directory names. For a new kit, preserve old outputs if desired and use fresh directories with direct commands. From the repository root, with the intended kit's qmake (use `qmake6` instead if that is its name):

First remove `.deps` and generated build/test directories if changing kit/compiler/architecture, select the matching environment, then bootstrap before qmake:

```sh
JOBS=4 ./bin/build-dependencies
mkdir build-new-kit
cd build-new-kit
qmake ../ponchowriter.pro CONFIG+=release
make -j4
cd ..
mkdir build-tests-new-kit
cd build-tests-new-kit
qmake ../tests/tests.pro CONFIG+=release
make -j4
QT_QPA_PLATFORM=offscreen ./tst_ponchowriter
```

On macOS the test may instead be `./tst_ponchowriter.app/Contents/MacOS/tst_ponchowriter`; use the path actually generated. Repeat all thirteen projects/targets in the Windows test table in separate directories, using absolute project paths if convenient. This manual core test alone is **not** the complete suite. The app output is `build-new-kit/ponchowriter` on Linux or `build-new-kit/ponchowriter.app` on macOS.

## Opening files after installation

The current build routes file requests to one running process: an already-open canonical file is focused without reloading its draft, while a different file gets its own window. **New window** still creates an untitled window. Run the executable with multiple file paths, or use the OS's Open With association. The macOS bundle declares Markdown document support but does not forcibly replace your preferred editor.

Quit older copies first; an earlier app in `/Applications` cannot participate in this new routing protocol. macOS `open -a … --args …` is only a launch-time argument mechanism; use `open -a "/path/to/ponchowriter.app" "/path/to/note.md"` for a native document-open request or call the bundle executable directly. See [Opening files and windows](opening-files-and-windows.md), including `open -n`, recovery and desktop focus limits. Windows/Linux behavior is source-supported but has not been run here.

## 4. Optional Mermaid rendering and feature boundaries

PonchoWriter is a native Markdown editor with Edit/Read views, not a browser, whiteboard or graphical diagram designer. Tables/images/diagrams reveal their Markdown source when edited; **there is no drag-node diagram editor or arbitrary graphical Mermaid round-trip editing**.

- **Windows:** Mermaid deliberately remains **source fallback**, even if Node and `mmdc` are installed. Rendering is disabled until safe process-tree cancellation is implemented. This is not solved by changing PATH.
- **macOS/Linux (Unix):** rendering can use an independently installed Mermaid CLI (`mmdc`) on the application's PATH. That optional setup needs its own supported Node version and Puppeteer/Chromium/browser dependencies. Consult the [Mermaid CLI project](https://github.com/mermaid-js/mermaid-cli) before installing; Node alone is not the renderer. External tooling installation can have its own browser-download behavior.
- PonchoWriter **does not automatically install or download** Node, Mermaid CLI or a browser. No embedded WebEngine does not mean that an external renderer needs no browser runtime.
- Check `command -v mmdc` in the launching terminal. On macOS, start the app via its executable to inherit this PATH; Finder launches can have a different environment. Restart after changing renderer availability.
- Missing CLI, invalid/unsupported syntax, rejected configuration or render failure leave usable source. Document-provided configuration, click actions/HTML and remote-resource syntax are constrained. Remote images are not automatically fetched.

Native [language-fence syntax highlighting](syntax-highlighting.md) is implemented separately in Edit and Read, using the bundled library with no Node/browser requirement. It does not provide autocomplete, LSP or a graphical code editor.

### macOS: install the optional renderer

With a supported Node/npm installed (for example `brew install node` if missing):

```sh
npm install -g @mermaid-js/mermaid-cli
mmdc --version
command -v mmdc
```

The official npm package uses Puppeteer/Chrome and may download a browser during installation. Do not use the old unsupported Homebrew `mermaid-cli` formula. Quit PonchoWriter, then, from your source checkout, launch the executable directly so it inherits this terminal's PATH:

```sh
./build/ponchowriter.app/Contents/MacOS/ponchowriter "$PWD/docs/preview-example.md"
```

Move the caret outside the Mermaid block or switch to Read. A version check alone does not verify browser availability or successful rendering; if the CLI fails, inspect its terminal error rather than assuming the app can render it.

Mermaid is optional: do the first installation and smoke checks without it.

## 5. First-run smoke checks (all platforms)

Use a disposable document in a temporary **notes folder**, not your only copy of real work. Passing automated tests does not replace these checks:

1. Open the app, then its F1 shortcut reference. Confirm text and controls are readable at your display scale. Toggle OS light/dark appearance and check the app.
2. Open, edit, Save and Save As a Markdown file, including a folder/name with spaces and Unicode. Reopen it and verify its contents outside PonchoWriter. Named files auto-save after a pause; untitled files need an explicit save.
3. Verify native Open/Save and Print (cancel printing if desired). Windows/macOS native Qt dialogs are expected; Linux depends on the desktop/portal integration. Record unexpected fallback behavior rather than assuming an offscreen test covers it.
4. Type headings, emphasis, a table and an image reference. Toggle Edit/Read using the book/pencil button. Click supported blocks to reveal source; confirm toggling does not rewrite the Markdown.
   Also try tagged backtick and tilde code fences (`python`/`py`, `cpp`/`c++`, `js`, `json`, `bash`, `yaml`), unknown/untagged fences, multiline tokens and adjacent fences. Check Edit/Read colors, light/dark, font/zoom, search and unchanged source/undo; see the [examples](syntax-highlighting.md).
5. Copy a screenshot and paste: Windows **Win+Shift+S**, then **Ctrl+V**; macOS **Cmd+Ctrl+Shift+4**, then **Cmd+V**; Linux use your desktop's screenshot-to-clipboard tool, then **Ctrl+V**. Save and check that the image exists under `img/` beside the document. Undo/redo, reopen, and try Save As into a different folder.
6. Check Find/Replace: **Ctrl+F / Ctrl+H** on Windows/Linux, **Cmd+F / Cmd+Option+F** on macOS. Fullscreen: **F11** on Windows; **Cmd+Ctrl+F** on macOS (F11 is also an alternative and may need Fn); Linux F11 or desktop-permitted Super+F. Do not assume Windows-key/Meta combinations escape OS interception.
7. Exercise unsaved close/cancel, external file-change warnings and recovery on disposable data. Do not induce a crash against valuable notes. Preserve recovery drafts and app data until you have saved recovered content safely.
8. Windows: repeat with deployed `dist` on a clean machine. macOS: check the deployed `.app` without Homebrew Qt. Linux: test in the intended Wayland/X11 desktop session. Treat all as validation work still to perform, not established cross-platform support.

## 6. Updating, backups and identifying the build

1. Close PonchoWriter. Back up notes and their complete `img/` folders. A source-control update is not a backup of your writing or recovery data.
2. In the source checkout, inspect changes and confirm the branch:

   ```sh
   git status
   git branch --show-current
   git pull --ff-only origin master
   git rev-parse HEAD
   ```

   These Git commands work in CMD/PowerShell too. Run the pull only while on `master`. If the tree is dirty or the pull refuses a fast-forward, stop and resolve it deliberately; do not use reset/clean to discard work.
3. Rebuild and rerun all suites with the intended kit, then repeat smoke checks. For Qt/compiler/architecture changes, **delete `.deps` and generated build/test directories**, then bootstrap with the matching environment before qmake. Never delete notes, `img/`, persistent app data or recovery files as a build-cleanup step. Recreate deployment folders from the new build; do not merge old/new DLLs or plugins.
4. `--version` may still report **0.7.2**. Identify this fork by repository URL, branch and **`git rev-parse HEAD`**, not that version string alone. Record Qt/compiler/OS/architecture and test results with bug reports.

## 7. Common failures

| Symptom | Check/action |
| --- | --- |
| Qt version requirement error | Confirm the selected qmake reports 6.7+; check `qmake6` too on Unix. Use a fresh build directory. |
| Unknown Qt module / missing QML import | Install the corresponding Base/Declarative/development/runtime component from the same kit. Do not mix DLLs/imports from unrelated Qt versions. |
| Windows/macOS asks for QtDBus | Checkout predates the accompanying Linux-only portability changes. Update the feature branch; QtDBus is not a Windows/macOS prerequisite. |
| `mingw32-make`, `g++`, `cl` or `nmake` not found | Use the correct matching kit environment. MSVC needs a Developer Command Prompt; MinGW needs its compiler bin on PATH. |
| CMake missing / dependency header missing | Install CMake 3.24+ first; run the dependency bootstrap before qmake, with matching Qt Core/Gui/Network and Qt Test for tests. |
| Link errors or incompatible architecture/CRT | Match Qt kit, compiler and CPU architecture; delete `.deps` and generated build/test trees, then rebuild everything in Release (Debug unsupported). |
| Windows EXE/DLL/platform plugin missing | Check both executable output paths; rerun matching `windeployqt` with `--qmldir` and keep the complete folder. Use `QT_DEBUG_PLUGINS=1` temporarily for diagnostics. |
| macOS app works from terminal but optional Mermaid fails from Finder | App PATH differs; use the direct executable or configure your launch environment. Not a reason to install WebEngine. |
| Linux dialog/theme integration fails | Check portal service, correct desktop provider/session and matching Qt runtime. Windows/macOS do not use that portal setup. |
| Tests pass but no GUI appears | Remove an accidentally persistent `QT_QPA_PLATFORM=offscreen`; tests intentionally use it. |
| Diagram remains source | Expected on Windows; on Unix check optional CLI/PATH and supported syntax. Markdown source remains usable. |

## References

- [Qt Online Installer](https://doc.qt.io/qt-6/qt-online-installation.html)
- [Qt for Windows: supported configurations](https://doc.qt.io/qt-6/windows.html)
- [Qt Windows deployment](https://doc.qt.io/qt-6/windows-deployment.html) — kit environment, `windeployqt`, QML scanning and compiler runtime guidance.
- [Qt macOS deployment](https://doc.qt.io/qt-6/macos-deployment.html) — `macdeployqt`, `-qmldir` and signing options.
- [Qt supported platforms](https://doc.qt.io/qt-6/supported-platforms.html)
- [Native language-fence syntax highlighting](syntax-highlighting.md)
- [Writing, shortcuts, screenshots and feature boundaries](writing-and-shortcuts.md)
- [Preview examples](preview-example.md) and [test documentation](testing.md)
