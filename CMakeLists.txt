cmake_minimum_required(VERSION 3.27)
project(alicia_editor)

add_subdirectory(3rd-party)
find_package(Boost REQUIRED)

add_library(libpak
    src/libpak/libpak.cpp)
target_compile_features(libpak
        PRIVATE cxx_std_20)
target_include_directories(libpak PUBLIC include/)
target_link_libraries(libpak PUBLIC zlibstatic)

add_executable(alicia_editor
    src/editor/main.cpp
    src/editor/util/util.cpp)
target_compile_features(alicia_editor
    PRIVATE cxx_std_20)
target_link_libraries(alicia_editor
    PRIVATE Boost::headers nlohmann_json::nlohmann_json spdlog::spdlog libpak)