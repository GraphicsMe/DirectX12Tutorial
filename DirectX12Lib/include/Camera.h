#pragma once

#include <d3dcompiler.h>
#include "MathLib.h"

class FCamera
{
public:
	FCamera(const Vector3f& CamPosition, const Vector3f& LookAtPosition, const Vector3f& UpDirection);
	Vector4f GetPosition() const;
	FMatrix GetViewMatrix() const;
	void MoveForward(float Value);
	void MoveRight(float Value);
	void MoveUp(float Value);
	void Orbit(float Yaw, float Pitch);
	void Rotate(float Yaw, float Pitch);
	Vector3f GetUp() const { return Up; }
	Vector3f GetFocus() const { return Position + Forward * CameraLength; }
private:
	void UpdateViewMatrix();

private:
	FMatrix ViewMat;
	float CameraLength;
	Vector3f Right, Up, Forward, Position;
};