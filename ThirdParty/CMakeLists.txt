set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

add_subdirectory(DirectXTex)
add_subdirectory(AGS)

SET(TINYGLTF_BUILD_LOADER_EXAMPLE OFF)
SET(TINYGLTF_HEADER_ONLY ON)
add_subdirectory(tinygltf)

set_target_properties(
	DirectXTex
	texassemble
	texconv
	texdiag
	PROPERTIES
		FOLDER ThirdParty
)