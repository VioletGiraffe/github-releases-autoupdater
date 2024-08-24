#include "cautoupdatergithub.h"
#include "updateinstaller.hpp"

DISABLE_COMPILER_WARNINGS
#include <QCollator>
#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

#include "maddy/parser.h"
RESTORE_COMPILER_WARNINGS

#include <assert.h>
#include <utility>

static const auto naturalSortQstringComparator = [](const QString& l, const QString& r) {
	static QCollator collator;
	collator.setNumericMode(true);
	collator.setCaseSensitivity(Qt::CaseInsensitive);

	// Fix for the new breaking changes in QCollator in Qt 5.14 - null strings are no longer a valid input
	return collator.compare(qToStringViewIgnoringNull(l), qToStringViewIgnoringNull(r)) < 0;
};

CAutoUpdaterGithub::CAutoUpdaterGithub(QString githubRepositoryName, QString currentVersionString, const std::function<bool (const QString&, const QString&)>& versionStringComparatorLessThan) :
	_repoName(std::move(githubRepositoryName)),
	_currentVersionString(std::move(currentVersionString)),
	_lessThanVersionStringComparator(versionStringComparatorLessThan ? versionStringComparatorLessThan : naturalSortQstringComparator)
{
	assert(_repoName.count(QChar('/')) == 1);
	assert(!_currentVersionString.isEmpty());
}

void CAutoUpdaterGithub::setUpdateStatusListener(UpdateStatusListener* listener)
{
	_listener = listener;
}

void CAutoUpdaterGithub::checkForUpdates()
{
	QNetworkRequest request;
	request.setUrl(QUrl("https://api.github.com/repos/" + _repoName + "/releases"));
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
	auto* reply = qobject_cast<QNetworkReply *>(sender());
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

	const QJsonDocument jsonDocument = QJsonDocument::fromJson(reply->readAll());
	qInfo() << QString::fromUtf8(jsonDocument.toJson()).remove('\r').replace("\\n", "\n");
	assert(jsonDocument.isArray());

	ChangeLog changelog;

	for (const auto& item: jsonDocument.array())
	{
		const auto release = item.toObject();
		if (release["draft"].toBool())
			continue;

		QString updateVersion = release["tag_name"].toString();

		if (updateVersion.startsWith(QStringLiteral(".v")))
			updateVersion.remove(0, 2);
		else if (updateVersion.startsWith('v'))
			updateVersion.remove(0, 1);

		if (!naturalSortQstringComparator(_currentVersionString, updateVersion))
			continue; // version <= _currentVersionString, skipping

#ifdef _WIN32
		static constexpr auto targetExtension = ".exe";
#elif defined __APPLE__
		static constexpr auto targetExtension = ".dmg";
#elif defined __linux__
		static constexpr auto targetExtension = ".AppImage";
#else
		static constexpr auto targetExtension = ".unknown";
#endif

		// Find the appropriate release URL for our platform
		QString url; // [0]["browser_download_url"].toString()
		for (const auto& releaseAsset : release["assets"].toArray())
		{
			const QString assetUrl = releaseAsset.toObject()["browser_download_url"].toString();
			if (assetUrl.endsWith(targetExtension))
			{
				url = assetUrl;
				break;
			}
		}

		if (url.isEmpty())
			url = release["html_url"].toString(); // Fallback in case there is no download link available

		const QString updateChanges = release["body"].toString().remove('\r');
		maddy::Parser markdownParser;
		std::istringstream istream{ updateChanges.toStdString() };
		std::string htmlChanges = markdownParser.Parse(istream);

		QString dateString = release["created_at"].toString();
		dateString = QDateTime::fromString(dateString, Qt::DateFormat::ISODate).toString("dd MMM yyyy");

		const bool prerelease = release["prerelease"].toBool();
		changelog.push_back({ updateVersion, QString::fromStdString(htmlChanges), dateString, url, prerelease });
	}

	if (_listener)
		_listener->onUpdateAvailable(changelog);
}

void CAutoUpdaterGithub::updateDownloaded()
{
	_downloadedBinaryFile.close();

	auto* reply = qobject_cast<QNetworkReply *>(sender());
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
		_listener->onUpdateDownloadProgress(bytesReceived < bytesTotal ? static_cast<float>(bytesReceived * 100) / static_cast<float>(bytesTotal) : 100.0f);
}

void CAutoUpdaterGithub::onNewDataDownloaded()
{
	auto* reply = qobject_cast<QNetworkReply*>(sender());
	if (!reply)
		return;

	_downloadedBinaryFile.write(reply->readAll());
}
