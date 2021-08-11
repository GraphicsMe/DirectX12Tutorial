// fitLTC.cpp : Defines the entry point for the console application.
//
#include <glm/glm.hpp>
using namespace glm;

#include <algorithm>
#include <fstream>
#include <iomanip>

#include "LTC.h"
#include "brdf.h"
#include "brdf_ggx.h"
#include "brdf_beckmann.h"
#include "brdf_disneyDiffuse.h"

#include "nelder_mead.h"

#include "export.h"

// size of precomputed table (theta, alpha)
const int N = 64;	//64
// number of samples used to compute the error during fitting
const int Nsample = 32;	//32
// minimal roughness (avoid singularities)
const float MIN_ALPHA = 0.00001f;

const float pi = acosf(-1.0f);

// computes
// * the norm (albedo) of the BRDF
// * the average Schlick Fresnel value
// * the average direction of the BRDF
// * ����norm��fresnel��averageDir���ǳ���BRDF��������pdf��
void computeAvgTerms(const Brdf& brdf, const vec3& V, const float alpha,
	float& norm, float& fresnel, vec3& averageDir)
{
	norm = 0.0f;
	fresnel = 0.0f;
	averageDir = vec3(0, 0, 0);

	for (int j = 0; j < Nsample; ++j)	//������0~1�������Χ��ȥ����L������brdf��ֵ��������Խ�࣬0~1��Χ�ڱ����ǵĵ�Խ�࣬�������L�Լ�brdf��ֵԽ��׼���о��������U1��U2��ֵҲ���ԣ�
		for (int i = 0; i < Nsample; ++i)
		{
			const float U1 = (i + 0.5f) / Nsample;
			const float U2 = (j + 0.5f) / Nsample;

			// sample
			// ������һ������
			const vec3 L = brdf.sample(V, alpha, U1, U2);

			// eval
			float pdf;
			float eval = brdf.eval(V, L, alpha, pdf);

			if (pdf > 0)
			{
				float weight = eval / pdf;

				// �������
				vec3 H = normalize(V + L);

				// accumulate
				norm += weight;	//norm�洢��������Fresnel��Ӳ����е�nD
				fresnel += weight * pow(1.0f - glm::max(dot(V, H), 0.0f), 5.0f);		//fresnel�洢��������Fresnel��Ӳ����е�fD

				// ��Ȩ��������
				averageDir += weight * L;
			}
		}

	norm /= (float)(Nsample * Nsample);
	fresnel /= (float)(Nsample * Nsample);

	// clear y component, which should be zero with isotropic BRDFs
	// ����ͬ�ԣ������Ƿ�λ��
	averageDir.y = 0.0f;

	// ��һ��
	averageDir = normalize(averageDir);
}

// compute the error between the BRDF and the LTC
// using Multiple Importance Sampling
// �ֱ��BRDF��LTC�������Ҫ�Բ������ɳ�L��Ȼ��������ֲַ���ֵ�Ĳ��죬���������ܶ�
float computeError(const LTC& ltc, const Brdf& brdf, const vec3& V, const float alpha)
{
	double error = 0.0;

	for (int j = 0; j < Nsample; ++j)
		for (int i = 0; i < Nsample; ++i)
		{
			const float U1 = (i + 0.5f) / Nsample;
			const float U2 = (j + 0.5f) / Nsample;

			// importance sample LTC
			{
				// sample
				const vec3 L = ltc.sample(U1, U2);

				float pdf_brdf;
				float eval_brdf = brdf.eval(V, L, alpha, pdf_brdf);
				float eval_ltc = ltc.eval(L);
				float pdf_ltc = eval_ltc / ltc.magnitude;

				// error with MIS weight
				double error_ = fabsf(eval_brdf - eval_ltc);
				error_ = error_ * error_ * error_;
				error += error_ / (pdf_ltc + pdf_brdf);
			}

			// importance sample BRDF
			{
				// sample
				const vec3 L = brdf.sample(V, alpha, U1, U2);

				float pdf_brdf;
				float eval_brdf = brdf.eval(V, L, alpha, pdf_brdf);
				float eval_ltc = ltc.eval(L);
				float pdf_ltc = eval_ltc / ltc.magnitude;

				// error with MIS weight
				double error_ = fabsf(eval_brdf - eval_ltc);
				error_ = error_ * error_ * error_;
				error += error_ / (pdf_ltc + pdf_brdf);
			}
		}

	return (float)error / (float)(Nsample * Nsample);
}

struct FitLTC
{
	FitLTC(LTC& ltc_, const Brdf& brdf, bool isotropic_, const vec3& V_, float alpha_) :
		ltc(ltc_), brdf(brdf), V(V_), alpha(alpha_), isotropic(isotropic_)
	{
	}

