#pragma once

#include "MathLib.h"

class FLight
{
public:
	FLight();

protected:
	Vector3f Position;
	Vector3f Color;
};


class FDirectionalLight : public FLight
{
public:
	FDirectionalLight();
	void SetDirection(const Vector3f& Direction);
	const Vector3f& GetDirection() const { return m_Direction; }
	const FMatrix& GetViewMatrix() const;
	const FMatrix& GetProjectMatrix();
	const FMatrix& GetLightToShadowMatrix();
	
	void UpdateShadowBound(const FBoundingBox& WorldBound);

private:
	void UpdateShadowMatrix();


protected:
	Vector3f m_Direction;
	FMatrix m_ViewMat, m_ProjMat, m_ShadowMatrix;
};