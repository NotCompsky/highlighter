cmake_minimum_required(VERSION 3.17)

project(ImageViewer)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_AUTOMOC ON)
find_package(Qt5 COMPONENTS Widgets REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(ImageViewer
    main.cpp
    image-viewer.cpp
)
target_link_libraries(ImageViewer
    PRIVATE
    Qt5::Widgets
    OpenSSL::Crypto
)
