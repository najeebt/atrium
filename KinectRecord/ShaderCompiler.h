#pragma once
#pragma comment(lib,"d3dcompiler.lib")

#include <d3dcompiler.h>
#include "..\Common\DeviceResources.h"
#include "..\Common\DirectXHelper.h"

ref class ShaderCompiler sealed
{
internal:
	ShaderCompiler();
	static int CompileShader(Platform::String^ shaderText, std::string profile, ID3DBlob** blob);
};

