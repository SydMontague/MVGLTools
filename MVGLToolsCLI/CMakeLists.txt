# Add source to this project's executable.
add_executable(MVGLToolsCLI)

target_sources(MVGLToolsCLI PRIVATE MVGLTools.cpp)
target_compile_features(MVGLToolsCLI PUBLIC cxx_std_23)
target_link_libraries(MVGLToolsCLI PRIVATE MVGLTools Boost::program_options)

# Install
install(TARGETS MVGLToolsCLI RUNTIME DESTINATION .)
install(DIRECTORY ../structures/ DESTINATION structures/)
install(FILES ../README.md ../LICENSE ../THIRD-PARTY-NOTICE DESTINATION .)

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_VERSION "v${PROJECT_VERSION}")
include(CPack)
