set( TARGET_NAME FitLTCMatrix )

set( HEADER_FILES
    brdf.h
    brdf_ggx.h
	brdf_beckmann.h
	brdf_disneyDiffuse.h
	LTC.h
	float_to_half.h
	dds.h
	export.h
	nelder_mead.h
)

set( SRC_FILES
	fitLTC.cpp
	dds.cpp
)

add_executable( ${TARGET_NAME}	
    ${HEADER_FILES} 
    ${SRC_FILES}
)

target_include_directories( ${TARGET_NAME}
    PRIVATE ../../ThirdParty/
)

