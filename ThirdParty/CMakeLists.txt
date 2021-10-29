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