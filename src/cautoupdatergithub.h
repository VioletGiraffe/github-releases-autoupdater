#pragma once

#include <QNetworkAccessManager>
#include <QString>

#include <functional>
#include <vector>

class CAutoUpdaterGithub : public QObject
{
	Q_OBJECT

public:
	struct VersionEntry {
		QString versionString;
		QString versionChanges;
	};

	typedef std::vector<VersionEntry> ChangeLog;

	struct UpdateStatusListener {
		// If no updates are found, the changelog is empty
		virtual void onUpdateAvailable(ChangeLog changelog) = 0;

		// percentageDownloaded >= 100.0f means the download has finished
		virtual void onUpdateDownloadProgress(float percentageDownloaded) = 0;

		virtual void onUpdateErrorCallback(QString errorMessage) = 0;
	};

public:
	CAutoUpdaterGithub(const QString& githubRepositoryAddress, const QString& currentVersionString, const std::function<bool (const QString&, const QString&)>& versionStringComparatorLessThan);

	CAutoUpdaterGithub& operator=(const CAutoUpdaterGithub& other) = delete;

	void setUpdateStatusListener(UpdateStatusListener* listener);

	void checkForUpdates();
	void downloadAndInstallUpdate();

private slots:
	void updateCheckRequestFinished(QNetworkReply * reply);
	void updateDownloadFinished(QNetworkReply * reply);
	void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private:
	const QString _updatePageAddress;
	const QString _currentVersionString;
	const std::function<bool (const QString&, const QString&)> _lessThanVersionStringComparator;

	UpdateStatusListener* _listener = nullptr;

	QNetworkAccessManager _networkManager;

	QString _updateDownloadLink;
};

