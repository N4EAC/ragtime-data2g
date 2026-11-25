# The Ragtime desktop app: wxWidgets on top of ragtime_core. Keep logic in
# core where it can be tested without a display; the app draws and wires.

if(RAGTIME_BUILD_WX)
    include(BuildWxWidgets)
else()
    if(APPLE)
        find_package(wxWidgets 3.2.10 REQUIRED COMPONENTS html net core base)
    else()
        find_package(wxWidgets 3.2 REQUIRED COMPONENTS html net core base)
    endif()
    set(RAGTIME_WX_LIBS ${wxWidgets_LIBRARIES})
endif()

# The window icon, built in: ragtime.ico from a resource on Windows, the PNG
# elsewhere. tools/icon/make_icons.sh renders both from res/ragtime.svg.
if(WIN32)
    set(RAGTIME_RESOURCES res/ragtime.rc)
    # windres looks for ragtime.ico on the include path.
    set(RAGTIME_RESOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/res)
else()
    include(EmbedFile)
    ragtime_embed_file(${CMAKE_CURRENT_SOURCE_DIR}/res/ragtime.png
        ${CMAKE_CURRENT_BINARY_DIR}/generated/RagtimeIconPng.h RAGTIME_ICON_PNG)
endif()

add_executable(ragtime WIN32
    src/RagtimeApp.cpp
    src/AppIcon.cpp
    src/MainFrame.cpp
    src/BroadcastPage.cpp
    src/ChatList.cpp
    src/ConnectionBar.cpp
    src/FilesPage.cpp
    src/GearshiftDialog.cpp
    src/DrawnIcon.cpp
    src/DeckParts.cpp
    src/VoiceDeck.cpp
    src/EarControls.cpp
    src/EarFilterBar.cpp
    src/EarFrame.cpp
    src/EarScope.cpp
    src/GearshiftIcon.cpp
    src/HostConnection.cpp
    src/PathStripView.cpp
    src/PathsDialog.cpp
    src/DialMenu.cpp
    src/RigConnection.cpp
    src/RigControl.cpp
    src/Html.cpp
    src/PhraseHighlight.cpp
    src/PictureDialog.cpp
    src/PicturePreview.cpp
    src/PictureFrame.cpp
    src/PictureView.cpp
    src/Pictures.cpp
    src/ReactionChips.cpp
    src/SealBadge.cpp
    src/Prefs.cpp
    src/QuitSignal.cpp
    src/Report.cpp
    src/SessionPage.cpp
    src/SettingsDialog.cpp
    src/RecentWorkPane.cpp
    src/StationList.cpp
    src/Thresholds.cpp
    src/ToggleSwitch.cpp
    src/Theme.cpp
    src/VoiceCards.cpp
    src/SwrBar.cpp
    src/UnpickClick.cpp
    src/ArrivalBar.cpp
    src/TxLamp.cpp
    src/WatchDialog.cpp
    src/globus/GlobusPanel.cpp
    src/globus/GlobusToggle.cpp
    src/globus/GlobusView.cpp
    ${RAGTIME_RESOURCES}
)
target_include_directories(ragtime PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated ${RAGTIME_RESOURCE_DIR})

if(APPLE)
    include(${PROJECT_SOURCE_DIR}/packaging/macos/Bundle.cmake)
endif()

