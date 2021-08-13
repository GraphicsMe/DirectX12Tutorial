#ifndef _BRDF_GGX_
#define _BRDF_GGX_

#include "brdf.h"

class BrdfGGX : public Brdf
{
public:
    virtual float eval(const vec3& V, const vec3& L, const float alpha, float& pdf) const
    {
        // 位于上半球之下，返回0
        if (V.z <= 0)
        {
            pdf = 0;
            return 0;
        }

        // masking
        const float LambdaV = lambda(alpha, V.z);

        // shadowing
        float G2;
        if (L.z <= 0.0f)
            G2 = 0;
        else
        {
            const float LambdaL = lambda(alpha, L.z);
            G2 = 1.0f/(1.0f + LambdaV + LambdaL);
        }

        // D
        // 法线分布函数部分
        const vec3 H = normalize(V + L);
		const float slopex = H.x / H.z;
		const float slopey = H.y / H.z;
		// 这个slopex* slopex + slopey * slopey其实等于(x ^ 2 + y ^ 2) / z ^ 2，结果就是tan(theta) * tan(theta)
		float D = 1.0f / (1.0f + (slopex * slopex + slopey * slopey) / alpha / alpha);
		D = D * D;
		D = D / (3.14159f * alpha * alpha * H.z * H.z * H.z * H.z);

        // 概率密度函数
        // GGX重要性采样得到的H向量的话：
        // 概率密度函数PDF为：D* NoH
        // 采样的是L向量的话：
        // PDF需要根据雅可比行列式进行转换，将H的分布概率转换为L的概率分布
        pdf = fabsf(D * H.z / 4.0f / dot(V, H));

        // Result = BRDF * dot(N,L) = DFG / (4 * dot(N,L) * dot(N,V)) * dot(N, L)
        // Result = BRDF * dot(N,L) = DFG / (4 * dot(N,V)) 
        // F = 1，dot(N,V) = V.z
        // Result = BRDF * dot(N,L) = DG / (4 * V.z) 
        float res = D * G2 / 4.0f / V.z;
        return res;
    }

    virtual vec3 sample(const vec3& V, const float alpha, const float U1, const float U2) const
    {
        // 计算方位角
        const float phi = 2.0f * 3.14159f * U1;
        // 修正粗糙度
        const float r = alpha * sqrtf(U2 / (1.0f - U2));
        // 通过枚举法线方向而不是观察方向得到所有法线与观察向量的组合情况
        const vec3 N = normalize(vec3(r * cosf(phi), r * sinf(phi), 1.0f));
        // 通过公式计算反射向量即入射光方向
        const vec3 L = -V + 2.0f * N * dot(N, V);
        return L;
    }

private:
    float lambda(const float alpha, const float cosTheta) const
    {
        const float a = 1.0f / alpha / tanf(acosf(cosTheta));
        return (cosTheta < 1.0f) ? 0.5f * (-1.0f + sqrtf(1.0f + 1.0f/a/a)) : 0.0f;    
    }
};

#endif
