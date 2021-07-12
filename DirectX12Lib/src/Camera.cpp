#include "Camera.h"
#include "GameInput.h"
#include <stdio.h>

FCamera::FCamera()
{
	SetPerspectiveParams(MATH_PI / 4.f, 1.f, 0.01f, 1000.f);
}

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

void FCamera::Update(float DeltaTime)
{
	float MoveDelta = float(m_MoveSpeed * DeltaTime);
	if (GameInput::IsKeyDown('W'))
	{
		this->MoveForward(MoveDelta);
	}
	if (GameInput::IsKeyDown('S'))
	{
		this->MoveForward(-MoveDelta);
	}
	if (GameInput::IsKeyDown('A'))
	{
		this->MoveRight(-MoveDelta);
	}
	if (GameInput::IsKeyDown('D'))
	{
		this->MoveRight(MoveDelta);
	}
	if (GameInput::IsKeyDown('Q'))
	{
		this->MoveUp(MoveDelta);
	}
	if (GameInput::IsKeyDown('E'))
	{
		this->MoveUp(-MoveDelta);
	}

	ProcessMouseMove(DeltaTime);

	// update all
	UpdateAllMatrix();
}

Vector4f FCamera::GetPosition() const
{
	return Vector4f(Position, 1.f);
}

void FCamera::UpdateAllMatrix()
{
	// Record the Previous frame ViewProjMatrix
	m_PreviousViewProjMatrix = m_ViewMat * m_ProjMat;

	UpdateViewMatrix();

	UpdateProjMatrix();

	m_ViewProjMatrix = m_ViewMat * m_ProjMat;

	// Update ReprojectMatrix
	m_ReprojectMatrix = m_ViewProjMatrix.Inverse() * m_PreviousViewProjMatrix;
}

void FCamera::UpdateViewMatrix()
{
	// Update ViewMatrix 
	Vector3f Focus = Position + Forward * CameraLength;
	m_ViewMat = FMatrix::MatrixLookAtLH(Position, Focus, Up);
}

const FMatrix FCamera::GetViewMatrix() const
{
	return m_ViewMat;
}

void FCamera::MoveForward(float Value)
{
	if (CameraLength > Value)
	{
		CameraLength -= Value;
		Vector3f Delta = Forward * Value;
		Position += Delta;
		//UpdateViewMatrix();
	}
}

void FCamera::MoveRight(float Value)
{
	Vector3f Delta = Right * Value;
	Position += Delta;
	//UpdateViewMatrix();
}

void FCamera::MoveUp(float Value)
{
	Vector3f Delta = Up * Value;
	Position += Delta;
	//UpdateViewMatrix();
}

void FCamera::Orbit(float Yaw, float Pitch)
{
	Vector3f Focus = Position + Forward * CameraLength;
	float D0 = -Right.Dot(Focus);
	float D1 = -Up.Dot(Focus);
	float D2 = -Forward.Dot(Focus);

	// 1. translate camera to focus
	FMatrix CameraTranslate = FMatrix::TranslateMatrix(Vector3f(0.f, 0.f, -CameraLength));
	FMatrix Result = CameraTranslate;

	Vector3f Forward1 = Vector3f(Forward.x, 0.f, Forward.z).Normalize();
	Vector3f Up1 = Cross(Forward1, Right);

	// 2. pitch camera to horizontal
	float ToHorizontalPitch = acos(Forward.Dot(Forward1));
	ToHorizontalPitch = Forward.y < 0.f ? ToHorizontalPitch : -ToHorizontalPitch;
	FMatrix ToHorrizontal = FMatrix::MatrixRotationRollPitchYaw(0.f, ToHorizontalPitch, 0.f);
	Result *= ToHorrizontal;

	// 3. rotate camera in horizontal
	FMatrix Rot = FMatrix::MatrixRotationRollPitchYaw(0.f, Pitch, Yaw);
	Result *= Rot;

	// 4. translate to world space
	FMatrix OrbitToWorld = FMatrix(Right, Up1, Forward1, Vector3f(0.f));
	Result *= OrbitToWorld;

	Right = Result.r0;
	Up = Result.r1;
	Forward = Result.r2;
	Position = Focus - Forward * CameraLength; // Focus will not change
	//UpdateViewMatrix();
}

void FCamera::Rotate(float Yaw, float Pitch)
{
	FMatrix Rot = FMatrix::MatrixRotationRollPitchYaw(0.f, Pitch, Yaw);
	FMatrix Trans(Right, Up, Forward, Vector3f(0.f));
	FMatrix Result = Trans * Rot;

	Right = Result.r0;
	Up = Result.r1;
	Forward = Result.r2;
	//UpdateViewMatrix();
}

void FCamera::SetVerticalFov(float VerticalFov)
{
	m_VerticalFov = VerticalFov;
	UpdateProjMatrix();
}

void FCamera::SetAspectRatio(float AspectHByW)
{
	m_AspectHByW = AspectHByW;
	UpdateProjMatrix();
}

void FCamera::SetNearFar(float NearZ, float FarZ)
{
	m_NearZ = NearZ;
	m_FarZ = FarZ;
	UpdateProjMatrix();
}

void FCamera::SetPerspectiveParams(float VerticalFov, float AspectHByW, float NearZ, float FarZ)
{
	m_VerticalFov = VerticalFov;
	m_AspectHByW = AspectHByW;
	m_NearZ = NearZ;
	m_FarZ = FarZ;
	UpdateProjMatrix();
}

void FCamera::UpdateProjMatrix()
{
	m_ProjMat = FMatrix::MatrixPerspectiveFovLH(m_VerticalFov, m_AspectHByW, m_NearZ, m_FarZ);
}

const FMatrix FCamera::GetProjectionMatrix() const
{
	return m_ProjMat;
}

void FCamera::ProcessMouseMove(float DeltaTime)
{
	bool LeftMouseButtonDown = GameInput::IsKeyDown(VK_LBUTTON);
	bool RightMouseButtonDown = GameInput::IsKeyDown(VK_RBUTTON);
	bool AltKeyDown = GameInput::IsKeyDown(VK_MENU);
	Vector2i MouseDelta = GameInput::GetMoveDelta();
	float RotateDelta = float(m_RotateSpeed * DeltaTime);
	if (MouseDelta.x != 0 || MouseDelta.y != 0)
	{
		if (RightMouseButtonDown)
		{
			// rotate camera
			this->Rotate(MouseDelta.x * RotateDelta, MouseDelta.y * RotateDelta);
		}
		else if (LeftMouseButtonDown && AltKeyDown)
		{
			// orbit camera around focus
			this->Orbit(MouseDelta.x * RotateDelta, MouseDelta.y * RotateDelta);
		}
		else if (LeftMouseButtonDown)
		{
			if (abs(MouseDelta.x) >= abs(MouseDelta.y))
			{
				this->Rotate(MouseDelta.x * RotateDelta, 0.f);
			}
			else
			{
				this->MoveForward(float(-MouseDelta.y * m_MoveSpeed * DeltaTime));
			}
		}
		GameInput::MouseMoveProcessed();
	}

	float Zoom = GameInput::ConsumeMouseZoom();
	if (Zoom != 0.f)
	{
		this->MoveForward(Zoom * m_ZoomSpeed);
	}
}
