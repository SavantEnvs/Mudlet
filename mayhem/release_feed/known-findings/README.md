# release_feed known finding

`float-cast-overflow-asset-size.json` crashes the `release_feed` target with UBSan:

    src/updater/Release.cpp:75:49: runtime error: 1.37253e+62 is outside the range of
    representable values of type 'long long'

`static_cast<qint64>(asset.value("size").toDouble())` is undefined behaviour for a JSON `size` that does not
fit in 64 bits. It is a genuine upstream bug, so `float-cast-overflow` is left enabled; this input is kept
here (not in `testsuite/`, which must stay crash-free) as a reproducer:

    /mayhem/release_feed-standalone mayhem/release_feed/known-findings/float-cast-overflow-asset-size.json
