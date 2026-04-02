cmake_minimum_required(VERSION 3.16)

project(SmartCampusSecondhandTradingSystemServer LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Sql Network WebSockets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Sql Network WebSockets)

add_executable(SmartCampusSecondhandTradingSystemServer
  main.cpp
  databasemanager.h databasemanager.cpp
  httpserver.h httpserver.cpp
  httpconnection.h httpconnection.cpp
  websocketserver.h websocketserver.cpp
  apihandler.h apihandler.cpp
  hunyuanclient.h hunyuanclient.cpp
)
target_link_libraries(SmartCampusSecondhandTradingSystemServer Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Sql Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::WebSockets)

include(GNUInstallDirs)
install(TARGETS SmartCampusSecondhandTradingSystemServer
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
