// utils::unzip: extraction of .mpackage / map .zip archives (untrusted, downloaded or dropped by the user).
// The input is the archive; it is written to a scratch file under $TMPDIR and extracted into a scratch directory,
// both removed before returning.
#include "utils.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtGlobal>

#include <cstddef>
#include <cstdint>

static void quietHandler(QtMsgType, const QMessageLogContext&, const QString&) {}

extern "C" int LLVMFuzzerInitialize(int*, char***)
{
    qInstallMessageHandler(quietHandler);
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    QTemporaryDir scratch;
    if (!scratch.isValid()) {
        return 0;
    }
    const QString archivePath = scratch.filePath(QStringLiteral("input.mpackage"));
    QFile archive(archivePath);
    if (!archive.open(QIODevice::WriteOnly) || archive.write(reinterpret_cast<const char*>(data), static_cast<qint64>(size)) != static_cast<qint64>(size)) {
        return 0;
    }
    archive.close();

    const QString destination = scratch.filePath(QStringLiteral("out/"));
    QDir().mkpath(destination);
    (void)utils::unzip(archivePath, destination, QDir(scratch.path()));
    return 0;
}
