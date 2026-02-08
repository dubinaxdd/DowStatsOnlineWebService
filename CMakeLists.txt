cmake_minimum_required(VERSION 3.16)

project(DowStatsOnlineWebService LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network WebSockets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network WebSockets)

add_executable(DowStatsOnlineWebService
  main.cpp
  OnlineWebService.h
  OnlineWebService.cpp
)

set( OPENSSL_ROOT_DIR ${OPENSSL_ROOT_DIR})
#set( OPENSSL_ROOT_DIR "D:/Qt/6.9.0/Tools/OpenSSLv3/Win_x64")

if(LINUX)
    set(OPENSSL_LIBS "/home/user/Qt6/Tools/OpenSSLv3/src/libcrypto.a" "/home/user/Qt6/Tools/OpenSSLv3/src/libssl.a")
    target_link_libraries(DowStatsOnlineWebService ${OPENSSL_LIBS})
else()
    find_package(OpenSSL REQUIRED)

    target_link_libraries(DowStatsOnlineWebService
      OpenSSL::SSL
      OpenSSL::Crypto
    )
endif()

target_link_libraries(DowStatsOnlineWebService
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Network
  Qt${QT_VERSION_MAJOR}::WebSockets
)

include(GNUInstallDirs)
install(TARGETS DowStatsOnlineWebService
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/config.json"
        "${CMAKE_CURRENT_BINARY_DIR}/config.json"
)
