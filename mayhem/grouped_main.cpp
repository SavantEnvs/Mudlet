// main() for the unit-case binary: upstream's GroupedTestMain.cpp pulls in the whole application
// (resources, trigger pool), so this is the same one-class-per-process registry without them.
#include "GroupedTest.h"

#include <QLatin1Char>
#include <QMap>
#include <QString>
#include <QStringList>

#include <cstdio>

namespace {
QMap<QString, GroupedTest::EntryFunction>& registry()
{
    static QMap<QString, GroupedTest::EntryFunction> cases;
    return cases;
}
} // namespace

void GroupedTest::registerCase(const char* name, EntryFunction entry)
{
    registry().insert(QString::fromUtf8(name), entry);
}

void GroupedTest::stopMatchHelpers() {}

int main(int argc, char* argv[])
{
    if (argc < 2 || !registry().contains(QString::fromUtf8(argv[1]))) {
        fprintf(stderr, "usage: %s <test class> [Qt Test arguments]\nthis binary holds: %s\n", argv[0], qUtf8Printable(QStringList(registry().keys()).join(QLatin1Char(' '))));
        return 2;
    }
    return registry().value(QString::fromUtf8(argv[1]))(argc - 1, argv + 1);
}
