#pragma once

#include <math.h>
#include <stdint.h>

const float MATH_PI = 3.141592654f;

template<typename T>
struct Vector4;

template<typename T>
struct Vector2
{
	union 
	{
		T data[2];
		struct { T x, y;};
	};
	
	T& operator [] (int i)
	{
		return data[i];
	}
	Vector2() : x(0), y(0) {}
	Vector2(T c) : x(c), y(c) {}
	Vector2(T x, T y) : x(x), y(y) {}
};

template<typename T>
struct Vector3
{
	union 
	{
		T data[3];
		struct { T x, y, z;};
	};
	T& operator [] (int i)
	{
		return data[i];
	}
	const T& operator [] (int i) const
	{
		return data[i];
	}
	Vector3() : x(0), y(0), z(0) {}
	Vector3(T c) : x(c), y(c), z(c) {}
	Vector3(T x, T y, T z) : x(x), y(y), z(z) {}
	Vector3(const Vector4<T>& v) : x(v.x), y(v.y), z(v.z) {}

	T Dot(const Vector3& rhs) const
	{
		return x * rhs.x + y * rhs.y + z * rhs.z;
	}

	Vector3& operator += (const Vector3& rhs)
	{
		x += rhs.x;
		y += rhs.y;
		z += rhs.z;
		return *this;
	}

	Vector3 operator + (const Vector3& rhs) const
	{
		Vector3 Res(*this);
		Res += rhs;
		return Res;
	}

	Vector3 operator * (const Vector3& rhs) const
	{
		return Vector3(x * rhs.x, y * rhs.y, z * rhs.z);
	}

	Vector3 operator - (const Vector3& rhs) const
	{
		return Vector3(x - rhs.x, y - rhs.y, z - rhs.z);
	}

	T Length() const
	{
		return (T)sqrt(x * x + y * y + z * z);
	}

	Vector3 Normalize() const
	{
		T length = Length();
		T inv = T(1) / length;
		return Vector3(x * inv, y * inv, z *inv);
	}
};

template<typename T, typename scalar>
Vector3<T> operator * (const Vector3<T>& lhs, scalar s)
{
	return Vector3<T>(lhs.x * s, lhs.y * s, lhs.z * s); 
}

template<typename T, typename scalar>
Vector3<T> operator * (scalar s, const Vector3<T>& lhs)
{
	return Vector3<T>(lhs.x * s, lhs.y * s, lhs.z * s); 
}

template<typename T>
Vector3<T> Cross(const Vector3<T>& lhs, const Vector3<T>& rhs)
{
	return Vector3<T>(lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.x - lhs.x * rhs.z, lhs.x * rhs.y - lhs.y * rhs.x);
}

template<typename T>
struct Vector4
{
	union 
	{
		T data[4];
		struct { T x, y, z, w;};
	};
	T& operator [] (int i)
	{
		return data[i];
	}
	const T& operator [] (int i) const
	{
		return data[i];
	}
	Vector4() : x(0), y(0), z(0), w(0) {}
	Vector4(T c) : x(c), y(c), z(c), w(c) {}
	Vector4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}
	Vector4(const Vector3<T>& v, T s = 0) : x(v.x), y(v.y), z(v.z), w(s) {}
	T Dot(const Vector4& rhs) const
	{
		return x * rhs.x + y * rhs.y + z * rhs.z + w * rhs.w;
	}
};



typedef Vector2<int> Vector2i;
typedef Vector2<float> Vector2f;
typedef Vector3<float> Vector3f;
typedef Vector4<float> Vector4f;



struct FMatrix
{
	union 
	{
		Vector4f row[4];
		struct { Vector4f r0, r1, r2, r3;};
	};
	FMatrix();

	FMatrix(const Vector3f& r0, const Vector3f& r1, const Vector3f& r2, const Vector3f& r3);

	FMatrix(const Vector4f& r0, const Vector4f& r1, const Vector4f& r2, const Vector4f& r3);
	FMatrix(float m00, float m01, float m02, float m03,
			float m10, float m11, float m12, float m13,
			float m20, float m21, float m22, float m23,
			float m30, float m31, float m32, float m33);

	Vector4f Column(int i) const;

	FMatrix operator * (const FMatrix& rhs);
	FMatrix& operator *= (const FMatrix& rhs);

	Vector3f TranslateVector(const Vector3f& vector);
	Vector3f TransformPosition(const Vector3f& position);
	FMatrix Transpose();

	static FMatrix TranslateMatrix(const Vector3f& T);
	static FMatrix ScaleMatrix(float s);
	static FMatrix RotateX(float v);
	static FMatrix RotateY(float v);
	static FMatrix RotateZ(float v);
	static FMatrix MatrixRotationRollPitchYaw(float Roll, float Pitch, float Yaw);
	static FMatrix MatrixLookAtLH(const Vector3f& EyePosition, const Vector3f& FocusPosition, const Vector3f& UpDirection);
	static FMatrix MatrixPerspectiveFovLH(float FovAngleY, float AspectHByW, float NearZ, float FarZ);
	static FMatrix MatrixOrthoLH(float Width, float Height, float NearZ, float FarZ);
};

//inline Vector4f operator*(const FMatrix& mat, const Vector4f& vec);

inline Vector4f operator*(const Vector4f& vec, const FMatrix& mat);

template <typename T>
T AlignUpWithMask(T Value, uint32_t Mask)
{
	return (T)(((uint32_t)Value + Mask) & (~Mask));
}

template <typename T>
T AlignUp(T Value, uint32_t Alignment)
{
	return AlignUpWithMask(Value, Alignment - 1);
}