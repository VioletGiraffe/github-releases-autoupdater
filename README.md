A C++ / Qt library for update checking and downloading the updates for the software distributed via GitHub releases.

# Usage

1. Create an instance of the updater class. Specify your GitHub repository address, and a string representation of the current version of the software (could be any format, just make sure it's consistent with your GitHub version tags):

  `_updater("https://github.com/VioletGiraffe/file-commander", "0.9.1")`
2. Specify the class that will receive update notification (via the `CAutoUpdaterGithub::UpdateStatusListener` interface):
  `_updater.setUpdateStatusListener(this);`
3. Call `checkForUpdates()`
4. The `onUpdateAvailable(CAutoUpdaterGithub::ChangeLog changelog)` callback will be called asynchronously (on the same thread that requested the check). If any updates were found, the `changelog` vector will be non-empty. You can use its items to retrieve the update details. If it's empty, no updates are available.
5. Call `downloadAndInstallUpdate()` to download the update and launch it.

# Building

Qt 5 is required, and a compiler with C++0x/11 support. Build the project as you would any Qt-based static library.
