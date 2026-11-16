// TEncodingHelper: legacy-codepage / lookup-table decoding and encoding of server-controlled bytes.
#include "TEncodingHelper.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <QtGlobal>

#include <cstddef>
#include <cstdint>

namespace {
const QByteArrayList& encodings()
{
    static const QByteArrayList list{"UTF-8",       "ISO 8859-1", "ISO 8859-2", "ISO 8859-5", "ISO 8859-7", "ISO 8859-15", "CP437",        "CP850",
                                     "CP1250",      "CP1251",     "CP1252",     "KOI8-R",     "KOI8-U",     "GBK",         "GB18030",      "Big5",
                                     "EUC-KR",      "Shift_JIS",  "EUC-JP",     "UTF-16",     "UTF-16LE",   "UTF-16BE",    "MACINTOSH",    "M_CP437",
                                     "CP667",       "CP737",      "CP869",      "MEDIEVIA",   "M_CP850",     "M_CP866",    "M_CP1252",   "M_KOI8-R",   "ASCII",      "US-ASCII",    "ISO-8859-1",   "windows-1252"};
    return list;
}
} // namespace

static void quietHandler(QtMsgType, const QMessageLogContext&, const QString&) {}

extern "C" int LLVMFuzzerInitialize(int*, char***)
{
    qInstallMessageHandler(quietHandler);
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 1) {
        return -1;
    }
    const QByteArrayList& list = encodings();
    const QByteArray encoding = list.at(data[0] % list.size());
    const QByteArray payload(reinterpret_cast<const char*>(data) + 1, static_cast<qsizetype>(size - 1));

    const QString text = TEncodingHelper::decode(payload, encoding);
    const QByteArray roundTrip = TEncodingHelper::encode(text, encoding);
    (void)TEncodingHelper::canEncode(text, encoding);
    (void)roundTrip;

    // Encoding names are server/user supplied too (CHARSET negotiation)
    (void)TEncodingHelper::isEncodingAvailable(payload);
    (void)TEncodingHelper::aliases(payload);
    (void)TEncodingHelper::isCustomEncoding(payload);
    (void)TEncodingHelper::decode(payload, payload.left(16));
    return 0;
}
