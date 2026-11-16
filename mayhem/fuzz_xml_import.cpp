// XmlImportReader: the reader behind Mudlet package, profile and map imports (untrusted XML).
// First byte picks the reader (pugixml tree vs QXmlStreamReader) and how elements are walked.
#include "XmlImportReader.h"

#include <QBuffer>
#include <QByteArray>
#include <QXmlStreamReader>

#include <cstddef>
#include <cstdint>

namespace {
constexpr int maxTokens = 20000;

void walk(XmlImportReader& reader, uint8_t mode)
{
    int tokens = 0;
    while (!reader.atEnd() && tokens++ < maxTokens) {
        const auto token = reader.readNext();
        if (token == QXmlStreamReader::StartElement) {
            (void)reader.name();
            auto attrs = reader.attributes();
            (void)attrs.isEmpty();
            (void)attrs.hasAttribute(QStringLiteral("name"));
            (void)attrs.value(QStringLiteral("name"));
            (void)attrs.value(QStringLiteral("isActive"));
            for (const auto& attr : attrs.all()) {
                (void)attr;
            }
            (void)reader.lineNumber();
            (void)reader.columnNumber();
            (void)reader.characterOffset();
            switch ((mode >> 2) % 4) {
            case 0:
                break;
            case 1:
                (void)reader.readElementText(QXmlStreamReader::IncludeChildElements);
                break;
            case 2:
                (void)reader.readElementText(QXmlStreamReader::SkipChildElements);
                break;
            default:
                reader.skipCurrentElement();
                break;
            }
        } else if (token == QXmlStreamReader::Characters) {
            (void)reader.text();
            (void)reader.isCDATA();
        }
    }
    if (reader.hasError()) {
        (void)reader.errorString();
        (void)reader.error();
        (void)reader.lineNumber();
        (void)reader.columnNumber();
    }
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 1) {
        return -1;
    }
    const uint8_t mode = data[0];
    XmlImportReader::setAlwaysUseStreamReaderForTesting(mode & 1);
    QByteArray xml(reinterpret_cast<const char*>(data) + 1, static_cast<qsizetype>(size - 1));
    QBuffer buffer(&xml);
    buffer.open(QIODevice::ReadOnly);
    XmlImportReader reader;
    reader.setDevice(&buffer);
    walk(reader, mode);
    return 0;
}
