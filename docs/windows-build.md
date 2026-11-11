# Native Windows build and Mermaid checklist

The root **`build.bat`** builds pinned KDE dependencies, the Release app, optionally two test suites, and optionally a runnable Qt folder. Requires **Windows 10/11**, **Qt 6.7+**, and a matching desktop compiler/Windows SDK. The example below uses **Qt 6.8.3 MSVC 2022 x64** as a reproducible kit, not a claim that it is the newest Qt.

**Status:** Windows-specific code/tests and this script have not been executed on Windows by the author of this change. Local macOS arm64 / Qt 6.12.0 checks: app build succeeded; main suite **136 passed, 0 failed, 3 skipped**; Mermaid suite **11 passed, 0 failed**. The Windows-only helpers are not included in those totals. Follow the checks below and retain the first failing command/output. macOS regression tests do not establish Windows support by themselves.

## 1. Install prerequisites with winget

Run these in PowerShell or CMD. Accept elevation when requested. Do not mix the two shells' continuation syntax; commands are deliberately single-line.

```text
winget install --id Git.Git --exact --source winget --accept-package-agreements --accept-source-agreements
winget install --id Kitware.CMake --exact --source winget --accept-package-agreements --accept-source-agreements
winget install --id Ninja-build.Ninja --exact --source winget --accept-package-agreements --accept-source-agreements
winget install --id StrawberryPerl.StrawberryPerl --exact --source winget --accept-package-agreements --accept-source-agreements
winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --source winget --accept-package-agreements --accept-source-agreements --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
winget install --id Microsoft.VCRedist.2015+.x64 --exact --source winget --accept-package-agreements --accept-source-agreements
```

In Visual Studio Installer, confirm **Desktop development with C++**, **MSVC v143 x64/x86 tools**, and a current **Windows 10/11 SDK** are installed. The SDK supplies `spellcheck.h`; installing the redistributable alone is not a compiler setup. CMake must be **3.24+**. If already installed, use `winget upgrade --id <ID> --exact --source winget` as appropriate.

**Perl is mandatory**, even with `BUILD_TESTING=OFF`: KSyntaxHighlighting generates PHP, Twig and Doxygen syntax definitions with Perl. `build.bat` verifies native Windows Perl and forwards its exact path to CMake. Strawberry also ships GCC: do not accidentally compile Qt dependencies with that GCC.

Close the terminal and open a **new x64 Native Tools Command Prompt for VS 2022** after installing. If `cmake`, `ninja` or `perl` are missing, verify their installation/PATH and reopen the prompt; do not reuse a terminal whose environment predates installation. Typical Perl location is `C:\Strawberry\perl\bin`.

## 2. Install a real Qt desktop kit

