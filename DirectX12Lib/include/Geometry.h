#pragma once

#include "Model.h"

class FSkyBox : public FModel
{
public:
	FSkyBox();
	~FSkyBox();
	bool IsSkyBox() const override { return true; }
};

class FCubeMapCross : public FModel
{
public:
	FCubeMapCross();
	~FCubeMapCross();
};

class FQuad : public FModel
{
public:
	FQuad();
	~FQuad();

	void GetLightPolygonalVertexPosSet(std::vector<Vector4f>& PolygonalLightVertexPos);
};