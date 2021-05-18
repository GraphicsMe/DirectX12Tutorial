#include "Light.h"

static FMatrix ProjToTexture(
	0.5f, 0.f, 0.f, 0.f,
	0.f, -0.5f, 0.f, 0.f,
	0.f, 0.f, 1.f, 0.f,
	0.5f, 0.5f, 0.f, 1.f);

FLight::FLight()
{
	Color = Vector3f(1.f);
}

FDirectionalLight::FDirectionalLight()
{
	SetDirection(Vector3f(1.f));
	SetIntensity(1.f);
	UpdateShadowBound(FBoundingBox(Vector3f(-1000.f), Vector3f(1000.f)));
}

void FDirectionalLight::SetDirection(const Vector3f& Direction)
{
	m_Direction = Direction.Normalize();
	m_ViewMat = FMatrix::MatrixLookAtLH(Vector3f(0.f), m_Direction, Vector3f(0.f, 1.f, 0.f));
	UpdateShadowMatrix();
}

void FDirectionalLight::SetIntensity(float Intensity)
{
	m_Intensity = Intensity;
}

const FMatrix& FDirectionalLight::GetViewMatrix() const
{
	return m_ViewMat;
}

const FMatrix& FDirectionalLight::GetProjectMatrix()
{
	return m_ProjMat;
}

const FMatrix& FDirectionalLight::GetLightToShadowMatrix()
{
	return m_ShadowMatrix;
}

void FDirectionalLight::UpdateShadowBound(const FBoundingBox& WorldBound)
{
	Vector3f WorldCenter = (WorldBound.BoundMax + WorldBound.BoundMin) * 0.5f;
	Vector3f ViewCenter = GetViewMatrix().TransformPosition(WorldCenter);
	float SphereRadius = (WorldBound.BoundMax - WorldBound.BoundMin).Length() * 0.5f;
	m_ProjMat = FMatrix::MatrixOrthoLH(2 * SphereRadius, 2 * SphereRadius, ViewCenter.z - SphereRadius, ViewCenter.z + SphereRadius);
	UpdateShadowMatrix();
}

void FDirectionalLight::UpdateShadowMatrix()
{
	m_ShadowMatrix = m_ViewMat * m_ProjMat * ProjToTexture;
}
