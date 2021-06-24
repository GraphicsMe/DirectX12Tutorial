#pragma once

#include "Model.h"


class FSkyBox : public FModel
{
public:
	FSkyBox();
	~FSkyBox();
	bool IsSkyBox() const override { return true; }

private:
};