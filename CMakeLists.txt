cmake_minimum_required(VERSION 3.11)

project(upload_dumper C)

add_executable(${PROJECT_NAME}
    ${PROJECT_SOURCE_DIR}/src/dumper.c
    ${PROJECT_SOURCE_DIR}/src/hexdump.c
)

target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

# Libusb configuration
if(MSVC)
    target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/libusb-1.0.26-binaries/VS2015-x64/include)
    target_link_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/libusb-1.0.26-binaries/VS2015-x64/lib)
    target_link_libraries(${PROJECT_NAME} PRIVATE libusb-1.0)
elseif(WIN32)
    target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/libusb-1.0.26-binaries/libusb-MinGW-x64/include)
    target_link_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/libusb-1.0.26-binaries/libusb-MinGW-x64/lib)
    target_link_libraries(${PROJECT_NAME} PRIVATE libusb-1.0)
elseif(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
    message(STATUS "libusb include dirs: ${LIBUSB_INCLUDE_DIRS}")
    target_include_directories(${PROJECT_NAME} PRIVATE ${LIBUSB_INCLUDE_DIRS})
    target_link_directories(${PROJECT_NAME} PRIVATE ${LIBUSB_LIBRARY_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBUSB_LIBRARIES})
elseif(APPLE)
    target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/libusb-1.0.26-binaries/macos_11.6/include)
    target_link_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/libusb-1.0.26-binaries/macos_11.6/lib)
    target_link_libraries(${PROJECT_NAME} PRIVATE libusb-1.0)
else()
    message(FATAL_ERROR "Unsupported platform!")
endif()
