if(UNIX AND NOT APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath='$ORIGIN'")
endif()

# Add source to this project's executable.
add_executable (DSCSToolsCLI ${EXE_SOURCE_FILES})

target_sources(DSCSToolsCLI 
    PRIVATE
    DSCSTools.cpp
)

target_link_libraries(DSCSToolsCLI PRIVATE DSCSTools)

# Install into /target/ folder
install (TARGETS DSCSToolsCLI 
			RUNTIME DESTINATION ${CMAKE_BINARY_DIR}/target
		)

        
add_executable (DSTSToolsCLI ${EXE_SOURCE_FILES})

target_sources(DSTSToolsCLI 
    PRIVATE
    DSTSTools.cpp
)

target_link_libraries(DSTSToolsCLI PRIVATE DSCSTools)