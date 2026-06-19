cmake_minimum_required(VERSION 3.28)

project("MVGLTools"
  VERSION 2.1.0
  DESCRIPTION "Tools for game using the Media.Vision engine (e.g. Digimon Story)"
  HOMEPAGE_URL "https://github.com/SydMontague/MVGLTools")

set(CMAKE_CXX_STANDARD 23)
set(CXX_SCAN_FOR_MODULES OFF)

include(cmake/CPM.cmake)

if(MSVC)
  add_definitions("-D_CRT_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0A00") # Windows 10
endif()

# --- Libraries ---
# doboz
add_subdirectory("libs/doboz" SYSTEM)
# lz4
add_subdirectory("libs/lz4" SYSTEM)
# csv-parser
add_subdirectory("libs/csv-parser" SYSTEM)

# boost
if(WIN32)
  set(Boost_USE_STATIC_LIBS ON)
endif()

CPMAddPackage(
  NAME Boost
  VERSION 1.89.0
  GITHUB_REPOSITORY "boostorg/boost"
  GIT_TAG "boost-1.89.0"
  SYSTEM YES
)

# Include sub-projects.
add_subdirectory("MVGLTools")
add_subdirectory("MVGLToolsCLI")
