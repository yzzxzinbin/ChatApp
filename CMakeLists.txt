cmake_minimum_required(VERSION 3.16)

project(ChatApp VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Core Network Sql)

# Set include directories
set(PROJECT_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/includes
)

# Define header files
set(PROJECT_HEADERS
    includes/mainwindow.h
    includes/contactmanager.h
    includes/chatmessagedisplay.h
    includes/addcontactdialog.h
    includes/peerinfowidget.h
    includes/mainwindowstyle.h
    includes/formattingtoolbarhandler.h
    includes/chathistorymanager.h
    includes/logindialog.h
    includes/networkmanager.h
    includes/networkeventhandler.h
    includes/settingsdialog.h
    includes/databasemanager.h
    includes/filetransfermanager.h
)

# Define source files
set(PROJECT_SOURCES
    src/main.cpp
    
    # MainWindow sources
    src/MainWindow/mainwindow.cpp
    src/MainWindow/contactmanager.cpp
    src/MainWindow/chatmessagedisplay.cpp
    src/MainWindow/addcontactdialog.cpp
    src/MainWindow/peerinfowidget.cpp
    src/MainWindow/setupmainstyle.cpp
    src/MainWindow/formattingtoolbarhandler.cpp
    src/MainWindow/chathistorymanager.cpp
    src/MainWindow/mainwindowstyle.cpp
    
    # Network sources
    src/NetworkModule/networkmanager.cpp
    src/NetworkModule/networkmanager_udp.cpp
    src/NetworkModule/networkeventhandler.cpp

    # Settings sources
    src/SettingDialog/settingsdialog.cpp

    # LoginWindow sources
    src/LoginWindow/logindialog.cpp
    src/LoginWindow/logindialogstyle.cpp

    # Database sources
    src/DatabaseModule/databasemanager.cpp

    # FileTransfer sources
    src/FileTransferModule/filetransfermanager.cpp

    # Resources
    src/ResourceImport/resources.qrc
    
    # Include all headers in the project
    ${PROJECT_HEADERS}
)

# Add include directories to the build
include_directories(${PROJECT_INCLUDE_DIRS})


if(WIN32)
    set(PROJECT_SOURCES ${PROJECT_SOURCES} src/ResourceImport/appicon.rc)
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

target_link_libraries(ChatApp PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Sql)

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
