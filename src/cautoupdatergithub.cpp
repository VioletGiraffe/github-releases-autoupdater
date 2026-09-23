#include "cautoupdatergithub.h"
#include "updateinstaller.hpp"

DISABLE_COMPILER_WARNINGS
#include <QCollator>
#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocale>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSysInfo>
RESTORE_COMPILER_WARNINGS

#include <assert.h>
#include <utility>

static const auto naturalSortQstringComparator = [](const QString& l, const QString& r) {
	static const QCollator collator = [] {
		// QCollator does not collate in the C locale, numeric mode included: English stands in for it
		const QLocale collationLocale = QLocale().collation();
		QCollator c{ collationLocale.language() == QLocale::C ? QLocale{ QLocale::English } : collationLocale };
		c.setNumericMode(true);
		c.setCaseSensitivity(Qt::CaseInsensitive);
		return c;
	}();

	// Fix for the new breaking changes in QCollator in Qt 5.14 - null strings are no longer a valid input
	return collator.compare(qToStringViewIgnoringNull(l), qToStringViewIgnoringNull(r)) < 0;
};

// Architecture names an asset may carry, each mapped to the QSysInfo::currentCpuArchitecture() value it stands for
static constexpr std::pair<QLatin1StringView, QLatin1StringView> assetArchitectureNames[]{
	{ QLatin1StringView{ "x86_64" }, QLatin1StringView{ "x86_64" } },
	{ QLatin1StringView{ "amd64" }, QLatin1StringView{ "x86_64" } },
	{ QLatin1StringView{ "x64" }, QLatin1StringView{ "x86_64" } },
	{ QLatin1StringView{ "aarch64" }, QLatin1StringView{ "arm64" } },
	{ QLatin1StringView{ "arm64" }, QLatin1StringView{ "arm64" } },
};

// The architecture is a '-' or '.' separated part of the asset name, e.g. App-aarch64.AppImage; empty if the name has none
static QLatin1StringView assetArchitecture(const QString& assetName)
{
	static const QRegularExpression separators{ QStringLiteral("[-.]") };
	for (const QString& part : assetName.split(separators, Qt::SkipEmptyParts))
	{
		for (const auto& [name, architecture] : assetArchitectureNames)
		{
			if (part.compare(name, Qt::CaseInsensitive) == 0)
				return architecture;
		}
	}

	return {};
}

// The platform's asset built for the running CPU, else the first one naming no architecture; empty if there is neither
static QString updateAssetUrl(const QJsonArray& assets)
{
	const QString currentArchitecture = QSysInfo::currentCpuArchitecture();
	QString unmarkedAssetUrl;
	for (const auto item : assets)
	{
		const QJsonObject asset = item.toObject();
		const QString name = asset.value("name").toString();
		if (!name.endsWith(UPDATE_FILE_EXTENSION))
			continue;

		const QLatin1StringView architecture = assetArchitecture(name);
		if (architecture == currentArchitecture)
			return asset.value("browser_download_url").toString();

		if (architecture.isEmpty() && unmarkedAssetUrl.isEmpty())
			unmarkedAssetUrl = asset.value("browser_download_url").toString();
	}

	return unmarkedAssetUrl;
}

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

	QJsonParseError parseError;
	const QJsonDocument jsonDocument = QJsonDocument::fromJson(reply->readAll(), &parseError);
	if (!jsonDocument.isArray())
	{
		if (_listener)
			_listener->onUpdateError(parseError.error != QJsonParseError::NoError ? parseError.errorString() : "Unexpected response from GitHub.");
		return;
	}

	ChangeLog changelog;

	for (const QJsonArray releases = jsonDocument.array(); const auto item : releases)
	{
		const auto release = item.toObject();
		if (release["draft"].toBool())
			continue;

		QString updateVersion = release["tag_name"].toString();

		if (updateVersion.startsWith(QStringLiteral(".v")))
			updateVersion.remove(0, 2);
		else if (updateVersion.startsWith('v'))
			updateVersion.remove(0, 1);

		if (!_lessThanVersionStringComparator(_currentVersionString, updateVersion))
			continue; // version <= _currentVersionString, skipping

		QString url = updateAssetUrl(release["assets"].toArray());
		if (url.isEmpty())
			url = release["html_url"].toString(); // Fallback in case there is no download link available

		QString dateString = release["created_at"].toString();
		dateString = QDateTime::fromString(dateString, Qt::DateFormat::ISODate).toString("dd MMM yyyy");

		const bool prerelease = release["prerelease"].toBool();
		changelog.push_back({ updateVersion, release["body"].toString(), dateString, url, prerelease, release["name"].toString() });
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
		_listener->onUpdateDownloadProgress(bytesReceived, bytesTotal);
}

void CAutoUpdaterGithub::onNewDataDownloaded()
{
	auto* reply = qobject_cast<QNetworkReply*>(sender());
	if (!reply)
		return;

	_downloadedBinaryFile.write(reply->readAll());
}
