#pragma once
#include <stdint.h>
#include "MathLib.h"

class FColorBuffer;
class FCommandContext;
class FComputeContext;
class FCamera;

namespace TemporalEffects
{
	extern bool	g_EnableTAA;

	void Initialize(void);

	void Destroy(void);

	// Call once per frame to increment the internal frame counter and, in the case of TAA, choosing the next
	// jittered sample position.
	void Update(void);

	// Returns whether the frame is odd or even
	uint32_t GetFrameIndexMod2(void);

	void GetJitterOffset(Vector4f& TemporalAAJitter, float Width, float Height);

	// return TAA ProjectionMatrix
	FMatrix HackAddTemporalAAProjectionJitter(const FCamera& Camera, float Width, float Height, bool PrevFrame = false);

	// Should be called after update() in one frame
	FColorBuffer& GetHistoryBuffer();

	void ClearHistory(FCommandContext& Context);

	void ResolveImage(FCommandContext& GraphicsContext, FColorBuffer& SceneColor);

	void ApplyTemporalAA(FComputeContext& Context, FColorBuffer& SceneColor);
	void SharpenImage(FComputeContext& Context, FColorBuffer& SceneColor, FColorBuffer& TemporalColor);

}