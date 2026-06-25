#
# KPtools - 鍗忚璋冭瘯宸ュ叿
# app/CMakeLists.txt
#

cmake_minimum_required(VERSION 3.16)
project(app VERSION 1.0.0 LANGUAGES C CXX)
if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

#-----------------------------------------------------------------------------
# Include directories (Serial-Studio 椋庢牸: include_directories(src))
#-----------------------------------------------------------------------------
include_directories(src)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

#-----------------------------------------------------------------------------
# Qt modules
#-----------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Widgets)

# 鍙€? SerialPort
find_package(Qt6 QUIET COMPONENTS SerialPort)
if(Qt6SerialPort_FOUND)
    message(STATUS "Qt6SerialPort found, serial support enabled")
    add_definitions(-DHAS_SERIALPORT)
else()
    message(WARNING "Qt6SerialPort not found, serial support disabled")
endif()

# 鍙€? Bluetooth (macOS 閫氬父鏀寔)
find_package(Qt6 QUIET COMPONENTS Bluetooth)
if(Qt6Bluetooth_FOUND)
    message(STATUS "Qt6Bluetooth found, BLE support enabled")
    add_definitions(-DHAS_QTBLUETOOTH)
else()
    message(WARNING "Qt6Bluetooth not found, BLE support disabled")
endif()

#-----------------------------------------------------------------------------
# Lua 5.4
#-----------------------------------------------------------------------------
set(LUA_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../lib/lua-5.4.7/src)
set(LUA_SOURCES
    ${LUA_SRC_DIR}/lapi.c
    ${LUA_SRC_DIR}/lcode.c
    ${LUA_SRC_DIR}/lctype.c
    ${LUA_SRC_DIR}/ldebug.c
    ${LUA_SRC_DIR}/ldo.c
    ${LUA_SRC_DIR}/ldump.c
    ${LUA_SRC_DIR}/lfunc.c
    ${LUA_SRC_DIR}/lgc.c
    ${LUA_SRC_DIR}/llex.c
    ${LUA_SRC_DIR}/lmem.c
    ${LUA_SRC_DIR}/lobject.c
    ${LUA_SRC_DIR}/lopcodes.c
    ${LUA_SRC_DIR}/lparser.c
    ${LUA_SRC_DIR}/lstate.c
    ${LUA_SRC_DIR}/lstring.c
    ${LUA_SRC_DIR}/ltable.c
    ${LUA_SRC_DIR}/ltm.c
    ${LUA_SRC_DIR}/lundump.c
    ${LUA_SRC_DIR}/lvm.c
    ${LUA_SRC_DIR}/lzio.c
    ${LUA_SRC_DIR}/lauxlib.c
    ${LUA_SRC_DIR}/lbaselib.c
    ${LUA_SRC_DIR}/lcorolib.c
    ${LUA_SRC_DIR}/ldblib.c
    ${LUA_SRC_DIR}/liolib.c
    ${LUA_SRC_DIR}/lmathlib.c
    ${LUA_SRC_DIR}/loadlib.c
    ${LUA_SRC_DIR}/loslib.c
    ${LUA_SRC_DIR}/lstrlib.c
    ${LUA_SRC_DIR}/ltablib.c
    ${LUA_SRC_DIR}/lutf8lib.c
    ${LUA_SRC_DIR}/linit.c
)


