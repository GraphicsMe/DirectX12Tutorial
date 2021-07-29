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
	include/CubeBuffer.h
	include/SkyBox.h
	include/CubeMapCross.h
	include/ImguiManager.h
	include/GenerateMips.h
	include/TemporalEffects.h
	include/BufferManager.h
	include/MotionBlur.h
	include/PostProcessing.h
	include/DepthOfField.h
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
	src/CubeBuffer.cpp
	src/SkyBox.cpp
	src/CubeMapCross.cpp
	src/ImguiManager.cpp
	src/GenerateMips.cpp
	src/TemporalEffects.cpp
	src/BufferManager.cpp
	src/MotionBlur.cpp
	src/PostProcessing.cpp
	src/DepthOfField.cpp
)

set( IMGUI_HEADERS
    include/imgui/imconfig.h
    include/imgui/imgui.h
    include/imgui/imgui_impl_win32.h
    include/imgui/imgui_internal.h
    include/imgui/imstb_rectpack.h
    include/imgui/imstb_textedit.h
    include/imgui/imstb_truetype.h
)


set( IMGUI_SOURCE
    src/imgui/imgui.cpp
    src/imgui/imgui_demo.cpp
    src/imgui/imgui_draw.cpp
    src/imgui/imgui_impl_win32.cpp
    src/imgui/imgui_widgets.cpp
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
	../Resources/shaders/EnvironmentShaders.hlsl
	../Resources/shaders/ImGUI.hlsl
	../Resources/shaders/ShaderUtils.hlsl
	../Resources/shaders/GenerateMips.hlsl
	../Resources/shaders/PBR.hlsl
	../Resources/shaders/TemporalBlendCS.hlsl
	../Resources/shaders/ResolveTAACS.hlsl
	../Resources/shaders/CameraVelocityCS.hlsl
	../Resources/shaders/PixelPacking_Velocity.hlsli
	../Resources/shaders/DepthOfField_CoC.hlsl
	../Resources/shaders/DepthOfField_Prefilter.hlsl
	../Resources/shaders/DepthOfField_BokehFilter.hlsl
	../Resources/shaders/DepthOfField_Postfilter.hlsl
	../Resources/shaders/DepthOfField_Combine.hlsl
	../Resources/shaders/DiskKernels.hlsl
	../Resources/shaders/SSR.hlsl
)

source_group("Shaders" FILES ${SHADER_FILES})
source_group("Header Files\\ImGUI" FILES ${IMGUI_HEADERS} )
source_group("Source Files\\ImGUI" FILES ${IMGUI_SOURCE} )

add_library(DirectX12Lib
	${HEADER_FILES}
	${SOURCES}
	${SHADER_FILES}
	${IMGUI_HEADERS}
	${IMGUI_SOURCE}
)

target_include_directories(DirectX12Lib PUBLIC include include/imgui)

target_link_libraries(DirectX12Lib LINK_PUBLIC DirectXTex)

set_source_files_properties(${SHADER_FILES} PROPERTIES VS_TOOL_OVERRIDE "None")