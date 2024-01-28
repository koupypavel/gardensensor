# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(PROJECT_VER "1.0.0-28012024") 
set(PROJECT_VER_MAJOR "1") 
set(PROJECT_VER_MINOR "0") 
set(PROJECT_VER_PATCH "0") 

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(GardenSensor)

configure_file(version.h.in version.h)
