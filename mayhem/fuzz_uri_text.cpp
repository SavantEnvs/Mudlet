// Text sanitizers/resolvers that process server-supplied strings: telnet/mailto URIs, Lua literal quoting,
// invisible-character escaping, MXP entity resolution/interpolation, update version strings.
#include "LuaLiteral.h"
#include "MailtoUri.h"
#include "SemVer.h"
#include "TEntityResolver.h"
#include "TelnetUri.h"
#include "UntrustedText.h"

#include <QString>

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 1) {
        return -1;
    }
    const uint8_t selector = data[0];
    const QString text = QString::fromUtf8(reinterpret_cast<const char*>(data) + 1, static_cast<qsizetype>(size - 1));

    switch (selector % 7) {
    case 0:
        (void)parseTelnetUri(text);
        break;
    case 1:
        (void)MailtoUri::withOnlyMessageFields(text);
        break;
    case 2:
        (void)LuaLiteral::quote(text);
        break;
    case 3:
        (void)UntrustedText::forTarget(text);
        (void)UntrustedText::forAuthoredText(text);
        break;
    case 4: {
        TEntityResolver resolver;
        resolver.registerEntity(QStringLiteral("name"), QStringLiteral("value"));
        resolver.registerEntity(text.left(8), text.mid(8));
        (void)resolver.getResolution(text);
        (void)resolver.getResolution(text, false);
        (void)resolver.interpolate(text);
        (void)TEntityResolver::resolveCode(text);
        (void)TEntityResolver::resolveCode(text, 10);
        (void)TEntityResolver::resolveCode(text, 16);
        resolver.unregisterEntity(text.left(8));
        break;
    }
    case 5: {
        dblsqd::SemVer a(text.left(text.size() / 2));
        dblsqd::SemVer b(text.mid(text.size() / 2));
        (void)(a < b);
        (void)(a == b);
        (void)a.isValid();
        break;
    }
    default:
        for (const char32_t cp : text.toUcs4()) {
            (void)UntrustedText::unsafeCharacter(cp);
            (void)UntrustedText::unsafeAuthoredCharacter(cp);
        }
        break;
    }
    return 0;
}
