# Minimal desktop spell-checking

OmaWrite's spelling feature uses **Sonnet Core 6.13.0** with a single platform
backend. It keeps the Qt Quick editor, ordinary Markdown and one source undo
history. It does not embed Kate, a browser, Sonnet's editor widgets or a cloud
service. This is spelling assistance, not grammar checking or autocorrect.

## Controls

In **Edit**, right-click prose (or use **Shift+F10 / the Menu key**) and open
**Spelling → Check spelling**. Select an installed language in that same submenu.
These two preferences are global, not stored in Markdown. Read has no spelling
squiggles or correction menu.

Right-click a flagged word for suggestions, **Ignore for this session**, or
**Learn word**. Accepting a suggestion is one source edit: normal Undo/Redo works.
Ignore lasts for this controller/window, is language-specific and survives file
changes, but is not shared with another window or saved across restarts. Learn
explicitly changes the backend's personal dictionary. An image's existing
Name/folder/Locate menu keeps priority. Ordinary Cut/Copy/Paste remain available;
menu Paste uses the same persistent image/smart-link/plain-text path as the keyboard.

New squiggles appear after the current source snapshot finishes checking, rather
than streaming growing prefixes. Use the [spelling exercise](spellchecking-example.md)
for manual checks; its checklist is not itself a record of automated passes.

## Dictionaries

| Desktop | Backend | Dictionary availability |
| --- | --- | --- |
| macOS | Apple's NSSpellChecker | Languages exposed by the macOS spelling service |
| Windows | Windows ISpellChecker | Installed languages supported by Windows' spelling API |
| Linux | Hunspell | Installed system Hunspell dictionaries |

There is no single universal Linux dictionary/personal-word store shared by all
applications. On Linux, install the dictionaries for the languages you write in
using your distribution's package manager. On Windows/macOS, use the OS language
and spelling settings. OmaWrite does not download dictionaries automatically.

Native language identifiers differ: the tested Mac accepts generic `en`, while
`en_US` and `en-US` were invalid. Choose an identifier actually exposed by the
backend rather than assuming locale names are interchangeable.

The current controller first tries the system locale, then an installed
same-language dictionary; it does not silently substitute an unrelated language.
A saved selection that is no longer installed leaves checking unavailable until
an available dictionary is selected. Missing dictionaries must not prevent
writing or saving. No dictionary is bundled in the app.

## Editing model

- Spelling decoration belongs in **Edit**; Read remains uncluttered.
- Check readable prose, not Markdown destinations, image paths, URLs, code or
  Mermaid. Inactive rendered table/image/diagram objects must not receive
  spelling squiggles. Source text can be checked when those blocks are exposed.
- Only an explicitly accepted suggestion changes Markdown, as one undoable edit.
  Underlines, language selection and ignored words do not modify the note.
- Personal learned words belong to the selected backend's dictionary. They may
  be visible to other applications using that same spelling service. Session
  ignores are app-owned, since native backend capabilities differ.
- No automatic correction, automatic language guessing or document metadata is
  introduced. Select an available dictionary for the current writing language.

## Implementation boundaries

Check canonical source snapshots, never the projection's object-replacement
characters. Cache results outside the document and project only eligible text
ranges into the existing Markdown highlighter, merging underline properties
rather than replacing syntax/search styling. Results and menu replacements must
be invalidated when their source or language changes. Dictionary calls must not
run inside paint/highlight callbacks.

Native AppKit and COM APIs have thread-affinity constraints. Bounded queued
checking is preferable to indiscriminately dispatching OS calls to QtConcurrent
workers. Spelling results do not rebuild the rich-text projection or alter its
layout batching rules, particularly after the image render-thread hardening.
Pure source classification runs in a worker; native dictionary operations stay
on the GUI thread in queued batches of at most 64 words / 4 ms. One OS call
cannot be preempted. Results publish once per completed snapshot, avoiding
quadratic repeated prefix projection/restyling. Suggestions take priority over
background checks, and IME preedit defers checks/publication. The prose classifier
is conservative, not a full CommonMark parser: ordinary indented/container code
is excluded, but arbitrary raw HTML element bodies are not fully recognized.

## Build and licensing

`bin/build-dependencies` builds the pinned private Release Sonnet Core and one
static backend under `.deps/install`, alongside KSyntaxHighlighting. Sonnet's
Widgets and QML components are disabled. `dependencies/sonnet-static-plugins.cmake`
explicitly selects static plugins; `dependencies/spellplugins.cpp` imports the
chosen backend. No runtime Sonnet plugin search path or KDE desktop is needed.

Linux additionally needs `pkg-config` and the Hunspell development library at
build time, plus language dictionaries at runtime. See [installation](install.md).
An external private prefix can be selected with `OMAWRITE_SPELL_PREFIX`; it must
match the same Qt kit, compiler, architecture and Release configuration.

Sonnet is not MIT-licensed; see [upstream notices](../third_party/sonnet/README.md).
The notice includes the pinned source, static-plugin build modification and four
upstream license texts. Static LGPL obligations include the required ability to
relink with a modified library; copying notices alone is not compliance. The
existing syntax-resource redistribution audit remains blocking. This developer
build is not a redistribution-cleared release.

## Verification

The first macOS native backend smoke check enumerated NSSpellChecker languages,
recognized `dictionary`, rejected `dictionnaryy`, and suggested `dictionary`.
It did not learn or delete any words in the user's system dictionary.

The macOS app builds. The eleven-suite offscreen run passes **275 checks**, with
0 failures and 2 native-render bodies skipped. Separate native Mac runs cover the
OS dictionary smoke, actual spelling menu/suggestion replacement/undo, screenshot
paste through keyboard and menu, and 100 threaded rendered mode switches.
Injected dictionaries test Ignore/Learn success/failure, unavailable languages,
stale actions, composition and projection safety. **Native Learn was never called**;
no user's personal dictionary was changed. See [testing](testing.md) for scope.

Windows and Linux are source-supported but **not built or runtime-tested on those
platforms**; report failures with the Qt/compiler kit, dictionary language and
backend availability.
