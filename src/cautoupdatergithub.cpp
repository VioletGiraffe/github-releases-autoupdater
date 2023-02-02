#include "cautoupdatergithub.h"
#include "updateinstaller.hpp"

DISABLE_COMPILER_WARNINGS
#include <QCollator>
#include <QCoreApplication>
#include <QDir>
#include <QNetworkReply>
#include <QNetworkRequest>
RESTORE_COMPILER_WARNINGS

#include <assert.h>

static const auto naturalSortQstringComparator = [](const QString& l, const QString& r) {
	static QCollator collator;
	collator.setNumericMode(true);
	collator.setCaseSensitivity(Qt::CaseInsensitive);

	// Fix for the new breaking changes in QCollator in Qt 5.14 - null strings are no longer a valid input
	return collator.compare(qToStringViewIgnoringNull(l), qToStringViewIgnoringNull(r)) < 0;
};

CAutoUpdaterGithub::CAutoUpdaterGithub(const QString& githubRepositoryAddress, const QString& currentVersionString, const std::function<bool (const QString&, const QString&)>& versionStringComparatorLessThan) :
    _updatePageAddress(githubRepositoryAddress),
	_currentVersionString(currentVersionString),
	_lessThanVersionStringComparator(versionStringComparatorLessThan ? versionStringComparatorLessThan : naturalSortQstringComparator)
{
    assert(githubRepositoryAddress.contains("https://api.github.com/repos/"));
	assert(!currentVersionString.isEmpty());
}

void CAutoUpdaterGithub::setUpdateStatusListener(UpdateStatusListener* listener)
{
	_listener = listener;
}

void CAutoUpdaterGithub::checkForUpdates()
{
    QNetworkRequest request;
    request.setUrl(QUrl(_updatePageAddress));
    request.setRawHeader("Accept", "application/vnd.github+json");
    QNetworkReply * reply = _networkManager.get(request);
	if (!reply)
	{
		if (_listener)
			_listener->onUpdateError("Network request rejected.");
		return;
	}

	connect(reply, &QNetworkReply::finished, this, &CAutoUpdaterGithub::updateCheckRequestFinished, Qt::UniqueConnection);
}

void CAutoUpdaterGithub::downloadAndInstallUpdate(const QString& updateUrl)
{
	assert(!_downloadedBinaryFile.isOpen());

	_downloadedBinaryFile.setFileName(QDir::tempPath() + '/' + QCoreApplication::applicationName() + UPDATE_FILE_EXTENSION);
	if (!_downloadedBinaryFile.open(QFile::WriteOnly))
	{
		if (_listener)
			_listener->onUpdateError("Failed to open temporary file " + _downloadedBinaryFile.fileName());
		return;
	}

	QNetworkRequest request((QUrl(updateUrl)));
	request.setSslConfiguration(QSslConfiguration::defaultConfiguration()); // HTTPS
	request.setMaximumRedirectsAllowed(5);
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	QNetworkReply * reply = _networkManager.get(request);
	if (!reply)
	{
		if (_listener)
			_listener->onUpdateError("Network request rejected.");
		return;
	}

	connect(reply, &QNetworkReply::readyRead, this, &CAutoUpdaterGithub::onNewDataDownloaded);
	connect(reply, &QNetworkReply::downloadProgress, this, &CAutoUpdaterGithub::onDownloadProgress);
	connect(reply, &QNetworkReply::finished, this, &CAutoUpdaterGithub::updateDownloaded, Qt::UniqueConnection);
}

void CAutoUpdaterGithub::updateCheckRequestFinished()
{
	auto reply = qobject_cast<QNetworkReply *>(sender());
	if (!reply)
		return;

	reply->deleteLater();

	if (reply->error() != QNetworkReply::NoError)
	{
		if (_listener)
			_listener->onUpdateError(reply->errorString());

		return;
	}

	if (reply->bytesAvailable() <= 0)
	{
		if (_listener)
			_listener->onUpdateError("No data downloaded.");
		return;
	}

	ChangeLog changelog;
    const auto releases = QString(reply->readAll());
    const QJsonDocument jsonDocument = QJsonDocument::fromJson(releases.toUtf8());
    const QJsonObject json = jsonDocument.object();

	// Skipping the 0 item because anything before the first "release-header" is not a release
	for (int releaseIndex = 1, numItems = releases.size(); releaseIndex < numItems; ++releaseIndex)
	{
        QString updateVersion = json["tag_name"].toString();
        QString url = json["assets"][0]["browser_download_url"].toString();
        QString releaseUrl = json["html_url"].toString(); // Fallback incase there is no download link available
			
		if (updateVersion.startsWith(QStringLiteral(".v")))
			updateVersion.remove(0, 2);
		else if (updateVersion.startsWith('v'))
			updateVersion.remove(0, 1);

		if (!naturalSortQstringComparator(_currentVersionString, updateVersion))
			continue; // version <= _currentVersionString, skipping

        const QString updateChanges = json["body"].toString();

        changelog.push_back({ updateVersion, updateChanges, !url.isEmpty() ? url : releaseUrl });
	}

	if (_listener)
		_listener->onUpdateAvailable(changelog);
}

void CAutoUpdaterGithub::updateDownloaded()
{
	_downloadedBinaryFile.close();

	auto reply = qobject_cast<QNetworkReply *>(sender());
	if (!reply)
		return;

	reply->deleteLater();

	if (reply->error() != QNetworkReply::NoError)
	{
		if (_listener)
			_listener->onUpdateError(reply->errorString());

		return;
	}

	if (_listener)
		_listener->onUpdateDownloadFinished();

	if (!UpdateInstaller::install(_downloadedBinaryFile.fileName()) && _listener)
		_listener->onUpdateError("Failed to launch the downloaded update.");
}

void CAutoUpdaterGithub::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
	if (_listener)
		_listener->onUpdateDownloadProgress(bytesReceived < bytesTotal ? bytesReceived * 100 / (float)bytesTotal : 100.0f);
}

void CAutoUpdaterGithub::onNewDataDownloaded()
{
	auto reply = qobject_cast<QNetworkReply*>(sender());
	if (!reply)
		return;

	_downloadedBinaryFile.write(reply->readAll());
}
