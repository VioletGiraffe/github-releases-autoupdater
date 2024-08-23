#pragma once

#include "../cpp-template-utils/compiler/compiler_warnings_control.h"

DISABLE_COMPILER_WARNINGS
#include <QFile>
#include <QNetworkAccessManager>
#include <QString>
RESTORE_COMPILER_WARNINGS

#include <functional>
#include <vector>

#if defined _WIN32
#define UPDATE_FILE_EXTENSION QLatin1String(".exe")
#elif defined __APPLE__
#define UPDATE_FILE_EXTENSION QLatin1String(".dmg")
#else
#define UPDATE_FILE_EXTENSION QLatin1String(".AppImage")
#endif

class CAutoUpdaterGithub final : public QObject
{
public:
	using QObject::QObject;

	struct VersionEntry {
		QString versionString;
		QString versionChanges;
		QString date;
		QString versionUpdateUrl;
	};

	using ChangeLog = std::vector<VersionEntry>;

	struct UpdateStatusListener {
		virtual ~UpdateStatusListener() = default;
		// If no updates are found, the changelog is empty
		virtual void onUpdateAvailable(const ChangeLog& changelog) = 0;
		virtual void onUpdateDownloadProgress(float percentageDownloaded) = 0;
		virtual void onUpdateDownloadFinished() = 0;
		virtual void onUpdateError(const QString& errorMessage) = 0;
	};

public:
	// If the string comparison functior is not supplied, case-insensitive natural sorting is used (using QCollator)
	CAutoUpdaterGithub(QString githubRepositoryName, // Name of the repo, e. g. VioletGiraffe/github-releases-autoupdater
					   QString currentVersionString,
					   const std::function<bool (const QString&, const QString&)>& versionStringComparatorLessThan = {});

	CAutoUpdaterGithub& operator=(const CAutoUpdaterGithub& other) = delete;

	void setUpdateStatusListener(UpdateStatusListener* listener);

	void checkForUpdates();
	void downloadAndInstallUpdate(const QString& updateUrl);

private:
	void updateCheckRequestFinished();
	void updateDownloaded();
	void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
	void onNewDataDownloaded();

private:
	QFile _downloadedBinaryFile;
	const QString _repoName;
	const QString _currentVersionString;
	const std::function<bool (const QString&, const QString&)> _lessThanVersionStringComparator;

	UpdateStatusListener* _listener = nullptr;

	QNetworkAccessManager _networkManager;
};