#-----------------------------------------------------------------------------
# QCodeEditor (from Serial-Studio)
#-----------------------------------------------------------------------------
set(QCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../lib/QCodeEditor)
set(QCE_SOURCES
    ${QCE_DIR}/src/internal/QCodeEditor.cpp
    ${QCE_DIR}/src/internal/QLineNumberArea.cpp
    ${QCE_DIR}/src/internal/QCXXHighlighter.cpp
    ${QCE_DIR}/src/internal/QJavascriptHighlighter.cpp
    ${QCE_DIR}/src/internal/QSyntaxStyle.cpp
    ${QCE_DIR}/src/internal/QStyleSyntaxHighlighter.cpp
    ${QCE_DIR}/src/internal/QGLSLCompleter.cpp
    ${QCE_DIR}/src/internal/QGLSLHighlighter.cpp
    ${QCE_DIR}/src/internal/QLanguage.cpp
    ${QCE_DIR}/src/internal/QXMLHighlighter.cpp
    ${QCE_DIR}/src/internal/QJSONHighlighter.cpp
    ${QCE_DIR}/src/internal/QLuaCompleter.cpp
    ${QCE_DIR}/src/internal/QLuaHighlighter.cpp
    ${QCE_DIR}/src/internal/QPythonCompleter.cpp
    ${QCE_DIR}/src/internal/QPythonHighlighter.cpp
    ${QCE_DIR}/src/internal/QFramedTextAttribute.cpp
    ${QCE_DIR}/src/internal/QJavascriptCompleter.cpp
)

# QCodeEditor headers (needed for AUTOMOC)
set(QCE_HEADERS
    ${QCE_DIR}/include/QCodeEditor
    ${QCE_DIR}/include/QCXXHighlighter
    ${QCE_DIR}/include/QJavascriptHighlighter
    ${QCE_DIR}/include/QLuaHighlighter
    ${QCE_DIR}/include/QPythonHighlighter
    ${QCE_DIR}/include/QGLSLHighlighter
    ${QCE_DIR}/include/QXMLHighlighter
    ${QCE_DIR}/include/QJSONHighlighter
    ${QCE_DIR}/include/QSyntaxStyle
    ${QCE_DIR}/include/QStyleSyntaxHighlighter
    ${QCE_DIR}/include/QLineNumberArea
    ${QCE_DIR}/include/QFramedTextAttribute
    ${QCE_DIR}/include/QLanguage
    ${QCE_DIR}/include/QHighlightRule
    ${QCE_DIR}/include/QHighlightBlockRule
    ${QCE_DIR}/include/QLuaCompleter
    ${QCE_DIR}/include/QPythonCompleter
    ${QCE_DIR}/include/QGLSLCompleter
    ${QCE_DIR}/include/QJavascriptCompleter
    ${QCE_DIR}/include/internal/QCodeEditor.hpp
    ${QCE_DIR}/include/internal/QSyntaxStyle.hpp
    ${QCE_DIR}/include/internal/QStyleSyntaxHighlighter.hpp
    ${QCE_DIR}/include/internal/QLineNumberArea.hpp
    ${QCE_DIR}/include/internal/QFramedTextAttribute.hpp
    ${QCE_DIR}/include/internal/QLanguage.hpp
    ${QCE_DIR}/include/internal/QLuaHighlighter.hpp
    ${QCE_DIR}/include/internal/QLuaCompleter.hpp
    ${QCE_DIR}/include/internal/QCXXHighlighter.hpp
    ${QCE_DIR}/include/internal/QJavascriptHighlighter.hpp
    ${QCE_DIR}/include/internal/QPythonHighlighter.hpp
    ${QCE_DIR}/include/internal/QGLSLHighlighter.hpp
    ${QCE_DIR}/include/internal/QXMLHighlighter.hpp
    ${QCE_DIR}/include/internal/QJSONHighlighter.hpp
    ${QCE_DIR}/include/internal/QPythonCompleter.hpp
    ${QCE_DIR}/include/internal/QGLSLCompleter.hpp
    ${QCE_DIR}/include/internal/QJavascriptCompleter.hpp
    ${QCE_DIR}/include/internal/QHighlightRule.hpp
    ${QCE_DIR}/include/internal/QHighlightBlockRule.hpp
)


# QCodeEditor 璧勬簮
set(QCE_RES_DIR ${QCE_DIR}/resources)
set(QCE_LANG_DIR ${QCE_RES_DIR}/languages)
set(QCE_RES_FILES
    ${QCE_RES_DIR}/default_style.xml
    ${QCE_RES_DIR}/dark_style.xml
    ${QCE_LANG_DIR}/lua.xml
    ${QCE_LANG_DIR}/cpp.xml
    ${QCE_LANG_DIR}/javascript.xml
    ${QCE_LANG_DIR}/python.xml
    ${QCE_LANG_DIR}/glsl.xml
)
foreach(QCE_FILE ${QCE_RES_FILES})
    file(RELATIVE_PATH QCE_ALIAS ${QCE_DIR}/resources ${QCE_FILE})
    set_source_files_properties(${QCE_FILE} PROPERTIES QT_RESOURCE_ALIAS ${QCE_ALIAS})
