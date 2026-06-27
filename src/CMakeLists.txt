## CMakeLists.txt
## last updated: 27/06/2026
## cmake -G "Ninja" ..                               > (dynamic Qt6 + static crypto, default)
## [or] cmake -G "Ninja" -DMAGE_STATIC_BUILD=ON ..   > (full static build)
## ninja
cmake_minimum_required(VERSION 3.20)
project(mage_core VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC ON)
option(MAGE_STATIC_BUILD "link static build" OFF)
if(MAGE_STATIC_BUILD)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    set(BUILD_SHARED_LIBS OFF)
    set(OPENSSL_USE_STATIC_LIBS TRUE)
    
    find_package(Qt6 COMPONENTS Core Gui Widgets Svg Concurrent REQUIRED)
else()
    find_package(Qt6 COMPONENTS Core Gui Widgets Svg Concurrent REQUIRED)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    set(BUILD_SHARED_LIBS OFF)
    set(OPENSSL_USE_STATIC_LIBS TRUE)
endif()
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_library(mage_core STATIC
    .cpp/path_handler.cpp
    .cpp/argon2id_hashing.cpp
    .cpp/secure_memory.cpp
    .cpp/__cipher.cpp
    .cpp/aes256gcm.cpp
    .cpp/xchacha20poly1305.cpp
    .cpp/fileformat.cpp
    .cpp/error_msg.cpp
    .cpp/compression.cpp
    .cpp/aes_ni.cpp
    .cpp/keyfile.cpp
)
find_package(OpenSSL REQUIRED)
find_package(sodium QUIET)
find_package(zstd REQUIRED)
find_package(liblzma REQUIRED)
target_include_directories(mage_core PUBLIC
    ${CMAKE_SOURCE_DIR}/.hpp
    ${OPENSSL_INCLUDE_DIR}
)
target_compile_options(mage_core PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -O2
)
target_link_libraries(mage_core PUBLIC
    bcrypt
    OpenSSL::Crypto
    zstd
    LibLZMA::LibLZMA
)
if(sodium_FOUND)
    target_link_libraries(mage_core PUBLIC ${sodium_LIBRARIES})
else()
    target_link_libraries(mage_core PUBLIC sodium)
endif()
add_executable(mage WIN32
    assets/__cache/no/app_icon.rc
    .cpp/main.cpp
    .cpp/gui.cpp
    .hpp/gui.hpp
    .cpp/outs.cpp
    .hpp/outs.hpp
    .cpp/gui_worker.cpp
    .hpp/gui_worker.hpp
    .cpp/settings.cpp
    .hpp/settings.hpp
    .cpp/sfx.cpp
    .hpp/sfx.hpp
    .cpp/stylesheet.cpp
    .cpp/shortcuts.cpp
    .hpp/shortcuts.hpp
)
target_link_libraries(mage
    PRIVATE
    mage_core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Qt6::Concurrent
    dwmapi
    winmm
)
add_custom_command(TARGET mage POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:mage>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_SOURCE_DIR}/assets/__cache/no/qt.conf" "$<TARGET_FILE_DIR:mage>/qt.conf"
)
if(MAGE_STATIC_BUILD)
    message(STATUS "static")
    target_link_options(mage PRIVATE -static -static-libgcc -static-libstdc++)
else()
    message(STATUS "dynamic (df)")
    get_target_property(QT_QMAKE_LOCATION Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_LOCATION}" DIRECTORY)
    
    add_custom_command(TARGET mage POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E env 
            "PATH=${QT_BIN_DIR};$ENV{PATH}"
            ${QT_BIN_DIR}/windeployqt.exe
            --no-translations 
            "$<TARGET_FILE:mage>"
        COMMENT "Copying Qt6..."
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
    add_custom_command(TARGET mage POST_BUILD
        COMMAND bash "${CMAKE_SOURCE_DIR}/deploy_deps.sh" "$<TARGET_FILE:mage>" "$<TARGET_FILE_DIR:mage>"
        COMMENT "Copying the entire MSYS2 fucking env..."
    )
endif()
message(STATUS "MAGE v${PROJECT_VERSION} - build type: ${CMAKE_BUILD_TYPE}  static: ${MAGE_STATIC_BUILD}")
message(STATUS "Toolchain: ${CMAKE_CXX_COMPILER}")

## end