# The ham phrase highlight in the entry lines talks to GTK itself
# (PhraseHighlight.cpp): a GtkEntry takes no wx text styles.
if(UNIX AND NOT APPLE AND NOT RAGTIME_BUILD_WX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    target_include_directories(ragtime SYSTEM PRIVATE ${GTK3_INCLUDE_DIRS})
    target_link_libraries(ragtime PRIVATE ${GTK3_LIBRARIES})
endif()

# wx headers go in as system headers, so a warning in wxWidgets itself never
# fails a -Werror build of our code.
if(RAGTIME_BUILD_WX)
    foreach(lib ${RAGTIME_WX_LIBS})
        get_target_property(dirs ${lib} INTERFACE_INCLUDE_DIRECTORIES)
        target_include_directories(ragtime SYSTEM PRIVATE ${dirs})
    endforeach()
else()
    target_include_directories(ragtime SYSTEM PRIVATE ${wxWidgets_INCLUDE_DIRS})
    target_compile_definitions(ragtime PRIVATE ${wxWidgets_DEFINITIONS})
    if(wxWidgets_CXX_FLAGS)
        separate_arguments(RAGTIME_WX_CXX_FLAGS UNIX_COMMAND "${wxWidgets_CXX_FLAGS}")
        target_compile_options(ragtime PRIVATE ${RAGTIME_WX_CXX_FLAGS})
    endif()
endif()
target_link_libraries(ragtime PRIVATE ragtime_core ragtime_rig ragtime_audio ragtime_image ragtime_denoise ${RAGTIME_WX_LIBS})
ragtime_warnings(ragtime)

if(RAGTIME_TESTS AND NOT WIN32)
    # Runs without a display: --version answers before the toolkit starts.
    add_test(NAME ragtime_app_version COMMAND ragtime --version)
    set_tests_properties(ragtime_app_version PROPERTIES
        PASS_REGULAR_EXPRESSION "^Ragtime ${PROJECT_VERSION}")
endif()

# Widget checks on real windows: skipped (77) with no display.
if(RAGTIME_TESTS AND NOT WIN32)
    function(ragtime_widget_test target)
        add_executable(${target} ${ARGN})
        target_include_directories(${target} PRIVATE src ${PROJECT_SOURCE_DIR}/tests)
        if(RAGTIME_BUILD_WX)
            foreach(lib ${RAGTIME_WX_LIBS})
                get_target_property(dirs ${lib} INTERFACE_INCLUDE_DIRECTORIES)
                target_include_directories(${target} SYSTEM PRIVATE ${dirs})
            endforeach()
        else()
            target_include_directories(${target} SYSTEM PRIVATE ${wxWidgets_INCLUDE_DIRS})
            target_compile_definitions(${target} PRIVATE ${wxWidgets_DEFINITIONS})
            if(wxWidgets_CXX_FLAGS)
                target_compile_options(${target} PRIVATE ${RAGTIME_WX_CXX_FLAGS})
            endif()
        endif()
        target_link_libraries(${target} PRIVATE ragtime_core ${RAGTIME_WX_LIBS})
        if(GTK3_FOUND)
            target_include_directories(${target} SYSTEM PRIVATE ${GTK3_INCLUDE_DIRS})
            target_link_libraries(${target} PRIVATE ${GTK3_LIBRARIES})
        endif()
        ragtime_warnings(${target})
        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(${target} PROPERTIES SKIP_RETURN_CODE 77)
    endfunction()

    ragtime_widget_test(ragtime_chat_list_test ${PROJECT_SOURCE_DIR}/tests/ChatListTest.cpp src/ChatList.cpp)
    ragtime_widget_test(ragtime_recent_work_pane_test ${PROJECT_SOURCE_DIR}/tests/RecentWorkPaneTest.cpp
        src/RecentWorkPane.cpp src/DrawnIcon.cpp src/Html.cpp src/Theme.cpp src/UnpickClick.cpp)
    ragtime_widget_test(ragtime_picture_reader_test ${PROJECT_SOURCE_DIR}/tests/PictureReaderTest.cpp src/Pictures.cpp)
    target_link_libraries(ragtime_picture_reader_test PRIVATE ragtime_image)
    target_compile_definitions(ragtime_picture_reader_test PRIVATE RAGTIME_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    set_tests_properties(ragtime_chat_list_test ragtime_recent_work_pane_test ragtime_picture_reader_test PROPERTIES TIMEOUT 30)
    if(APPLE)
        find_package(Python3 QUIET COMPONENTS Interpreter)
        if(Python3_Interpreter_FOUND)
            add_test(NAME ragtime_app_shutdown_test COMMAND ${Python3_EXECUTABLE}
                ${PROJECT_SOURCE_DIR}/tests/macos/AppShutdownTest.py
                --app $<TARGET_FILE:ragtime> --bench $<TARGET_FILE:ragtime_fake_air>)
            set_tests_properties(ragtime_app_shutdown_test PROPERTIES TIMEOUT 75 SKIP_RETURN_CODE 77)
        endif()
    endif()
endif()
