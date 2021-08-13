#ifndef _BRDF_GGX_
#define _BRDF_GGX_

#include "brdf.h"

class BrdfGGX : public Brdf
{
public:
    virtual float eval(const vec3& V, const vec3& L, const float alpha, float& pdf) const
    {
        // λ���ϰ���֮�£�����0
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
        // ���߷ֲ���������
        const vec3 H = normalize(V + L);
		const float slopex = H.x / H.z;
		const float slopey = H.y / H.z;
		// ���slopex* slopex + slopey * slopey��ʵ����(x ^ 2 + y ^ 2) / z ^ 2���������tan(theta) * tan(theta)
		float D = 1.0f / (1.0f + (slopex * slopex + slopey * slopey) / alpha / alpha);
		D = D * D;
		D = D / (3.14159f * alpha * alpha * H.z * H.z * H.z * H.z);

        // �����ܶȺ���
        // GGX��Ҫ�Բ����õ���H�����Ļ���
        // �����ܶȺ���PDFΪ��D* NoH
        // ��������L�����Ļ���
        // PDF��Ҫ�����ſɱ�����ʽ����ת������H�ķֲ�����ת��ΪL�ĸ��ʷֲ�
        pdf = fabsf(D * H.z / 4.0f / dot(V, H));

        // Result = BRDF * dot(N,L) = DFG / (4 * dot(N,L) * dot(N,V)) * dot(N, L)
        // Result = BRDF * dot(N,L) = DFG / (4 * dot(N,V)) 
        // F = 1��dot(N,V) = V.z
        // Result = BRDF * dot(N,L) = DG / (4 * V.z) 
        float res = D * G2 / 4.0f / V.z;
        return res;
    }

    virtual vec3 sample(const vec3& V, const float alpha, const float U1, const float U2) const
    {
        // ���㷽λ��
        const float phi = 2.0f * 3.14159f * U1;
        // �����ֲڶ�
        const float r = alpha * sqrtf(U2 / (1.0f - U2));
        // ͨ��ö�ٷ��߷�������ǹ۲췽��õ����з�����۲�������������
        const vec3 N = normalize(vec3(r * cosf(phi), r * sinf(phi), 1.0f));
        // ͨ����ʽ���㷴������������ⷽ��
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
