set(HEADER_FILES
	include/ApplicationWin32.h
	include/Camera.h
	include/ColorBuffer.h
	include/CommandContext.h
	include/CommandListManager.h
	include/CommandQueue.h
	include/Common.h
	include/D3D12Resource.h
	include/D3D12RHI.h
	include/d3dx12.h
	include/DepthBuffer.h
	include/DescriptorAllocator.h
	include/DynamicDescriptorHeap.h
	include/Game.h
	include/GpuBuffer.h
	include/LinearAllocator.h
	include/MathLib.h
	include/MeshData.h
	include/ObjLoader.h
	include/PipelineState.h
	include/PixelBuffer.h
	include/RenderWindow.h
	include/RootSignature.h
	include/SamplerManager.h
	include/Texture.h
	include/Timer.h
	include/WindowWin32.h
	include/Model.h
	include/ShadowBuffer.h
	include/Light.h
	include/GameInput.h
)

set(SOURCES
	src/ApplicationWin32.cpp
	src/Camera.cpp
	src/ColorBuffer.cpp
	src/CommandContext.cpp
	src/CommandListManager.cpp
	src/CommandQueue.cpp
	src/Common.cpp
	src/D3D12RHI.cpp
	src/DepthBuffer.cpp
	src/DescriptorAllocator.cpp
	src/DynamicDescriptorHeap.cpp
	src/Game.cpp
	src/GpuBuffer.cpp
	src/LinearAllocator.cpp
	src/MathLib.cpp
	src/MeshData.cpp
	src/ObjLoader.cpp
	src/PipelineState.cpp
	src/PixelBuffer.cpp
	src/RenderWindow.cpp
	src/RootSignature.cpp
	src/Texture.cpp
	src/Timer.cpp
	src/WindowWin32.cpp
	src/Model.cpp
	src/ShadowBuffer.cpp
	src/Light.cpp
	src/GameInput.cpp
)

set(SHADER_FILES
	../Resources/shaders/AtmosphericScatteringCS.hlsl
	../Resources/shaders/AtmosphericScatteringPS.hlsl
	../Resources/shaders/AtmosphericScatteringCommon.hlsl
	../Resources/shaders/PostProcess.hlsl
	../Resources/shaders/BlurCS.hlsl
	../Resources/shaders/BlurHorizontalCS.hlsl
	../Resources/shaders/BlurVerticalCS.hlsl
	../Resources/shaders/Shadow.hlsl
	../Resources/shaders/ShadowPCF.hlsl
	../Resources/shaders/ShadowVSM.hlsl
	../Resources/shaders/VSMConvertCS.hlsl
	../Resources/shaders/simple.hlsl
	../Resources/shaders/triangle.frag
	../Resources/shaders/triangle.vert
	../Resources/shaders/triangle.hlsl
)
source_group("Shaders" FILES ${SHADER_FILES})

add_library(DirectX12Lib
	${HEADER_FILES}
	${SOURCES}
	${SHADER_FILES}
)

target_include_directories(DirectX12Lib PUBLIC include)

target_link_libraries(DirectX12Lib LINK_PUBLIC DirectXTex)

set_source_files_properties(${SHADER_FILES} PROPERTIES VS_TOOL_OVERRIDE "None")