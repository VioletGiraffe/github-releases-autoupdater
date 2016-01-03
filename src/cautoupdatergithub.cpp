#include "cautoupdatergithub.h"

#include <QDebug>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTextBlock>
#include <QTextDocument>

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
	assert(reply);

	connect(&_networkManager, &QNetworkAccessManager::finished, this, &CAutoUpdaterGithub::updateCheckRequestFinished, Qt::UniqueConnection);
}

void CAutoUpdaterGithub::downloadAndInstallUpdate()
{

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
	if (reply->error() != QNetworkReply::NoError)
	{
		if (_listener)
			_listener->onUpdateErrorCallback(reply->errorString());

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

