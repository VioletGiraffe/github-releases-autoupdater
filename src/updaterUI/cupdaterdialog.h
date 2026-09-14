#pragma once

#include "../cautoupdatergithub.h"

DISABLE_COMPILER_WARNINGS
#include <QDialog>
RESTORE_COMPILER_WARNINGS

class QDialogButtonBox;
class QLabel;
class QProgressBar;
class QStackedWidget;
class QTextEdit;

class CUpdaterDialog final : public QDialog, private CAutoUpdaterGithub::UpdateStatusListener
{
public:
	explicit CUpdaterDialog(QWidget *parent,
							const QString& githubRepoName, // Name of the repo, e. g. VioletGiraffe/github-releases-autoupdater
							const QString& versionString,
							bool silentCheck = false);

private:
	void applyUpdate();

private:
	// If no updates are found, the changelog is empty
	void onUpdateAvailable(const CAutoUpdaterGithub::ChangeLog& changelog) override;
	void onUpdateDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) override;
	void onUpdateDownloadFinished() override;
	void onUpdateError(const QString& errorMessage) override;

private:
	QStackedWidget* _pages;
	QWidget* _progressPage;
	QLabel* _lblOperationInProgress;
	QProgressBar* _progressBar;
	QLabel* _lblPercentage;
	QWidget* _changelogPage;
	QLabel* _lblUpdateAvailable;
	QTextEdit* _changeLogViewer;
	QDialogButtonBox* _buttonBox;

	const bool _silent;
	bool _updateDownloadStarted = false;

	QString _latestUpdateUrl;
	CAutoUpdaterGithub _updater;
};

