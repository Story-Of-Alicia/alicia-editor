cmake_minimum_required(VERSION 3.27)
project(alicia-editor)

add_subdirectory(3rd-party)
find_package(Boost REQUIRED)

add_executable(alicia-editor
    src/editor/main.cpp
    src/editor/util/util.cpp)
target_compile_features(alicia-editor
    PRIVATE cxx_std_20)
target_link_libraries(alicia-editor
    PRIVATE Boost::headers nlohmann_json::nlohmann_json spdlog::spdlog libpak)