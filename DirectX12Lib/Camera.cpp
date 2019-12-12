#include "Camera.h"
#include <stdio.h>

FCamera::FCamera(const Vector3f& CamPosition, const Vector3f& LookAtPosition, const Vector3f& UpDirection)
{
	Position = CamPosition;

	Up = UpDirection.Normalize();
	Forward = LookAtPosition - CamPosition;
	CameraLength = Forward.Length();
	Forward = Forward.Normalize();
	Right = Cross(Up, Forward);
	Up = Cross(Forward, Right);

	UpdateViewMatrix();
}

Vector4f FCamera::GetPosition() const
{
	return Vector4f(Position, 1.f);
}

void FCamera::UpdateViewMatrix()
{
	Vector3f Focus = Position + Forward * CameraLength;
	ViewMat = FMatrix::MatrixLookAtLH(Position, Focus, Up);
}

FMatrix FCamera::GetViewMatrix() const
{
	return ViewMat;
}

void FCamera::MoveForward(float Value)
{
	Vector3f Delta = Forward * Value;
	Position += Delta;
	UpdateViewMatrix();
}

void FCamera::MoveRight(float Value)
{
	Vector3f Delta = Right * Value;
	Position += Delta;
	UpdateViewMatrix();
}

void FCamera::MoveUp(float Value)
{
	Vector3f Delta = Up * Value;
	Position += Delta;
	UpdateViewMatrix();
}

void FCamera::Orbit(float Yaw, float Pitch)
{
	printf("Orbit %f %f\n", Yaw, Pitch);
	Vector3f Focus = Position + Forward * CameraLength;
	float D0 = -Right.Dot(Focus);
	float D1 = -Up.Dot(Focus);
	float D2 = -Forward.Dot(Focus);

	// 1. to orbit space
	FMatrix ToOrbit(Vector4f(Right, D0), Vector4f(Up, D1), Vector4f(Forward, D2), Vector4f(0.f, 0.f, 0.f, 1.f));
    ToOrbit = ToOrbit.Transpose();

	// 3. translate camera in orbit space
	FMatrix CameraTranslate = FMatrix::TranslateMatrix(Vector3f(0.f, 0.f, -CameraLength));
	FMatrix Result = CameraTranslate;

	// 2. rotate in orbit space
	FMatrix Rot = FMatrix::MatrixRotationRollPitchYaw(0, Pitch, Yaw);
	Result *= Rot;

	// 4. translate to world space
	FMatrix OrbitToWorld = FMatrix(Right, Up, Forward, Focus);
	Result *= OrbitToWorld;

	Right = Result.r0;
	Up = Result.r1;
	Forward = Result.r2;
	Position = Result.r3;
	UpdateViewMatrix();
}

void FCamera::Rotate(float Yaw, float Pitch)
{
	printf("Rotate %f %f\n", Yaw, Pitch);
	// pitch, yaw, roll
	FMatrix Rot = FMatrix::MatrixRotationRollPitchYaw(0.f, Pitch, Yaw);
	FMatrix Trans(Right, Up, Forward, Vector3f(0.f));
	FMatrix Result = Rot * Trans;

	Right = Result.r0;
	Up = Result.r1;
	Forward = Result.r2;
	UpdateViewMatrix();
}