	void update(const float* params)
	{
		float m11 = std::max<float>(params[0], 1e-7f);
		float m22 = std::max<float>(params[1], 1e-7f);
		float m13 = params[2];

		if (isotropic)
		{
			ltc.m11 = m11;
			ltc.m22 = m11;
			ltc.m13 = 0.0f;
		}
		else
		{
			ltc.m11 = m11;
			ltc.m22 = m22;
			ltc.m13 = m13;
		}
		ltc.update();
	}

	float operator()(const float* params)
	{
		update(params);
		return computeError(ltc, brdf, V, alpha);
	}

	const Brdf& brdf;
	LTC& ltc;
	bool isotropic;

	const vec3& V;
	float alpha;
};

// fit brute force
// refine first guess by exploring parameter space
void fit(LTC& ltc, const Brdf& brdf, const vec3& V, const float alpha, const float epsilon = 0.05f, const bool isotropic = false)
{
	float startFit[3] = { ltc.m11, ltc.m22, ltc.m13 };
	float resultFit[3];

	FitLTC fitter(ltc, brdf, isotropic, V, alpha);

	// Find best-fit LTC lobe (scale, alphax, alphay)
	float error = NelderMead<3>(resultFit, startFit, epsilon, 1e-5f, 100, fitter);

	// Update LTC with best fitting values
	fitter.update(resultFit);
}

// fit data
// tab��洢����LTC����tabMagFresnel��洢����LTC��magnitude��fresnel
void fitTab(mat3* tab, vec2* tabMagFresnel, const int N, const Brdf& brdf)
{
	LTC ltc;

	//  ��һ��ѭ��ö�ٴֲڶȣ��ڶ���ѭ��ö�ٹ۲�������ƽ��ļнǴӶ��õ��뷨�ߵļн�theta
	for (int a = N - 1; a >= 0; --a)	//�ֲڶȴ�1�䵽0
		for (int t = 0; t <= N - 1; ++t)	//theta��0�䵽PI/2
		{
			// parameterised by sqrt(1 - cos(theta)), ��x = sqrt(1 - cos(theta))������cos(theta) = 1 - x^2
			float x = t / float(N - 1);
			float ct = 1.0f - x * x;
			float theta = std::min<float>(1.57f, acosf(ct));
			// y������0������Ϊ�����߿ռ�����ģ�y�ᴹֱV��N���ڵ�ƽ��
			const vec3 V = vec3(sinf(theta), 0, cosf(theta));

			// alpha = roughness^2
			float roughness = a / float(N - 1);
			// ��С�Ĵֲڶ�ȡ��0.00001f
			float alpha = std::max<float>(roughness * roughness, MIN_ALPHA);	//alpha��ƽ���ֲڶȣ�Ҳ����˵�ڼ���BRDF��LTC��ֵʱ�õ���ƽ���ֲڶ�

			cout << "a = " << a << "\t t = " << t << endl;
			cout << "alpha = " << alpha << "\t theta = " << theta << endl;
			cout << endl;

			// �õ�һ��ƽ������
			vec3 averageDir;
			computeAvgTerms(brdf, V, alpha, ltc.magnitude, ltc.fresnel, averageDir);

			bool isotropic;

			// 1.����Ҫ�²�һ��M����
			// ��ʼ��һ����Ϊ���ʵİ�����ֲ�����D0
			// ���theta==0����lobe�ǹ���Z�����ԳƵ�
			if (t == 0)
			{
				ltc.X = vec3(1, 0, 0);
				ltc.Y = vec3(0, 1, 0);
				ltc.Z = vec3(0, 0, 1);

				if (a == N - 1) // roughness = 1
				{
					ltc.m11 = 1.0f;
					ltc.m22 = 1.0f;
				}
				else // init with roughness of previous fit
				{
					ltc.m11 = tab[a + 1 + t * N][0][0];
					ltc.m22 = tab[a + 1 + t * N][1][1];
				}

				ltc.m13 = 0;
				ltc.update();	//��ʼ��LTC����

				isotropic = true;
			}
			// otherwise use previous configuration as first guess
			else
			{
				vec3 L = averageDir;
				vec3 T1(L.z, 0, -L.x);
				vec3 T2(0, 1, 0);
				ltc.X = T1;
				ltc.Y = T2;
				ltc.Z = L;

				ltc.update();	//��LTC������ת��������L����

				isotropic = false;
			}

			// 2. fit (explore parameter space and refine first guess)ͨ�������η���������M����ʹ���ֽ�����޽���BRDF
			float epsilon = 0.05f;
			fit(ltc, brdf, V, alpha, epsilon, isotropic);

			// copy data
			// ���д洢��ͬһ������theta�ڱ�
			// ---> roughness ��������洢��������Ǵֲڶ�Ϊ0��
			// |
			// ��
			// ��ֱ����parameterised by sqrt(1 - cos��)
			tab[a + t * N] = ltc.M;	

			tabMagFresnel[a + t * N][0] = ltc.magnitude;
			tabMagFresnel[a + t * N][1] = ltc.fresnel;

			// kill useless coefs in matrix��������ֻ����5��Ԫ��
			tab[a + t * N][0][1] = 0;
			tab[a + t * N][1][0] = 0;
			tab[a + t * N][2][1] = 0;
			tab[a + t * N][1][2] = 0;

			cout << tab[a + t * N][0][0] << "\t " << tab[a + t * N][1][0] << "\t " << tab[a + t * N][2][0] << endl;
			cout << tab[a + t * N][0][1] << "\t " << tab[a + t * N][1][1] << "\t " << tab[a + t * N][2][1] << endl;
			cout << tab[a + t * N][0][2] << "\t " << tab[a + t * N][1][2] << "\t " << tab[a + t * N][2][2] << endl;
			cout << endl;
		}
}

