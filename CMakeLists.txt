cmake_minimum_required(VERSION 3.10)

project(DirectX12Tutorial)

if(WIN32)
	add_compile_definitions(_UNICODE UNICODE)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/debug)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/release)

# Use solution folders to organize projects
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(DirectX12Lib)
# add_subdirectory(Supplement)
add_subdirectory(ThirdParty)

add_executable(Tutorial01 Tutorial01/tutorial1.cpp)
target_link_libraries(Tutorial01 LINK_PUBLIC DirectX12Lib)
set_target_properties(Tutorial01 PROPERTIES
	PROJECT_LABEL "Tutorial01 - Empty Project")

add_executable(Tutorial02 Tutorial02/tutorial2.cpp)
target_link_libraries(Tutorial02 LINK_PUBLIC DirectX12Lib)
set_target_properties(Tutorial02 PROPERTIES
	PROJECT_LABEL "Tutorial02 - Triangle")

add_executable(Tutorial03 Tutorial03/tutorial3.cpp)
target_link_libraries(Tutorial03 LINK_PUBLIC DirectX12Lib)
set_target_properties(Tutorial03 PROPERTIES
	PROJECT_LABEL "Tutorial03 - Cube")

add_executable(Tutorial04 Tutorial04/tutorial4.cpp)
target_link_libraries(Tutorial04 LINK_PUBLIC DirectX12Lib)
set_target_properties(Tutorial04 PROPERTIES
	PROJECT_LABEL "Tutorial04 - Texture")

add_executable(Tutorial05 Tutorial05/tutorial5.cpp)
target_link_libraries(Tutorial05 LINK_PUBLIC DirectX12Lib)
set_target_properties(Tutorial05 PROPERTIES
	PROJECT_LABEL "Tutorial05 - Multiple vertex stream")

add_executable(Tutorial06 Tutorial06/tutorial6.cpp)
target_link_libraries(Tutorial06 LINK_PUBLIC DirectX12Lib)
set_target_properties(Tutorial06 PROPERTIES
	PROJECT_LABEL "Tutorial06 - Mesh Loader")

add_executable(Tutorial07 Tutorial07/tutorial7.cpp)
target_link_libraries(Tutorial07 LINK_PUBLIC DirectX12Lib)
set_target_properties(Tutorial07 PROPERTIES
	PROJECT_LABEL "Tutorial07 - Shadow Mapping")

add_executable(Tutorial08 Tutorial08/tutorial8.cpp)
target_link_libraries(Tutorial08 LINK_PUBLIC DirectX12Lib)
set_target_properties(Tutorial08 PROPERTIES
	PROJECT_LABEL "Tutorial08 - Atmospheric Scattering")

add_executable(Tutorial09 Tutorial09/tutorial9.cpp)
target_link_libraries(Tutorial09 LINK_PUBLIC DirectX12Lib)
set_target_properties(Tutorial09 PROPERTIES
	PROJECT_LABEL "Tutorial09 - IBL Maps Generator")
	
add_executable(Tutorial10 Tutorial10/tutoria10.cpp)
target_link_libraries(Tutorial10 LINK_PUBLIC DirectX12Lib)
set_target_properties(Tutorial10 PROPERTIES
	PROJECT_LABEL "Tutorial10 - IBL Precompute")
	
add_executable(TutorialSkin TutorialSkin/TutorialSkin.cpp)
target_link_libraries(TutorialSkin LINK_PUBLIC DirectX12Lib)
set_target_properties(TutorialSkin PROPERTIES
	PROJECT_LABEL "TutorialSkin - Skin Rendering")
	
add_executable(TutorialLTC TutorialLTC/TutorialLTC.cpp)
target_link_libraries(TutorialLTC LINK_PUBLIC DirectX12Lib)
set_target_properties(TutorialLTC PROPERTIES
	PROJECT_LABEL "TutorialLTC - PolygonalLightShadingWithLinearlyTransformedCosines")
	
add_executable(TutorialAO TutorialAO/TutorialAO.cpp)
target_link_libraries(TutorialAO LINK_PUBLIC DirectX12Lib)
set_target_properties(TutorialAO PROPERTIES
	PROJECT_LABEL "TutorialAO - Ambient Occlusion")

set_target_properties(
	Tutorial01
	Tutorial02
	Tutorial03
	Tutorial04
	Tutorial05
	Tutorial06
	Tutorial07
	Tutorial08
	Tutorial09
	Tutorial10
	TutorialSkin
	TutorialLTC
	TutorialAO
	PROPERTIES
		FOLDER Tutorials
)