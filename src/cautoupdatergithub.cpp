#include "cautoupdatergithub.h"

#include <assert.h>

CAutoUpdaterGithub::CAutoUpdaterGithub(const QString& githubRepositoryAddress) :
	_updatePageAddress(githubRepositoryAddress + "/releases/latest/")
{
	assert(githubRepositoryAddress.contains("https://github.com/"));
}

