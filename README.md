# PonchoWriter

A native Qt Markdown editor, forked from [Omawrite](https://github.com/omacom/omawrite) and maintained by Alfonso Alvarez.

- Caret-aware Markdown Edit and Read modes, search/replace and familiar shortcuts.
- Editable virtualized tables, syntax-colored code fences, local images and clipboard screenshots.
- Independent document windows, recovery drafts, native spelling and Print/PDF snapshots.
- Optional Mermaid diagrams through an external CLI; no embedded browser.

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

On macOS: `open -a "$PWD/build/ponchowriter.app" document.md`. On Linux: `./build/ponchowriter document.md`.

Installer automation is unsigned and requires native Windows validation. MSVC targets need the VC++ Redistributable; Node/Mermaid remain external. Bundling libraries is not a fully static Qt build.

## License

The application retains its [MIT license](LICENSE). Bundled dependencies/resources have additional terms, including static LGPL relinking obligations and mixed-license syntax resources: see [copyright and notices](COPYRIGHT.md). Building or packaging does **not** establish redistribution clearance.
