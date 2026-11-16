// Updater: GitHub releases JSON -> dblsqd::Release / Feed update selection, and SHA256SUMS parsing.
// Valid JSON (an array of releases, or one release) goes through Release; anything else is parsed as a checksum file.
#include "Feed.h"
#include "Release.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QtGlobal>

#include <cstddef>
#include <cstdint>

static void quietHandler(QtMsgType, const QMessageLogContext&, const QString&) {}

extern "C" int LLVMFuzzerInitialize(int*, char***)
{
    qInstallMessageHandler(quietHandler);
    return 0;
}

namespace {
struct Platform
{
    const char* os;
    const char* arch;
};
constexpr Platform platforms[] = {{"linux", "x86_64"}, {"win", "x86_64"}, {"mac", "arm64"}, {"mac", "x86_64"}, {"", ""}};
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const QByteArray bytes(reinterpret_cast<const char*>(data), static_cast<qsizetype>(size));
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);

    if (parseError.error != QJsonParseError::NoError || (!doc.isArray() && !doc.isObject())) {
        const QString text = QString::fromUtf8(bytes);
        const qsizetype newline = text.indexOf(QLatin1Char('\n'));
        const QString filename = newline < 0 ? text.left(32) : text.left(newline).section(QLatin1Char(' '), -1);
        int entries = 0;
        (void)dblsqd::Feed::findChecksum(text, filename, &entries);
        (void)dblsqd::Feed::findChecksum(text, QStringLiteral("Mudlet-4.17.2-linux-x64.AppImage.tar"));
        return 0;
    }

    QJsonArray array = doc.isArray() ? doc.array() : QJsonArray{doc.object()};
    for (const Platform& platform : platforms) {
        QList<dblsqd::Release> releases;
        for (const QJsonValue& value : std::as_const(array)) {
            if (releases.size() >= 64) {
                break;
            }
            if (value.isObject()) {
                releases.append(dblsqd::Release(value.toObject(), QString::fromLatin1(platform.os), QString::fromLatin1(platform.arch)));
            }
        }
        if (releases.isEmpty()) {
            continue;
        }
        for (const dblsqd::Release& release : std::as_const(releases)) {
            (void)release.getVersion();
            (void)release.getChangelog();
            (void)release.getDate();
            (void)release.getDownloadUrl();
            (void)release.getChecksumsUrl();
            (void)dblsqd::Release::gitHubRef(release.getVersion());
        }
        std::sort(releases.begin(), releases.end(), [](const dblsqd::Release& a, const dblsqd::Release& b) { return b < a; });
        const dblsqd::Release& newest = releases.first();
        const dblsqd::Release& oldest = releases.last();
        (void)dblsqd::Feed::selectUpdates(releases, oldest);
        (void)dblsqd::Feed::selectUpdates(releases, dblsqd::Release(QStringLiteral("4.17.2")));
        (void)dblsqd::Feed::selectReleasesBetween(releases, oldest, newest);
        (void)dblsqd::Feed::selectReleasesBetween(releases, dblsqd::Release(QStringLiteral("0.0.1")), newest);
    }
    return 0;
}
