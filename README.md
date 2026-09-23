A C++ / Qt library that checks GitHub releases for updates, then downloads and launches the update.
Automatic installation is only implemented on Windows, where the downloaded `.exe` is launched as the installer. On other platforms the update check and download link work, installation is left to the user. If you want to extend it to other platforms - be my guest.

# Usage

1. Construct `CAutoUpdaterGithub` with the repository name and the current version string:
  `CAutoUpdaterGithub updater{"VioletGiraffe/github-releases-autoupdater", "0.9.1"};`
  The optional third argument is a less-than comparator for version strings. The default is case-insensitive natural sorting.
2. Pass your `CAutoUpdaterGithub::UpdateStatusListener` implementation to `setUpdateStatusListener()`.
3. Call `checkForUpdates()`. `onUpdateAvailable()` is called asynchronously with every release newer than the current version, in GitHub's order (newest first). The changelog is empty when no update is available.
4. Call `downloadAndInstallUpdate()` with a changelog entry's `versionUpdateUrl`. The listener receives `onUpdateDownloadProgress()` during the download and `onUpdateDownloadFinished()` before the installer is launched.

`onUpdateError()` reports a failure of any step.

## Releases

* Draft releases are skipped. Pre-releases are included, marked by `isPrerelease`; the dialog labels them as such.
* A leading `v` or `.v` is removed from the tag name before comparing versions.
* `versionUpdateUrl` is the first release asset ending in the platform's extension (`.exe`, `.dmg`, `.AppImage`), or the release page if there is none.
* `versionChangesMarkdown` is the release description as written on GitHub, in Markdown. The dialog renders it with Qt's Markdown support.

## Ready-made dialog

`CUpdaterDialog` implements all of the above: `new CUpdaterDialog(parent, "owner/repo", currentVersion, silentCheck)`.
With `silentCheck`, the dialog stays hidden and shows no errors unless an update is found.
Add `CONFIG += updater_without_widgets` to leave the dialog out and drop the Qt Widgets dependency.

# Building

Prerequisites:
* Qt 6.
* A C++20 compiler.
* [cpp-template-utils](https://github.com/VioletGiraffe/cpp-template-utils) checked out next to this repository.

Build `github-releases-autoupdater.pro` as you would any Qt-based static library.