endforeach()

#-----------------------------------------------------------------------------
# C++ sources (鏂扮洰褰曠粨鏋?
#-----------------------------------------------------------------------------
set(SOURCES
    src/main.cpp
    src/IO/ConnectionManager.cpp
    src/IO/DeviceManager.cpp
    src/IO/FrameProcessor.cpp
    src/IO/Drivers/UART.cpp
    src/IO/Drivers/UsbBulk.cpp
    src/IO/Drivers/ZlgCanFd.cpp
    src/IO/Drivers/BluetoothLE.cpp
    src/DataModel/ProtocolEngine.cpp
    src/DataModel/FirmwareUpgradeManager.cpp
    src/DataModel/DataMonitorModel.cpp
    src/DataModel/QuickCommandManager.cpp
    src/DataModel/SettingsManager.cpp
    src/Script/LuaEngine.cpp
    src/Script/LuaCodeEditor.cpp
    src/Misc/TimerEvents.cpp
    src/Misc/CommonFonts.cpp
    src/Misc/ThemeManager.cpp
    src/Misc/Translator.cpp
    src/Misc/ModuleManager.cpp

    src/Terminal/Terminal.cpp
    src/Terminal/TerminalWidget.cpp
    ${LUA_SOURCES}
    ${QCE_SOURCES}
    ${QCE_HEADERS}
)

set(HEADERS
    src/IO/HAL_Driver.h
    src/IO/ConnectionManager.h
    src/IO/DeviceManager.h
    src/IO/FrameProcessor.h
    src/IO/Drivers/UART.h
    src/IO/Drivers/UsbBulk.h
    src/IO/Drivers/ZlgCanFd.h
    src/IO/Drivers/BluetoothLE.h
    src/DataModel/ProtocolEngine.h
    src/DataModel/FirmwareUpgradeManager.h
    src/DataModel/DataMonitorModel.h
    src/DataModel/QuickCommandManager.h
    src/DataModel/SettingsManager.h
    src/Script/LuaEngine.h
    src/Script/LuaCodeEditor.h
    src/Misc/TimerEvents.h
    src/Misc/CommonFonts.h
    src/Misc/ThemeManager.h
    src/Misc/Translator.h
    src/Misc/ModuleManager.h

    src/Terminal/Terminal.h
    src/Terminal/TerminalWidget.h
)

#-----------------------------------------------------------------------------
# Platform-specific sources
#-----------------------------------------------------------------------------
if(APPLE)
    list(APPEND SOURCES src/Platform/macos_helpers.mm)
    list(APPEND HEADERS src/Platform/macos_helpers.h)
endif()

#-----------------------------------------------------------------------------
# QML sources (鏂扮洰褰曠粨鏋?
#-----------------------------------------------------------------------------
set(QML_SOURCES
    qml/main.qml
    qml/MainWindow/MainWindow.qml
    qml/Panes/Dashboard.qml
    qml/Panes/Console.qml
    qml/Panes/ScriptEditor.qml
    qml/Panes/Setup.qml
    qml/Panes/SetupPanes/Drivers/UART.qml
    qml/Panes/SetupPanes/Drivers/USB.qml
    qml/Panes/SetupPanes/Drivers/CAN.qml
    qml/Panes/SetupPanes/Drivers/BLE.qml
    qml/Panes/SetupPanes/Drivers/Stub.qml
    qml/Panes/ToolPanel.qml
    qml/Widgets/StatusBar.qml
    qml/Widgets/QuickCmd.qml
    qml/Widgets/ResponseFrameBox.qml
)

#-----------------------------------------------------------------------------
# Executable
#-----------------------------------------------------------------------------
qt_add_executable(KPtools
    ${SOURCES}
    ${HEADERS}
)

