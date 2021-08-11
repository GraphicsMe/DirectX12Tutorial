add_subdirectory(DirectXTex)
add_subdirectory(GLM)

set_target_properties(
	DirectXTex
	texassemble
	texconv
	texdiag
	glm_dummy
	PROPERTIES
		FOLDER ThirdParty
)