Use either the [official Qt installer](https://www.qt.io/download-qt-installer-oss) or aqt below. There is no dependency on Qt Designer as a separate winget package. Select Qt **6.8.3 → MSVC 2022 64-bit**, including Qt Base, Qt Declarative/Quick, Quick Controls 2, Quick Dialogs 2 and Qt Test (normally part of the desktop archive). **Qt WebEngine is not needed.** Observe the Qt license you selected.

Alternatively install Python and use aqt (network access and several GB required):

```text
winget install --id Python.Python.3.12 --exact --source winget --accept-package-agreements --accept-source-agreements
```

Reopen CMD, then:

```bat
py -3.12 -m pip install --upgrade aqtinstall
py -3.12 -m aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 --outputdir C:\Qt
```

If choosing another Qt release/architecture, use aqt's available-archive listing or the official installer rather than guessing kit names. Do not combine an MSVC Qt archive with MinGW or mix x86/x64/ARM64 libraries.

## 3. Build from CMD

All commands below use **CMD syntax**, not PowerShell. From PowerShell, start `cmd.exe` first. Clone your desired branch/commit, or use your existing checkout:

```bat
git clone https://github.com/ponchoalv/PonchoWriter.git
cd PonchoWriter
set "QT_DIR=C:\Qt\6.8.3\msvc2022_64"
set "JOBS=8"
where cl
where cmake
where ninja
where perl
"%QT_DIR%\bin\qmake.exe" -query QT_VERSION
perl -v
build.bat --test --deploy
echo %ERRORLEVEL%
```

Zero means success. The script can initialize installed VS 2022 tools automatically for x64 when `cl.exe` is absent, but the prepared Developer Prompt is preferred. It does not install anything or change your permanent PATH. `JOBS` controls CMake/MinGW parallelism; `nmake` remains serial.

- Dependency downloads (ECM, KSyntaxHighlighting, Sonnet **6.13.0**) are SHA256-pinned in `dependencies/CMakeLists.txt` and require network access on the first build.
- Dependencies: `.deps\windows-win32-msvc-qt6.8.3-x64\install`.
- Build: `build\windows-win32-msvc-qt6.8.3-x64`; EXE normally in its `release` subdirectory.
- Runnable deployment: `build\windows-win32-msvc-qt6.8.3-x64\dist\ponchowriter.exe`.
- `build.bat` without options just builds. `--test` runs Mermaid and main editor suites. `--deploy` uses the **same kit's** `windeployqt --release --compiler-runtime --qmldir src`, including notices. It refuses to overwrite an existing `dist`; move/remove only that generated folder to redeploy.
- Qt/kit/architecture-specific build directories avoid reusing macOS/Linux caches. Changing compiler installation/version triggers a toolchain-stamp error: move/remove the reported **generated** directory and rebuild. Do not remove your source checkout or documents.
- Sonnet uses Windows `ISpellChecker`: no Hunspell install needed. Install a supported Windows spelling language for interactive spellchecking tests.

### MinGW alternative

In the official Qt installer select a **MinGW desktop kit and its matching Qt-supplied MinGW tools**. Do not use Strawberry's compiler or an arbitrary winget GCC. For example, after verifying the actual installed paths:

```bat
set "QT_DIR=C:\Qt\6.8.3\mingw_64"
set "MINGW_DIR=C:\Qt\Tools\mingw1310_64"
build.bat --test --deploy
```

Those paths are examples, not a guarantee of your installer's current compiler version. `MINGW_DIR` excludes `\bin`; the script prepends it before Strawberry's tools. Dependencies use Ninja; the qmake app uses the same kit's `mingw32-make`. Use a fresh CMD rather than carrying an unrelated MSVC environment into MinGW.

## 4. Optional Mermaid dependencies

These are **runtime tools**, not prerequisites for compiling or for the synthetic Mermaid tests. Install Node LTS, then reopen CMD before running npm:

```text
winget install --id OpenJS.NodeJS.LTS --exact --source winget --accept-package-agreements --accept-source-agreements
```

```bat
node --version
npm --version
npm install --global @mermaid-js/mermaid-cli
where node
where mmdc
mmdc --version
```

The CLI uses Puppeteer/Chrome; its installation may download a supported browser. Follow your proxy/security policy and the CLI's browser setup instructions if download/launch fails. Do not disable Chromium's sandbox or system security to make it work. npm/Node/browser downloads are external dependencies, not shipped in `dist`.

Standard global npm installs place `mmdc.cmd` under `%APPDATA%\npm`. Ensure **both** that directory and the directory containing `node.exe` are in your **user PATH**. Reopen PonchoWriter after changes; a launcher inherits its environment and may require signing out/in. Running `where` in a custom developer terminal alone does not prove Explorer has the same PATH.

Windows discovery understands native `mmdc.exe`, or the standard npm `mmdc.cmd` + `node_modules\@mermaid-js\mermaid-cli\package.json` / `src\cli.js` layout. For npm it invokes **`node.exe` directly with the JS entry point**—never `cmd /c`, PowerShell, `npx`, or batch-file contents. Unknown wrapper layouts, missing Node/CLI/browser, old SDKs without Job List support, or failed process containment leave Mermaid as source. Executables on PATH are trusted local software; the syntax filter is not an untrusted-code sandbox.

## 5. Manual Windows acceptance checks

1. Run `build.bat --test`. Mermaid tests use a copied native helper executable (fake Node/Chromium), including paths with spaces/Unicode/shell metacharacters. They verify npm discovery, missing tools, unchanged parent PATH, normal completion, crashed-parent cleanup, timeout and destruction killing child/grandchild processes, plus failed-start queue draining. They do **not** validate a real browser. POSIX-only fixtures skip on Windows.
2. Launch the deployed EXE normally with `QT_QPA_PLATFORM` unset. Empty document: caret is visibly left of `# Start writing`; typing/deleting, undo/redo, changing text scale and IME composition must keep the hint/caret correct without inserting placeholder text into the document.
3. Open `docs/preview-example.md` and verify its Mermaid blocks with a working CLI. Test a deliberately invalid Mermaid block and no-Node/no-CLI environments in a temporary CMD; source must remain editable. Do not rename system-installed tools or permanently overwrite PATH.
4. Close the app while a render is running and inspect Task Manager for orphaned **app-owned** Node/Chrome processes. Do not kill unrelated browser/Node work. Renderer jobs must clean descendants on success, crash, timeout and app destruction; a refused nested Job Object must fail back to source rather than run uncontained.
5. Exercise Open/Save/Print, clipboard, spelling and normal editing. Offscreen/synthetic tests cannot establish native dialog/IME/browser behavior.
6. Copy the **entire dist folder**, not only the EXE, to a clean Windows machine without Qt/build tools and launch it. MSVC needs the official supported VC++ Redistributable on the target (winget command above). For a weaker local check, open a fresh CMD, `cd` to `dist`, temporarily `set "PATH=%SystemRoot%\System32;%SystemRoot%"`, then run `ponchowriter.exe`; Mermaid is expected to fall back because its tools are intentionally absent. Close that CMD afterward.

For failures report Windows version, Qt kit/compiler, first failing command and exit code, complete build/test error output, and whether launch was from Explorer or a terminal. Do not describe this platform as validated until these checks actually run.

## Distribution caveat

`--deploy` creates a testable runtime folder, **not** a signed installer or a legal release clearance. Keep Qt plugins/QML imports/compiler runtime files together. Follow [install.md](install.md)'s LGPL/static KDE, bundled syntax/font and relinking/source obligations; review the upstream Qt/KDE/third-party notices as well as the copied repository notices before redistribution. `windeployqt` does not install Node, npm, Chrome, or Windows spelling languages.
