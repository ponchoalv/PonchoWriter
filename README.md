# Write (Omawrite)

A feature-rich fork of [Omawrite](https://github.com/omacom/omawrite), following upstream's Write branding and development.

- Caret-aware Markdown Edit/Read modes, translated UI, font picker and search/replace.
- Editable virtualized tables, syntax-colored code fences, local images and clipboard screenshots.
- Independent document windows, recovery drafts, native spelling and Print/PDF snapshots.
- Optional Mermaid diagrams through an external CLI; no embedded browser.
- Windows, macOS and Linux support.

## Use and build

- [Features and shortcuts](docs/writing-and-shortcuts.md)
- [Dependencies, setup, architecture checks and installers](docs/install.md)
- [Tests](docs/testing.md)
- Examples: [preview](docs/preview-example.md), [code](docs/code-highlighting-example.md), [spelling](docs/spellchecking-example.md)

Windows x64, from CMD:

```bat
setup.bat -Install -Installer
build.bat --check --installer
build.bat --test --installer
```

macOS/Linux, with the [dependencies](docs/install.md#macos-and-linux) installed:

```sh
./bin/build
./bin/test
```

On macOS: `open -a "$PWD/build/cmake/Write.app" document.md`. On Linux: `./build/cmake/omawrite document.md`.

Quit older copies before upgrading. Existing fork settings, recovery storage and ownership locks are reused in place; internal legacy identifiers are intentionally retained for compatibility. No documents or drafts are copied or deleted by the rename.

Installer automation is unsigned and requires native Windows validation. MSVC targets need the VC++ Redistributable; Node/Mermaid remain external. Bundling libraries is not a fully static Qt build.

## License

The application retains its [MIT license](LICENSE). Bundled dependencies/resources have additional terms, including static LGPL relinking obligations and mixed-license syntax resources: see [copyright and notices](COPYRIGHT.md). Building or packaging does **not** establish redistribution clearance.
