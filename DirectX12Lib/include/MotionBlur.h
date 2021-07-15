#pragma once
#include <stdint.h>
#include "ColorBuffer.h"
#include "MathLib.h"

class FCommandContext;
class FComputeContext;
class FCamera;

namespace MotionBlur
{
	extern FColorBuffer	g_VelocityBuffer;

	void Initialize(void);

	void ClearVelocityBuffer(FCommandContext& Context);

	void GenerateCameraVelocityBuffer(FCommandContext& Context, const FCamera& camera);

	void GenerateCameraVelocityBuffer(FCommandContext& BaseContext, const FMatrix& reprojectionMatrix, float nearClip, float farClip);

	void Destroy(void);
}