#pragma once

#include <QString>

class CAutoUpdaterGithub
{
public:
	explicit CAutoUpdaterGithub(const QString& githubRepositoryAddress);

private:
	const QString _updatePageAddress;
};

