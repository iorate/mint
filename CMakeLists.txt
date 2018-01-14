
# mint
#
# Copyright iorate 2018.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 3.0.2)

project(mint)

if(CMAKE_CXX_COMPILER_ID MATCHES MSVC)
  add_compile_options(/std:c++17 /WX /W4)
  add_definitions(/D_UNICODE /DUNICODE)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup")
else(CMAKE_CXX_COMPILER_ID MATCHES GNU)
  add_compile_options(-std=c++17 -Werror -Wall -Wextra -Wpedantic)
  add_compile_options(-municode)
  add_compile_options(-mwindows)
endif()

include_directories(${PROJECT_SOURCE_DIR}/nonsugar)

add_executable(m2 main.cpp)
