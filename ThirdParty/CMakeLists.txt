add_subdirectory(DirectXTex)
add_subdirectory(AGS)

set_target_properties(
	DirectXTex
	texassemble
	texconv
	texdiag
	PROPERTIES
		FOLDER ThirdParty
)