if(WIN32)
    target_sources(KPtools PRIVATE rcc/KPtools.rc)
    set_target_properties(KPtools PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_include_directories(KPtools PRIVATE
    ${LUA_SRC_DIR}
    ${QCE_DIR}/include
)

if(MSVC)
    target_compile_options(KPtools PRIVATE $<$<CONFIG:Release>:/Od /Ob0>)
endif()

#-----------------------------------------------------------------------------
# QML resources
#-----------------------------------------------------------------------------
qt_add_resources(KPtools "qml"
    PREFIX "/ProtoDebug"
    FILES ${QML_SOURCES}
)

#-----------------------------------------------------------------------------
# Icon resources
#-----------------------------------------------------------------------------
qt_add_resources(KPtools "icons"
    PREFIX "/ProtoDebug"
    FILES
        rcc/icons/proto_512.png
        rcc/icons/proto_256.png
        rcc/icons/proto_128.png
        rcc/icons/proto_64.png
        rcc/icons/proto_32.png
        rcc/icons/proto_16.png
)

#-----------------------------------------------------------------------------
# QCodeEditor 璧勬簮
#-----------------------------------------------------------------------------
qt_add_resources(KPtools "qcodeeditor_resources"
    PREFIX "/"
    FILES ${QCE_RES_FILES}
)

#-----------------------------------------------------------------------------
# Link libraries
#-----------------------------------------------------------------------------
target_link_libraries(KPtools PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
)

if(Qt6SerialPort_FOUND)
    target_link_libraries(KPtools PRIVATE Qt6::SerialPort)
endif()

if(Qt6Bluetooth_FOUND)
    target_link_libraries(KPtools PRIVATE Qt6::Bluetooth)
endif()

if(WIN32)
    target_link_libraries(KPtools PRIVATE setupapi winusb)
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBUSB QUIET libusb-1.0)
    endif()
    if(LIBUSB_FOUND)
        target_compile_definitions(KPtools PRIVATE HAS_LIBUSB)
        target_include_directories(KPtools PRIVATE ${LIBUSB_INCLUDE_DIRS})
        target_link_libraries(KPtools PRIVATE ${LIBUSB_LIBRARIES})
    else()
        message(WARNING "libusb-1.0 not found, USB bulk backend disabled on non-Windows builds")
    endif()
endif()

add_custom_command(TARGET KPtools POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/zlg_can_bridge.py"
        "$<TARGET_FILE_DIR:KPtools>/zlg_can_bridge.py"
    COMMENT "Copy ZLG CAN bridge helper"
)

if(APPLE)
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    target_link_libraries(KPtools PRIVATE ${COCOA_LIBRARY})
endif()

#-----------------------------------------------------------------------------
# macOS bundle
#-----------------------------------------------------------------------------
if(APPLE)
    set_target_properties(KPtools PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "KPtools"
        MACOSX_BUNDLE_GUI_IDENTIFIER "app.kptools"
        MACOSX_BUNDLE_ICON_FILE "ProtoDebug"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    )
    # 澶嶅埗鍥炬爣鍒?bundle
    set(APP_ICON ${CMAKE_CURRENT_SOURCE_DIR}/rcc/icons/ProtoDebug.icns)
    set_source_files_properties(${APP_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(KPtools PRIVATE ${APP_ICON})

    # 钃濈墮鏉冮檺绛惧悕锛堥槻姝?macOS SIGKILL锛?
    # 鍏堟竻鐞?bundle 鍐呮畫鐣欑殑閰嶇疆鏂囦欢锛堟棫鐗?SettingsManager 閬楃暀锛?
    add_custom_command(TARGET KPtools POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory
            "$<TARGET_BUNDLE_DIR:KPtools>/Contents/MacOS/config"
        COMMAND codesign --force --sign - --entitlements
            "${CMAKE_CURRENT_SOURCE_DIR}/ProtoDebug.entitlements"
            "$<TARGET_BUNDLE_DIR:KPtools>"
        COMMENT "Ad-hoc signing with Bluetooth entitlements"
    )
endif()
