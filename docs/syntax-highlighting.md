# Native language-fence syntax highlighting

Language-tagged **backtick and tilde fences are highlighted in both Edit and Read**. PonchoWriter uses standalone **KSyntaxHighlighting 6.13.0**, not Kate, KTextEditor or Qt WebEngine. This is token coloring inside the native Markdown writing surface, not autocomplete, an LSP client, code execution or a graphical code editor.

For an immediately usable test document, open [code-highlighting-example.md](code-highlighting-example.md).

## Write a tagged fence

Paste these ordinary Markdown examples into a disposable note:

````markdown
```python
# A comment, a keyword and a string
for name in ["PonchoWriter"]:
    print(name)
```

~~~cpp
/* Multiline
   comment */
const char *message = "Native Qt";
~~~

```json
{"enabled": true, "count": 3}
```
````

Use the first info-string token as the language. Resolution supports aliases, native definition names and safe extension lookup, for example:

| Language | Example tags |
| --- | --- |
| Python | `python`, `py` |
| C++ | `cpp`, `c++` |
| JavaScript | `js`, `javascript` |
| TypeScript | `ts` |
| JSON | `json` |
| Shell | `bash`, `sh` |
| YAML | `yaml` |

Unknown tags and untagged fences stay plain code; there is no content-based language guessing. Multiline strings/comments carry state within a fence, but **every separate fence resets the lexer**, including adjacent fences with the same language. For example, the second fence below must not inherit the unfinished comment:

````markdown
```cpp
/* unfinished comment
```
```cpp
int value = 42;
```
````

## Edit and Read behavior

- Edit keeps code editable and fence markers muted. Colors follow the light/dark theme without replacing the writing font, zoom or code background; existing search styling remains intact.
- Read bakes token character formats into its presentation document **before HTML serialization**, rather than relying on layout-only highlighting that would disappear on conversion. This feature does not change the separate Print/Copy as HTML export pipeline.
- Read conservatively compares source-fence metadata with Qt's Markdown import. If the parsers disagree about a block's metadata or boundaries, that code stays plain rather than receiving potentially wrong colors. Complex/nested Markdown can therefore differ from Edit.
- Highlighting and toggling views do not rewrite Markdown, alter source/view mappings or add undo operations. Existing source editing, search, font and zoom behavior are preserved.
- Mermaid's optional external renderer remains separate. Code highlighting needs **no Node, browser, network access or runtime downloads**.

## Build prerequisites and bootstrap

Install **CMake 3.24+ first**, a C++17 compiler/build tool and a matching **Qt 6.7+** desktop kit. The dependency needs Qt Core, Gui and Network; tests need Qt Test. The app also needs its existing Qt modules; see the complete [Windows/macOS/Linux installation guide](install.md).

On macOS, install the tools with `brew install qt git cmake` and select that Qt's tools on PATH. On Linux, install CMake and `make` with your compiler and matching Qt development packages; Ninja is optional. From the repository root on macOS/Linux:

```sh
# Select your intended Qt/compiler environment first.
cmake --version
JOBS=4 ./bin/build-dependencies
./bin/build
./bin/test
```

The explicit bootstrap is optional: `bin/build` and `bin/test` automatically call it **only if the private dependency header is missing**, not whenever the kit changes. It downloads official KDE archives during the developer build, builds a static Release library and embeds syntax/theme resources with `QRC_SYNTAX=ON` under the private, Git-ignored `.deps/install` prefix. There are no runtime grammar downloads.

On **Windows**, use the matching MinGW CMD environment or MSVC Developer Command Prompt to configure/build `dependencies` **before qmake or Qt Creator**, as shown in [§1.4/§1.5 of the installation guide](install.md#14-build-with-mingw-in-command-prompt). Select the same kit and **Release** for the app and all tests. Static dependency/qmake Release settings must match the Windows CRT; **Debug is currently unsupported**. The static library needs no extra KDE DLL, but Qt deployment is still required.

When changing Qt kit, compiler or architecture, delete `.deps` and generated app/test build directories before bootstrapping again. Never delete personal notes or attachments. An optional `PONCHOWRITER_SYNTAX_PREFIX` can point to a prebuilt **static** installation prefix, but it must match the exact kit/compiler/architecture.

### Pinned developer downloads

`dependencies/CMakeLists.txt` pins these official KDE sources and verifies SHA-256:

| Archive | SHA-256 |
| --- | --- |
| [Extra CMake Modules 6.13.0](https://download.kde.org/stable/frameworks/6.13/extra-cmake-modules-6.13.0.tar.xz) | `7006017c00c817ff4c056995146d271791d1487a398d39ea6cac1cd59a8bf402` |
| [Syntax Highlighting 6.13.0](https://download.kde.org/stable/frameworks/6.13/syntax-highlighting-6.13.0.tar.xz) | `d6f19bb886d6e1c7251d813497f14ecc9dac413253764eb61a8f3a836806450d` |

## Licenses and verification status

The **engine is MIT-licensed; embedded grammar/definition data is not all MIT**. It includes GPL/LGPL/other licenses. Consult `third_party/ksyntaxhighlighting/README.md` and its `LICENSES/` files for upstream notices. A successful local build is **not a redistribution-ready binary release**. Resource/license/source auditing remains required, and historical Arch packaging is disabled pending that work.

On 2026-11-07, **208 checks passed, 0 failed across eight suites**, including `tests/codehighlighting/codehighlighting.pro` / `tst_codehighlighting`, on macOS arm64 with Qt 6.12.0. The app was rebuilt with static embedded syntax resources and upstream notices. Tests cover actual Read token formats, multiline/per-fence state, incremental edits, both themes, font/search/source/undo preservation and plain fallback; see [testing details](testing.md). Windows/Linux instructions are source-inspected, **not built or runtime-tested there**.

To run just the new suite on macOS/Linux after bootstrapping, from the repository root (use `qmake6` if that is your kit's tool name):

```sh
mkdir build-tests-codehighlighting
cd build-tests-codehighlighting
qmake ../tests/codehighlighting/codehighlighting.pro CONFIG+=release
make -j4
QT_QPA_PLATFORM=offscreen ./tst_codehighlighting
```

On macOS, if built as a bundle, run `QT_QPA_PLATFORM=offscreen ./tst_codehighlighting.app/Contents/MacOS/tst_codehighlighting` instead. Stop on any failed command; this one suite is not the complete eight-suite run.

For manual checks, try both fence styles, aliases, unknown/untagged tags, multiline tokens, language-only edits, adjacent fences, Edit/Read, light/dark, search, font/zoom and undo/redo. Check that saved Markdown remains unchanged. Use [the installation guide's eight-suite commands](install.md#17-windows-automated-tests) for automated tests; offscreen tests are not deployment or GUI certification.

The [investigation](syntax-highlighting-investigation.md) is retained as historical design context, not a pending feature proposal.
