# Mayhem build of the Host-free units of Mudlet that parse untrusted input (MXP, XML import,
# updater feed, text encodings, URIs). Not the full Qt application: that needs ~60 libraries.
# MAYHEM_MODE=fuzz   -> harnesses (libFuzzer and standalone driver), compiled with the sanitizer flags
# MAYHEM_MODE=tests  -> upstream's own QtTest cases for the same units, compiled with normal flags
cmake_minimum_required(VERSION 3.25)
project(mudlet_mayhem LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(MAYHEM_MODE "fuzz" CACHE STRING "fuzz or tests")
set(MAYHEM_ENGINE "-fsanitize=fuzzer" CACHE STRING "link flags for the fuzzing engine")
set(MAYHEM_TARGETS xml_import mxp release_feed encoding uri_text unzip)
set(MAYHEM_STANDALONE_MAIN "" CACHE FILEPATH "LLVM StandaloneFuzzTargetMain.c")
set(MAYHEM_LSAN_OFF_SRC "${CMAKE_CURRENT_SOURCE_DIR}/lsan_off.cc" CACHE FILEPATH "defines __lsan_is_turned_off()")

get_filename_component(SRC ${CMAKE_CURRENT_SOURCE_DIR}/../src ABSOLUTE)
get_filename_component(TST ${CMAKE_CURRENT_SOURCE_DIR}/../test ABSOLUTE)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Widgets Test Core5Compat)
find_package(pugixml REQUIRED)
find_package(libzip REQUIRED)

set(MXP_SOURCES
    ${SRC}/MxpTag.cpp
    ${SRC}/TEntityHandler.cpp
    ${SRC}/TEntityResolver.cpp
    ${SRC}/TMxpBRTagHandler.cpp
    ${SRC}/TMxpColorTagHandler.cpp
    ${SRC}/TMxpCustomElementTagHandler.cpp
    ${SRC}/TMxpDestTagHandler.cpp
    ${SRC}/TMxpElementDefinitionHandler.cpp
    ${SRC}/TMxpElementRegistry.cpp
    ${SRC}/TMxpEntityTagHandler.cpp
    ${SRC}/TMxpExpireTagHandler.cpp
    ${SRC}/TMxpFontTagHandler.cpp
    ${SRC}/TMxpFormattingTagsHandler.cpp
    ${SRC}/TMxpFrameTagHandler.cpp
    ${SRC}/TMxpHRTagHandler.cpp
    ${SRC}/TMxpImageTagHandler.cpp
    ${SRC}/TMxpLinkTagHandler.cpp
    ${SRC}/TMxpMusicTagHandler.cpp
    ${SRC}/TMxpNodeBuilder.cpp
    ${SRC}/TMxpProcessor.cpp
    ${SRC}/TMxpSendTagHandler.cpp
    ${SRC}/TMxpSoundTagHandler.cpp
    ${SRC}/TMxpStatTagHandler.cpp
    ${SRC}/TMxpSupportTagHandler.cpp
    ${SRC}/TMxpTagHandler.cpp
    ${SRC}/TMxpTagParser.cpp
    ${SRC}/TMxpTagProcessor.cpp
    ${SRC}/TMxpVarTagHandler.cpp
    ${SRC}/TMxpVersionTagHandler.cpp
    ${SRC}/LuaLiteral.cpp
    ${SRC}/MailtoUri.cpp
    ${SRC}/TelnetUri.cpp
    ${SRC}/UntrustedText.cpp
    ${SRC}/TStringUtils.cpp
    ${SRC}/TEncodingHelper.cpp
    ${SRC}/TEncodingTable.cpp
    ${SRC}/TTextCodec.cpp
)
set(XML_SOURCES ${SRC}/XmlImportReader.cpp ${SRC}/utils.cpp)
set(UPDATER_SOURCES ${SRC}/updater/Feed.cpp ${SRC}/updater/Release.cpp ${SRC}/updater/SemVer.cpp)

