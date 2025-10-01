# specify no SHARED/STATIC, so BUILD_SHARED_LIBS gets used
add_library (DSCSTools)

target_sources(DSCSTools
	PRIVATE
	MDB1.cpp
	EXPA.cpp
	SaveFile.cpp
	AFS2.cpp
	MDB1New.cpp
)

target_include_directories(DSCSTools 
	PUBLIC
	$<INSTALL_INTERFACE:include>    
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(DSCSTools PUBLIC doboz lz4 Boost::property_tree Boost::multiprecision Boost::crc Boost::regex Boost::asio)

# Install into /target/ folder
install (TARGETS DSCSTools 
			LIBRARY DESTINATION ${CMAKE_BINARY_DIR}/target
			RUNTIME DESTINATION ${CMAKE_BINARY_DIR}/target
			ARCHIVE DESTINATION ${CMAKE_BINARY_DIR}/target
		)
install (DIRECTORY structures/ DESTINATION ${CMAKE_BINARY_DIR}/target/structures)
install (DIRECTORY include/ DESTINATION ${CMAKE_BINARY_DIR}/target/include)

# Ugly hack to make sure install works
file(TOUCH ${CMAKE_BINARY_DIR}/_deps/boost-build/cmake_install.cmake)