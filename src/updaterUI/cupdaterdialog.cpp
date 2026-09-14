#include "cupdaterdialog.h"

DISABLE_COMPILER_WARNINGS
#include "ui_cupdaterdialog.h"

#include <QDesktopServices>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QStringBuilder>
RESTORE_COMPILER_WARNINGS

CUpdaterDialog::CUpdaterDialog(QWidget *parent, const QString& githubRepoName, const QString& versionString, bool silentCheck) :
	QDialog(parent),
	ui(new Ui::CUpdaterDialog),
	_silent(silentCheck),
	_updater(githubRepoName, versionString)
{
	ui->setupUi(this);

	if (_silent)
		hide();

	connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &CUpdaterDialog::applyUpdate);
	ui->buttonBox->button(QDialogButtonBox::Ok)->setText("Install");

	ui->stackedWidget->setCurrentIndex(0);
	ui->progressBar->setMaximum(0);
	ui->progressBar->setValue(0);
	ui->lblPercentage->setVisible(false);

	_updater.setUpdateStatusListener(this);
	_updater.checkForUpdates();
}

CUpdaterDialog::~CUpdaterDialog()
{
	delete ui;
}

void CUpdaterDialog::applyUpdate()
{
#ifdef _WIN32
	if (_latestUpdateUrl.endsWith(UPDATE_FILE_EXTENSION))
	{
		ui->progressBar->setMaximum(100);
		ui->progressBar->setValue(0);
		ui->lblPercentage->setVisible(true);
		ui->lblOperationInProgress->setText("Downloading the update...");
		ui->stackedWidget->setCurrentIndex(0);

		_updateDownloadStarted = true;
		_updater.downloadAndInstallUpdate(_latestUpdateUrl);
	} else {
		QDesktopServices::openUrl(QUrl(_latestUpdateUrl));
		accept();
	}
#else
	QMessageBox msg(
		QMessageBox::Question,
		tr("Manual update required"),
		tr("Automatic update is not supported on this operating system. Do you want to download and install the update manually?"),
		QMessageBox::Yes | QMessageBox::No,
		this);

	if (msg.exec() == QMessageBox::Yes)
		QDesktopServices::openUrl(QUrl(_latestUpdateUrl));
#endif
}

// If no updates are found, the changelog is empty
void CUpdaterDialog::onUpdateAvailable(const CAutoUpdaterGithub::ChangeLog& changelog)
{
	if (!changelog.empty())
	{
		static constexpr auto annotateEmptyDescription = [](const QString& desc) -> QString {
			return !desc.isEmpty() ? desc : "<br><i>Release doesn't provide a description</i><br>";
		};

		static constexpr auto versionTitleHtml = [](const CAutoUpdaterGithub::VersionEntry& release) -> QString {
			const QString title = !release.releaseTitle.isEmpty() ? release.releaseTitle : release.versionString;
			QString html = "<b>" % title.toHtmlEscaped() % "</b>";
			if (!release.releaseTitle.isEmpty() && release.releaseTitle != release.versionString)
				html += " (tag: " % release.versionString.toHtmlEscaped() % ")";

			if (release.isPrerelease)
				html += " [Pre-release]";

			return html;
		};

		QString html;
		ui->stackedWidget->setCurrentIndex(1);
		for (const auto& changelogItem : changelog)
		{
			html.append(
				versionTitleHtml(changelogItem) % " (" % changelogItem.date % ")" % annotateEmptyDescription(changelogItem.versionChanges) % "<br>"
			);
		}


		ui->changeLogViewer->setHtml(html);
		_latestUpdateUrl = changelog.front().versionUpdateUrl;
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
		ui->progressBar->setMaximum(0);
		ui->lblPercentage->setText(locale().formattedDataSize(bytesReceived));
		return;
	}

	const double percentage = static_cast<double>(bytesReceived) * 100.0 / static_cast<double>(bytesTotal);
	ui->progressBar->setMaximum(100);
	ui->progressBar->setValue(static_cast<int>(percentage));
	ui->lblPercentage->setText(QString::number(percentage, 'f', 2) + " %");
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