add_library(mudlet_units STATIC ${MXP_SOURCES} ${XML_SOURCES} ${UPDATER_SOURCES})
target_include_directories(mudlet_units PUBLIC ${SRC} ${SRC}/updater ${TST} ${TST}/functional_tests)
target_link_libraries(mudlet_units PUBLIC Qt6::Core Qt6::Gui Qt6::Network Qt6::Widgets Qt6::Core5Compat pugixml::pugixml libzip::zip)
# Qt Test's default test-data paths would bake the build directory into every object
target_compile_definitions(mudlet_units PUBLIC QT_SKIP_DEFAULT_TESTCASE_DIRS)

if(MAYHEM_MODE STREQUAL "fuzz")
    # OBJECT, not STATIC: the ASan runtime only weakly references the hook, so an archive member is never pulled in
    add_library(mayhem_lsan_off OBJECT ${MAYHEM_LSAN_OFF_SRC})
    add_library(standalone_main OBJECT ${MAYHEM_STANDALONE_MAIN})
    set_source_files_properties(${MAYHEM_STANDALONE_MAIN} PROPERTIES LANGUAGE C)
    foreach(h ${MAYHEM_TARGETS})
        # <h>: libFuzzer binary; <h>-standalone: run-once reproducer reading one input file
        add_executable(fuzz_${h} ${CMAKE_CURRENT_SOURCE_DIR}/fuzz_${h}.cpp)
        target_link_libraries(fuzz_${h} PRIVATE mudlet_units mayhem_lsan_off ${MAYHEM_ENGINE})
        set_target_properties(fuzz_${h} PROPERTIES OUTPUT_NAME ${h})
        add_executable(fuzz_${h}_standalone ${CMAKE_CURRENT_SOURCE_DIR}/fuzz_${h}.cpp $<TARGET_OBJECTS:standalone_main>)
        target_link_libraries(fuzz_${h}_standalone PRIVATE mudlet_units mayhem_lsan_off)
        set_target_properties(fuzz_${h}_standalone PROPERTIES OUTPUT_NAME ${h}-standalone)
    endforeach()
