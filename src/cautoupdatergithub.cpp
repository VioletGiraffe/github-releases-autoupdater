#include "cautoupdatergithub.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QProcess>

#include <assert.h>
#include <utility>

CAutoUpdaterGithub::CAutoUpdaterGithub(const QString& githubRepositoryAddress, const QString& currentVersionString, const std::function<bool (const QString&, const QString&)>& versionStringComparatorLessThan) :
	_updatePageAddress(githubRepositoryAddress + "/releases/"),
	_currentVersionString(currentVersionString),
	_lessThanVersionStringComparator(versionStringComparatorLessThan)
{
	assert(githubRepositoryAddress.contains("https://github.com/"));
	assert(!currentVersionString.isEmpty());
	assert(versionStringComparatorLessThan);
}

void CAutoUpdaterGithub::setUpdateStatusListener(UpdateStatusListener* listener)
{
	_listener = listener;
}

void CAutoUpdaterGithub::checkForUpdates()
{
	QNetworkReply * reply = _networkManager.get(QNetworkRequest(QUrl(_updatePageAddress)));
	if (!reply)
	{
		if (_listener)
			_listener->onUpdateErrorCallback("Network request rejected.");
		return;
	}

	connect(&_networkManager, &QNetworkAccessManager::finished, this, &CAutoUpdaterGithub::updateCheckRequestFinished, Qt::UniqueConnection);
}

void CAutoUpdaterGithub::downloadAndInstallUpdate()
{
	QNetworkReply * reply = _networkManager.get(QNetworkRequest(QUrl(_updateDownloadLink)));
	if (!reply)
	{
		if (_listener)
			_listener->onUpdateErrorCallback("Network request rejected.");
		return;
	}

	connect(reply, &QNetworkReply::downloadProgress, this, &CAutoUpdaterGithub::onDownloadProgress);
	connect(&_networkManager, &QNetworkAccessManager::finished, this, &CAutoUpdaterGithub::updateDownloadFinished, Qt::UniqueConnection);
}

inline std::pair<QString /*result*/, int /*end pos*/> match(const QString& pattern, const QString& text, int from)
{
	const auto delimiters = pattern.split('*');
	if (delimiters.size() != 2)
	{
		assert(!"Invalid pattern");
		return std::make_pair(QString(), -1);
	}

	const int leftDelimiterStart = text.indexOf(delimiters[0], from);
	if (leftDelimiterStart < 0)
		return std::make_pair(QString(), -1);

	const int rightDelimiterStart = text.indexOf(delimiters[1], leftDelimiterStart + delimiters[0].length());
	if (rightDelimiterStart < 0)
		return std::make_pair(QString(), -1);

	const int resultLength = rightDelimiterStart - leftDelimiterStart - delimiters[0].length();
	if (resultLength <= 0)
		return std::make_pair(QString(), -1);

	return std::make_pair(text.mid(leftDelimiterStart + delimiters[0].length(), resultLength), rightDelimiterStart + delimiters[1].length());
}

void CAutoUpdaterGithub::updateCheckRequestFinished(QNetworkReply * reply)
{
	if (!reply)
		return;

	reply->deleteLater();

	if (reply->error() != QNetworkReply::NoError)
	{
		if (_listener)
			_listener->onUpdateErrorCallback(reply->errorString());

		return;
	}

	if (reply->bytesAvailable() <= 0)
	{
		if (_listener)
			_listener->onUpdateErrorCallback("No data downloaded.");
		return;
	}

	const QString text = reply->readAll();
	ChangeLog changelog;
	static const QString changelogPattern = "<div class=\"markdown-body\">\n*</div>";
	static const QString versionPattern = "/releases/tag/*\">";
	static const QString downloadLinkPattern = "<ul class=\"release-downloads\">\n          <li>\n            <a href=\"*\"";
	int pos = 0;
	_updateDownloadLink.clear();
	QFile f("H:\\1.htm");
	f.open(QFile::WriteOnly);
	f.write(text.toUtf8());
	while (pos < text.length())
	{
		// Version first
		const auto versionMatch = match(versionPattern, text, pos);
		if (!_lessThanVersionStringComparator(_currentVersionString, versionMatch.first))
			break;

		const auto changeLogMatch = match(changelogPattern, text, versionMatch.second);
		if (_updateDownloadLink.isEmpty())
			_updateDownloadLink = match(downloadLinkPattern, text, changeLogMatch.second).first.prepend("https://github.com");

		pos = changeLogMatch.second;
		changelog.push_back({versionMatch.first, changeLogMatch.first.trimmed()});
	}

	if (_listener)
		_listener->onUpdateAvailable(changelog);
}

void CAutoUpdaterGithub::updateDownloadFinished(QNetworkReply * reply)
{
	if (!reply)
		return;

	reply->deleteLater();

	if (reply->error() != QNetworkReply::NoError)
	{
		if (_listener)
			_listener->onUpdateErrorCallback(reply->errorString());

		return;
	}

	const QUrl redirectUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
	if (!redirectUrl.isEmpty())
	{
		// We are being redirected
		QNetworkReply * reply = _networkManager.get(QNetworkRequest(redirectUrl));
		if (!reply)
		{
			if (_listener)
				_listener->onUpdateErrorCallback("Network request rejected.");
			return;
		}

		connect(reply, &QNetworkReply::downloadProgress, this, &CAutoUpdaterGithub::onDownloadProgress);
		connect(&_networkManager, &QNetworkAccessManager::finished, this, &CAutoUpdaterGithub::updateDownloadFinished, Qt::UniqueConnection);

		return;
	}


// 	if (reply->bytesAvailable() <= 0)
// 	{
// 		if (_listener)
// 			_listener->onUpdateErrorCallback("No data downloaded.");
// 		return;
// 	}

	QFile tempExeFile(QDir::tempPath() + '/' + QCoreApplication::applicationName() + ".exe");
	if (!tempExeFile.open(QFile::WriteOnly))
	{
		if (_listener)
			_listener->onUpdateErrorCallback("Failed to open temporary file.");
		return;
	}
	tempExeFile.write(reply->readAll());
	tempExeFile.close();

	if (!QProcess::startDetached(tempExeFile.fileName()) && _listener)
		_listener->onUpdateErrorCallback("Failed to launch the downloaded update.");
}

void CAutoUpdaterGithub::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
	if (_listener)
		_listener->onUpdateDownloadProgress(bytesReceived < bytesTotal ? bytesReceived / (float)bytesTotal : 100.0f);
}
