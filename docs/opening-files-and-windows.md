# Opening files and windows

PonchoWriter uses one app process per user/session endpoint, with an independent document, undo history, recovery slot and QML engine for each window. The Qt local connection adds no browser, network service, polling loop or extra dependency.

- An already-open local file activates its existing window **without reloading it**.
- Another file opens in another window. Open from the file picker does not replace the originating draft or ask you to discard it.
- **Cmd+N / Ctrl+N** always creates a fresh untitled window.
- Launching the running app without filenames activates an existing window. Launching it when closed starts an untitled window (or restores orphaned drafts).
- Closing a modified window still requires Save, Discard or Cancel. Cancel retains the document and its ownership. The app exits after its last accepted close.

## Commands

### macOS

After quitting older copies and opening the rebuilt app, use native file opening:

```bash
open -a "/Users/alfonso.alvarez/github/vendor/ponchowriter/build/ponchowriter.app" "/absolute/path/to/note.md"
```

Several files can follow the application path. Finder Open With uses the same native file-open event handler; the app bundle declares Markdown extensions without taking over your default association automatically.

Prefer this to `--args`: macOS does not reliably deliver new command-line arguments to an already-running app. `-n` is no longer needed. Even a newly launched updated process forwards its requests to the primary; it does not deliberately create another editor for the same file.

The executable also supports repeatable command-line requests:

```bash
"/Users/alfonso.alvarez/github/vendor/ponchowriter/build/ponchowriter.app/Contents/MacOS/ponchowriter" "/absolute/path/to/note.md"
```

### Windows

From Command Prompt, using your deployed build:

```bat
"C:\Apps\PonchoWriter\ponchowriter.exe" "C:\Notes\note.md"
```

PowerShell uses `&` before a quoted executable path. Register Open With yourself if desired; this source build does not install a Windows file association.

### Linux

```bash
ponchowriter "/home/me/Notes/note.md"
```

Desktop file-association requests reach the same router. Existing desktop launchers can pass one file per launch; multiple launches still converge on one process.

## Identity and write protection

Routing compares canonical local paths, including symlink/relative aliases where resolvable, without changing the operational Markdown path used for relative attachments. Missing targets use a cleaned absolute path and canonical existing parent. This is not vault-wide tracking, inode watching or automatic repair after arbitrary external moves.

Save and Save As check ownership **before copying attachments or writing Markdown**. If another window owns the target, the save fails, that owner is activated, and the originating draft/URL/modified state remain intact. Choose another name. Saves are serialized on the GUI thread; no merging or collaborative editing is attempted.

Normal duplicate opens therefore do not create competing autosavers. This cannot protect against older versions without routing, another editor, another login/session, or external filesystem changes. Quit all older PonchoWriter processes before switching to this build. In particular, an older `/Applications/ponchowriter.app` is not updated by rebuilding `build/ponchowriter.app`.

## Recovery and failure behavior

At startup, orphaned recovery drafts get their own windows before requested files are routed. A requested file does not overwrite a recovered draft. If two recovered drafts name the same file, both texts and their original attachment base URLs are retained; only the first owner may save to that target while both are open. Recovered named drafts remain autosave-suspended until an explicit save. Save the other draft under a different name.

New Window never claims an orphan snapshot. Accepted closes stop save/recovery timers and release the window's recovery lock after QML teardown. Staged attachments are **not deleted** on close. Malformed recovery JSON is renamed with an `.invalid-…` suffix for manual inspection instead of silently deleted. Up to 100 live recovery slots are available; the app refuses an additional window if none can be claimed safely.

The broker uses a private per-user directory, user-only socket permissions, a lock-guarded endpoint, framed/validated local-path requests and bounded clients/messages/timeouts. Only a lock owner may remove a stale endpoint. An IPC failure does **not** authorize opening a competing editor: the secondary exits with an error. ACK means the primary queued the request, not that disk reading or window activation succeeded. A busy/unresponsive app can require retrying later.

Native macOS file events are batched. A forwarding process keeps its event loop alive for a short quiet interval so events delivered after initial startup are also forwarded; it does not become a background daemon.

## Verification limits

macOS is the available test host. The automated checks include subprocess framing, simultaneous startup, stale/unresponsive owners, private directory validation, slow queued opening, duplicate/alias routing, fresh-window recovery, duplicate recovered drafts, Save As protection, accepted-close cleanup and delayed native file events. Native window activation remains subject to desktop focus policies; Windows/Linux compilation and runtime behavior need testing on those systems.
