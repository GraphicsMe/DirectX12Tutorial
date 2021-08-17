#pragma once

#include <d3d12.h>
#include "AGS/amd_ags.h"

class FComputeContext;

// For adding markers in RGP
class UserMarker
{
public:
	UserMarker(class FCommandContext& context, const char* name);
	~UserMarker();
	static void SetAgsContext(AGSContext* agsContext) {
		m_agsContext = agsContext;
	}

private:

	static AGSContext* m_agsContext;
	ID3D12GraphicsCommandList* m_commandBuffer = nullptr;
};