else()
    set(CMAKE_AUTOMOC ON)
    # upstream's own cases for the units above, run through the one-class-per-process grouped main
    set(GROUPED_CASES
        TMxpTagParserTest TMxpSendTagHandlerTest TMxpLinkTagHandlerTest TMxpEntityTagHandlerTest
        TMxpVersionTagTest TMxpFormattingTagsTest TMxpCustomElementTagHandlerTest TMxpModeSecurityTest
        TMxpEdgeCasesTest TMxpElementDefinitionHandlerTest TMxpFrameDestTagHandlerTest
        TEntityResolverTest TEntityHandlerTest TEncodingHelperTest UntrustedTextTest XmlImportReaderTest
        LuaLiteralTest TAreaSpanIndexTest TAreaZLevelIndexTest TAreaGridIndexTest EventLoopPumpTest)
    list(TRANSFORM GROUPED_CASES APPEND .cpp OUTPUT_VARIABLE grouped_sources_rel)
    set(grouped_sources)
    foreach(f ${grouped_sources_rel})
        list(APPEND grouped_sources ${TST}/${f})
    endforeach()
    # the map indexes and event-loop pump those cases cover include only Qt Core, so they are compiled in directly
    add_executable(mudlet_unit_cases ${CMAKE_CURRENT_SOURCE_DIR}/grouped_main.cpp ${grouped_sources}
        ${SRC}/TAreaSpanIndex.cpp ${SRC}/TAreaZLevelIndex.cpp ${SRC}/TAreaGridIndex.cpp ${SRC}/EventLoopPump.cpp)
    find_path(LUA51_INCLUDE_PARENT lua5.1/lua.h REQUIRED)
    find_library(LUA51_LIBRARY NAMES lua5.1 REQUIRED)
    target_link_libraries(mudlet_unit_cases PRIVATE mudlet_units Qt6::Test ${LUA51_LIBRARY})
    set_property(SOURCE ${TST}/LuaLiteralTest.cpp APPEND PROPERTY COMPILE_DEFINITIONS INCLUDE_VERSIONED_LUA_HEADERS)
    set_property(SOURCE ${TST}/XmlImportReaderTest.cpp APPEND PROPERTY COMPILE_DEFINITIONS MUDLET_SRC_DIR="${SRC}")

    foreach(t SemVerTest ReleaseChecksumPairingTest ReleasePlatformAssetTest ReleaseChangelogSpanTest)
        add_executable(${t} ${TST}/${t}.cpp)
        target_link_libraries(${t} PRIVATE mudlet_units Qt6::Test)
    endforeach()

    # upstream's other standalone cases that need neither the Mudlet library nor a third-party library
    find_package(Qt6 REQUIRED COMPONENTS DBus)
    add_executable(MudletProxyStyleDockTitleTest ${TST}/MudletProxyStyleDockTitleTest.cpp ${SRC}/MudletProxyStyle.cpp)
    add_executable(LayoutMirrorTest ${TST}/LayoutMirrorTest.cpp ${SRC}/FlowLayout.cpp ${SRC}/PairGridLayout.cpp)
    foreach(t MudletProxyStyleDockTitleTest LayoutMirrorTest)
        target_include_directories(${t} PRIVATE ${SRC})
        target_link_libraries(${t} PRIVATE Qt6::Test Qt6::Widgets Qt6::DBus)
    endforeach()
    add_executable(CrashReportUploadTest ${TST}/CrashReportUploadTest.cpp ${SRC}/crash_reporter/sendCrashReport.cpp)
    target_include_directories(CrashReportUploadTest PRIVATE ${SRC}/crash_reporter)
    target_link_libraries(CrashReportUploadTest PRIVATE Qt6::Test Qt6::Network)
    foreach(t CMakeListsConsistencyTest LuaFunctionListTest DesignColourLiteralTest)
        add_executable(${t} ${TST}/${t}.cpp)
        target_link_libraries(${t} PRIVATE Qt6::Test Qt6::Gui)
        target_compile_definitions(${t} PRIVATE MUDLET_SRC_DIR="${SRC}")
    endforeach()
    # upstream's bundled communi and lua51, for the cases that link only those (test/CMakeLists.txt)
    # communi runs moc itself (QT_GENERATE_MOC), as upstream's top level, which leaves AUTOMOC off, expects
    set(CMAKE_AUTOMOC OFF)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/communi ${CMAKE_BINARY_DIR}/communi)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/lua51 ${CMAKE_BINARY_DIR}/lua51)
    set(CMAKE_AUTOMOC ON)
    target_compile_definitions(communi PUBLIC IRC_STATIC)
    add_executable(IrcMessageFormatterTest ${TST}/IrcMessageFormatterTest.cpp ${SRC}/ircmessageformatter.cpp)
    target_include_directories(IrcMessageFormatterTest PRIVATE ${SRC})
    target_link_libraries(IrcMessageFormatterTest PRIVATE Qt6::Test communi)
    add_executable(IrcMessageComposerTest ${TST}/IrcMessageComposerTest.cpp)
    target_link_libraries(IrcMessageComposerTest PRIVATE Qt6::Test Qt6::Network communi)
    add_executable(LuaWriteBarrierTest ${TST}/LuaWriteBarrierTest.cpp)
    target_link_libraries(LuaWriteBarrierTest PRIVATE Qt6::Test lua51)

    add_executable(XdgRecipeConsistencyTest ${TST}/XdgRecipeConsistencyTest.cpp)
    target_link_libraries(XdgRecipeConsistencyTest PRIVATE Qt6::Test)
    target_compile_definitions(XdgRecipeConsistencyTest PRIVATE MUDLET_TEST_DIR="${TST}")
endif()
