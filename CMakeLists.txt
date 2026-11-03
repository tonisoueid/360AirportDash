cmake_minimum_required(VERSION 3.21)
project(360AirportDash VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

add_executable(360AirportDash
    src/main.cpp
    src/Theme.h
    src/AppIcon.h
    src/SiteData.h
    src/Airport.h
    src/AirportSeed.h
    src/AirportSeed.cpp
    src/AirportSearch.h
    src/AirportSearch.cpp
    src/AirportRegistry.h
    src/AirportRegistry.cpp
    src/SiteRegistry.h
    src/SiteRegistry.cpp
    src/FeedService.h
    src/FeedService.cpp
    src/widgets/Panel.h
    src/widgets/Panel.cpp
    src/widgets/IconGlyph.h
    src/widgets/ExpandButton.h
    src/widgets/BodySplitter.h
    src/widgets/TrendChart.h
    src/widgets/TrendChart.cpp
    src/SiteCard.h
    src/SiteCard.cpp
    src/EventLog.h
    src/EventLog.cpp
    src/AdminDialog.h
    src/AdminDialog.cpp
    src/MainWindow.h
    src/MainWindow.cpp
)

target_include_directories(360AirportDash PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(360AirportDash PRIVATE Qt6::Widgets Qt6::Network)

# GUI subsystem on Windows: no console window behind the dashboard.
if(WIN32)
    set_target_properties(360AirportDash PROPERTIES WIN32_EXECUTABLE TRUE)

    # Embed the Explorer/taskbar icon. packaging/windows/build-win64.sh renders
    # icon.ico from src/AppIcon.h and passes it here; plain builds without it
    # still get the runtime window icon from main.cpp.
    set(APP_ICON_ICO "" CACHE FILEPATH "Path to a multi-size .ico to embed in the .exe")
    if(APP_ICON_ICO AND EXISTS "${APP_ICON_ICO}")
        enable_language(RC)
        set(APP_RC "${CMAKE_CURRENT_BINARY_DIR}/360AirportDash.rc")
        file(WRITE "${APP_RC}" "1 ICON \"${APP_ICON_ICO}\"\n")
        target_sources(360AirportDash PRIVATE "${APP_RC}")
    endif()
endif()

install(TARGETS 360AirportDash BUNDLE DESTINATION . RUNTIME DESTINATION bin)

# Linux desktop integration (dock/launcher icon): `cmake --install` places the
# launcher entry and renders the hicolor icon with the freshly built binary.
if(UNIX AND NOT APPLE)
    install(FILES packaging/linux/360airportdash.desktop
            DESTINATION share/applications)
    install(CODE "
        set(_dir \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/256x256/apps\")
        file(MAKE_DIRECTORY \"\${_dir}\")
        execute_process(
            COMMAND \${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen
                    \"$<TARGET_FILE:360AirportDash>\" --icon \"\${_dir}/360airportdash.png\"
            RESULT_VARIABLE _rc)
        if(NOT _rc EQUAL 0)
            message(WARNING \"could not render the 256px app icon (rc=\${_rc})\")
        endif()
    ")
endif()
