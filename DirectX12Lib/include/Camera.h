#pragma once

#include <d3dcompiler.h>
#include "MathLib.h"

class FCamera
{
public:
	FCamera();
	FCamera(const Vector3f& CamPosition, const Vector3f& LookAtPosition, const Vector3f& UpDirection);

	void Update(float DeltaTime);

	Vector4f GetPosition() const;
	const FMatrix GetViewMatrix() const;
	void MoveForward(float Value);
	void MoveRight(float Value);
	void MoveUp(float Value);
	void Orbit(float Yaw, float Pitch);
	void Rotate(float Yaw, float Pitch);
	Vector3f GetUp() const { return Up; }
	Vector3f GetFocus() const { return Position + Forward * CameraLength; }

	// Projection
	void SetVerticalFov(float VerticalFov);
	void SetAspectRatio(float AspectHByW);
	void SetNearFar(float NearZ, float FarZ);
	void SetPerspectiveParams(float VerticalFov, float AspectHByW, float NearZ, float FarZ);
	const FMatrix GetProjectionMatrix() const;

	void SetMouseMoveSpeed(float Speed) { m_MoveSpeed = Speed; }
	void SetMouseZoomSpeed(float Speed) { m_ZoomSpeed = Speed; }
	void SetMouseRotateSpeed(float Speed) { m_RotateSpeed = Speed; }

private:
	void UpdateViewMatrix();
	void UpdateProjMatrix();
	void ProcessMouseMove(float DeltaTime);

private:
	FMatrix m_ViewMat, m_ProjMat;
	float CameraLength;
	Vector3f Right, Up, Forward, Position;

	float m_VerticalFov, m_AspectHByW;
	float m_NearZ, m_FarZ;

	float m_MoveSpeed = 5e-3f;
	float m_RotateSpeed = 1e-3f;
	float m_ZoomSpeed = 1e-2f;
};