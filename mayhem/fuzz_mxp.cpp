// TMxpProcessor: the MXP (MUD eXtension Protocol) state machine that parses tags and entities in game server output.
// First two bytes pick the mode code and options; the rest is fed byte by byte as server output.
#include "TMxpProcessor.h"
#include "TMxpStubClient.h"

#include <QtGlobal>

#include <cstddef>
#include <cstdint>
#include <string>

static void quietHandler(QtMsgType, const QMessageLogContext&, const QString&) {}

extern "C" int LLVMFuzzerInitialize(int*, char***)
{
    qInstallMessageHandler(quietHandler);
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 2) {
        return -1;
    }
    const uint8_t modeByte = data[0];
    const uint8_t options = data[1];
    data += 2;
    size -= 2;

    TMxpStubClient client;
    TMxpProcessor processor(&client);
    processor.enable();
    processor.setMode(modeByte % 8);
    const bool resolveCustom = options & 1;
    const bool rawPath = options & 2;

    if (rawPath) {
        processor.processRawInput(QByteArrayView(reinterpret_cast<const char*>(data), static_cast<qsizetype>(size)));
    } else {
        for (size_t i = 0; i < size; ++i) {
            char ch = static_cast<char>(data[i]);
            const TMxpProcessingResult result = processor.processMxpInput(ch, resolveCustom);
            if (result == HANDLER_COMMIT_LINE) {
                (void)processor.takeIgnoredNewline();
                processor.resetToDefaultMode(options & 4);
            } else if (result == HANDLER_INSERT_ENTITY_CUST || result == HANDLER_INSERT_ENTITY_SYS || result == HANDLER_INSERT_ENTITY_LIT) {
                (void)processor.getEntityValue();
            }
            if ((options & 8) && ch == '\033') {
                (void)processor.abortCurrentTag();
            }
        }
    }
    (void)processor.abortCurrentTag();
    return 0;
}
