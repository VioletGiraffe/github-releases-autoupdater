#include "updateinstaller.hpp"

#include "../cpp-template-utils/compiler/compiler_warnings_control.h"

DISABLE_COMPILER_WARNINGS
#include <QProcess>
RESTORE_COMPILER_WARNINGS

bool UpdateInstaller::install(const QString& downloadedUpdateFilePath)
{
	return QProcess::startDetached('\"' + downloadedUpdateFilePath + '\"', {});
}
