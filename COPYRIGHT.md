# Write — copyright and third-party notices

This feature-rich fork is maintained by Alfonso Alvarez (ponchoalv). Original fork contributions are provided under the root [MIT license](LICENSE). The Write/Omawrite branding and application artwork follow upstream; this fork does not claim upstream endorsement.

The application is derived from [Omawrite](https://github.com/omacom/omawrite), copyright © 2026 David Heinemeier Hansson. Its MIT copyright, permission notice and disclaimer remain applicable and are retained in `LICENSE`. Rebranding and substantial modifications do not transfer the upstream author's copyright. Git history and historical benchmark reports remain factual records, not current branding.

## Separately licensed components

- **IBM Plex Mono**: copyright © 2017 IBM Corp. with Reserved Font Name "Plex". SIL OFL 1.1; see [fonts/OFL.txt](fonts/OFL.txt). Fonts have not been renamed or relicensed.
- **KSyntaxHighlighting 6.13.0**: MIT engine, but embedded definitions/themes have mixed additional terms. See [notices and unresolved resource audit](third_party/ksyntaxhighlighting/README.md).
- **Sonnet Core 6.13.0**: separately licensed KDE code, including LGPL obligations for static linking, source and relinking; see [notices](third_party/sonnet/README.md).
- Qt and platform libraries retain their own licenses. The application MIT license does not relicense dependencies, resources or bundled fonts.

**No prebuilt distribution is cleared by this rename.** The syntax-resource audit and static-library distribution obligations remain unresolved; the packaging guard remains enabled. Local development builds are not a declaration of distribution compliance.

## Compatibility is not branding

`src/profile.h` deliberately retains the previous application/settings/IPC identifiers. Existing preferences are imported without overwriting new values; existing recovery files and staged images remain together in their original store, with the same slot locks. The selected storage root is remembered. No recovery files are copied, deleted or renamed. The shared IPC endpoint prevents old and renamed versions from becoming independent primary editors. Quit old copies before launching the renamed build to avoid forwarding to an already-running previous version.
