#pragma once

#include "Common.h"

class Scene;
class MeshNode;
class FCommandContext;
class Renderer
{
public:
	void Draw(Scene* pScene, FCommandContext& CommandContext, bool UseDefaultMaterial = true);

private:
};