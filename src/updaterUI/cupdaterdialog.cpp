#include "cupdaterdialog.h"

DISABLE_COMPILER_WARNINGS
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QStringBuilder>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextEdit>
#include <QTextFormat>
#include <QVBoxLayout>
RESTORE_COMPILER_WARNINGS

// CommonMark allows a backslash before any ASCII punctuation character
[[nodiscard]] static QString escapedForMarkdown(const QString& text)
{
	static constexpr QStringView asciiPunctuation = u"!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

	QString escaped;
	escaped.reserve(text.size() * 2);
	for (const QChar c : text)
	{
		if (asciiPunctuation.contains(c))
			escaped += '\\';
		escaped += c;
	}

	return escaped;
}

CUpdaterDialog::CUpdaterDialog(QWidget *parent, const QString& githubRepoName, const QString& versionString, bool silentCheck) :
	QDialog(parent),
	_silent(silentCheck),
	_updater(githubRepoName, versionString)
{
	setWindowTitle(tr("Update checker"));
	setModal(true);

	_lblOperationInProgress = new QLabel(tr("Searching for updates..."), this);
	_progressBar = new QProgressBar(this);
	_progressBar->setMaximum(0);
	_progressBar->setValue(0);
	_progressBar->setTextVisible(false);
	_lblPercentage = new QLabel(this);
	_lblPercentage->setVisible(false);

	QHBoxLayout* progressRow = new QHBoxLayout;
	progressRow->addWidget(_lblOperationInProgress);
	progressRow->addWidget(_progressBar, 1);
	progressRow->addWidget(_lblPercentage);

	_progressPage = new QWidget(this);
	QVBoxLayout* progressPageLayout = new QVBoxLayout(_progressPage);
	progressPageLayout->setContentsMargins(0, 0, 0, 0);
	progressPageLayout->addLayout(progressRow);
	progressPageLayout->addStretch();

	_lblUpdateAvailable = new QLabel(this);
	_changeLogViewer = new QTextEdit(this);
	_changeLogViewer->setReadOnly(true);
	_changeLogViewer->setUndoRedoEnabled(false);
	_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

	_changelogPage = new QWidget(this);
	QVBoxLayout* changelogPageLayout = new QVBoxLayout(_changelogPage);
	changelogPageLayout->setContentsMargins(0, 0, 0, 0);
	changelogPageLayout->addWidget(_lblUpdateAvailable);
	changelogPageLayout->addWidget(_changeLogViewer);
	changelogPageLayout->addWidget(_buttonBox);

	_pages = new QStackedWidget(this);
	_pages->addWidget(_progressPage);
	_pages->addWidget(_changelogPage);

	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->addWidget(_pages);

	resize(560, 330);

	connect(_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(_buttonBox, &QDialogButtonBox::accepted, this, &CUpdaterDialog::applyUpdate);

	_updater.setUpdateStatusListener(this);
	_updater.checkForUpdates();
}

void CUpdaterDialog::applyUpdate()
{
#ifdef _WIN32
	if (_latestUpdateUrl.endsWith(UPDATE_FILE_EXTENSION))
	{
		_progressBar->setMaximum(100);
		_progressBar->setValue(0);
		_lblPercentage->setVisible(true);
		_lblOperationInProgress->setText(tr("Downloading the update..."));
		_pages->setCurrentWidget(_progressPage);

		_updateDownloadStarted = true;
		_updater.downloadAndInstallUpdate(_latestUpdateUrl);
		return;
	}
#endif

	QDesktopServices::openUrl(QUrl(_latestUpdateUrl));
	accept();
}

// If no updates are found, the changelog is empty
void CUpdaterDialog::onUpdateAvailable(const CAutoUpdaterGithub::ChangeLog& changelog)
{
	if (!changelog.empty())
	{
		static constexpr auto versionTitleMarkdown = [](const CAutoUpdaterGithub::VersionEntry& release) -> QString {
			const QString title = !release.releaseTitle.isEmpty() ? release.releaseTitle : release.versionString;
			QString markdown = "**" % escapedForMarkdown(title) % "**";
			if (!release.releaseTitle.isEmpty() && release.releaseTitle != release.versionString)
				markdown += " (tag: " % escapedForMarkdown(release.versionString) % ")";

			if (release.isPrerelease)
				markdown += R"( **\[Pre-release\]**)";

			return markdown;
		};

		_pages->setCurrentWidget(_changelogPage);
		QTextCursor cursor{ _changeLogViewer->document() };
		for (const auto& release : changelog)
		{
			const QString notesMarkdown = !release.versionChangesMarkdown.isEmpty() ? release.versionChangesMarkdown : QStringLiteral("*Release doesn't provide a description*");

			// Each release is its own document: an unclosed code fence in the notes cannot spill into the next release
			QTextDocument releaseDocument;
			releaseDocument.setMarkdown(versionTitleMarkdown(release) % " (" % escapedForMarkdown(release.date) % ")\n\n" % notesMarkdown);

			// Explicit formats: a new block otherwise inherits the previous one's, e. g. its list membership
			// Insertion away from the document start drops a fragment's first block format: the title's is reapplied here
			if (!cursor.atStart())
			{
				cursor.insertBlock(QTextBlockFormat{}, QTextCharFormat{}); // Empty line between releases
				cursor.insertBlock(releaseDocument.firstBlock().blockFormat(), QTextCharFormat{});
			}
			cursor.insertFragment(QTextDocumentFragment{ &releaseDocument });
		}

		_latestUpdateUrl = changelog.front().versionUpdateUrl;

		const bool prerelease = changelog.front().isPrerelease;
		_lblUpdateAvailable->setText(prerelease ? tr("A new pre-release version is available!") : tr("A new version is available!"));
#ifdef _WIN32
		_buttonBox->button(QDialogButtonBox::Ok)->setText(prerelease ? tr("Install pre-release") : tr("Install"));
#else
		_buttonBox->button(QDialogButtonBox::Ok)->setText(prerelease ? tr("Download pre-release") : tr("Download"));
#endif
		show();
	}
	else
	{
		accept();
		if (!_silent)
			QMessageBox::information(this, tr("No update available"), tr("You already have the latest version of the program."));
	}
}

void CUpdaterDialog::onUpdateDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
	if (bytesTotal <= 0)
	{
		_progressBar->setMaximum(0);
		_lblPercentage->setText(locale().formattedDataSize(bytesReceived));
		return;
	}

	const double percentage = static_cast<double>(bytesReceived) * 100.0 / static_cast<double>(bytesTotal);
	_progressBar->setMaximum(100);
	_progressBar->setValue(static_cast<int>(percentage));
	_lblPercentage->setText(QString::number(percentage, 'f', 2) + " %");
}

void CUpdaterDialog::onUpdateDownloadFinished()
{
	accept();
}

void CUpdaterDialog::onUpdateError(const QString& errorMessage)
{
	reject();
	if (_updateDownloadStarted)
		QMessageBox::critical(this, tr("Error installing the update"), errorMessage);
	else if (!_silent)
		QMessageBox::critical(this, tr("Error checking for updates"), errorMessage);
}