float sqr(float x)
{
	return x * x;
}

float G(float w, float s, float g)
{
	return -2.0f * sinf(w) * cosf(s) * cosf(g) + pi / 2.0f - g + sinf(g) * cosf(g);
}

float H(float w, float s, float g)
{
	float sinsSq = sqr(sin(s));
	float cosgSq = sqr(cos(g));

	return cosf(w) * (cosf(g) * sqrtf(sinsSq - cosgSq) + sinsSq * asinf(cosf(g) / sinf(s)));
}

float ihemi(float w, float s)
{
	float g = asinf(cosf(s) / sinf(w));
	float sinsSq = sqr(sinf(s));

	if (w >= 0.0f && w <= (pi / 2.0f - s))
		return pi * cosf(w) * sinsSq;

	if (w >= (pi / 2.0f - s) && w < pi / 2.0f)
		return pi * cosf(w) * sinsSq + G(w, s, g) - H(w, s, g);

	if (w >= pi / 2.0f && w < (pi / 2.0f + s))
		return G(w, s, g) + H(w, s, g);

	return 0.0f;
}

void genSphereTab(float* tabSphere, int N)
{
	for (int j = 0; j < N; ++j)
		for (int i = 0; i < N; ++i)
		{
			const float U1 = float(i) / (N - 1);
			const float U2 = float(j) / (N - 1);

			// z = cos(elevation angle)
			float z = 2.0f * U1 - 1.0f;

			// length of average dir., proportional to sin(sigma)^2
			float len = U2;

			float sigma = asinf(sqrtf(len));
			float omega = acosf(z);

			// compute projected (cosine-weighted) solid angle of spherical cap
			float value = 0.0f;

			if (sigma > 0.0f)
				value = ihemi(omega, sigma) / (pi * len);
			else
				value = std::max<float>(z, 0.0f);

			if (value != value)
				printf("nan!\n");

			tabSphere[i + j * N] = value;
		}
}

//��tab�д洢��LTC����������tabMagFresnel�д洢��LTC��Magnitude��Fresnel���Լ�tabSphere�д洢������Ǵ浽������
void packTab(
	vec4* tex1, vec4* tex2,
	const mat3* tab,
	const vec2* tabMagFresnel,
	const float* tabSphere,
	int N)
{
	for (int i = 0; i < N * N; ++i)
	{
		const mat3& m = tab[i];

		mat3 invM = inverse(m);

		// normalize by the middle element�����˾�����м�Ԫ�أ������ٴ�һ��Ԫ�أ���5�����4����
		invM /= invM[1][1];

		// store the variable terms
		tex1[i].x = invM[0][0];
		tex1[i].y = invM[0][2];
		tex1[i].z = invM[2][0];
		tex1[i].w = invM[2][2];

		tex2[i].x = tabMagFresnel[i][0];
		tex2[i].y = tabMagFresnel[i][1];
		tex2[i].z = 0.0f; // unused
		tex2[i].w = tabSphere[i];
	}
}

int main(int argc, char* argv[])
{
	// BRDF to fit
	BrdfGGX brdf;
	//BrdfBeckmann brdf;
	//BrdfDisneyDiffuse brdf;

	// allocate data
	mat3* tab = new mat3[N * N];
	vec2* tabMagFresnel = new vec2[N * N];
	float* tabSphere = new float[N * N];

	// fit
	fitTab(tab, tabMagFresnel, N, brdf);

	// projected solid angle of a spherical cap, clipped to the horizon
	genSphereTab(tabSphere, N);

	// pack tables (texture representation)
	vec4* tex1 = new vec4[N * N];
	vec4* tex2 = new vec4[N * N];
	packTab(tex1, tex2, tab, tabMagFresnel, tabSphere, N);	//��tab�д洢��LTC����������tabMagFresnel�д洢��LTC��Magnitude��Fresnel���Լ�tabSphere�д洢������Ǵ浽������

	// export to DDS
	writeDDS(tex1, tex2, N);

	// delete data
	delete[] tab;
	delete[] tabMagFresnel;
	delete[] tabSphere;
	delete[] tex1;
	delete[] tex2;

	return 0;
}