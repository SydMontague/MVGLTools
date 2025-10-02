cmake_minimum_required (VERSION 3.28)

project ("DSCSTools" 
            VERSION 2.0.0 
            DESCRIPTION "Tools for Digimon Story Cyber Sleuth"
            HOMEPAGE_URL "https://github.com/SydMontague/DSCSTools")

set(CMAKE_CXX_STANDARD 23)
set(CXX_SCAN_FOR_MODULES OFF)

include(cmake/CPM.cmake)

if (MSVC)
	add_definitions("-D_CRT_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0A00") # Windows 10
else()
#	set(CMAKE_CXX_FLAGS -pthread)
endif()

option(BUILD_SHARED_LIBS "Build Using Shared Libs" OFF)

# --- Libraries ---
# doboz
add_subdirectory("libs/doboz")
# lz4
add_subdirectory("libs/lz4")

# boost
if(WIN32)
    set(Boost_USE_STATIC_LIBS ON) 
endif()

CPMAddPackage(
  NAME Boost
  VERSION 1.89.0
  GITHUB_REPOSITORY "boostorg/boost"
  GIT_TAG "boost-1.89.0"
)

# Include sub-projects.
add_subdirectory ("DSCSTools")
add_subdirectory ("DSCSToolsCLI")

install (FILES LICENSE THIRD-PARTY-NOTICE DESTINATION ${CMAKE_BINARY_DIR}/target/license)