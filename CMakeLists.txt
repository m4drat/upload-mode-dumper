cmake_minimum_required(VERSION 3.11)

project(upload_dumper C)
include(ExternalProject)

ExternalProject_Add(
    libusb
)