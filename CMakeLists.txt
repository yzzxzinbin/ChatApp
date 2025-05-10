cmake_minimum_required(VERSION 3.16)

project(ChatApp VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Core Network) # 添加Network模块

set(PROJECT_SOURCES
        src/main.cpp
        src/mainwindow.cpp
        src/mainwindow.h
        src/contactmanager.cpp
        src/contactmanager.h
        src/chatmessagedisplay.h
        src/chatmessagedisplay.cpp
        src/networkmanager.h
        src/networkmanager.cpp
        src/addcontactdialog.cpp
        src/addcontactdialog.h
        src/settingsdialog.cpp
        src/settingsdialog.h
        src/peerinfowidget.h
        src/peerinfowidget.cpp
        src/styleutils.h
        src/styleutils.cpp
        src/formattingtoolbarhandler.h
        src/formattingtoolbarhandler.cpp
        src/networkeventhandler.h
        src/networkeventhandler.cpp
        
        src/setupmainstyle.cpp
        src/resources.qrc
        src/chathistorymanager.cpp # Added chathistorymanager.cpp
        src/chathistorymanager.h
)

if(WIN32)
    set(PROJECT_SOURCES ${PROJECT_SOURCES} src/appicon.rc)
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(ChatApp
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(ChatApp SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(ChatApp
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(ChatApp PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Network) # 链接Network模块

if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.ChatApp)
endif()
set_target_properties(ChatApp PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS ChatApp
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(ChatApp)
endif()
