#include "pch.h"

#include "ShaderCompiler.h"

// Convert a wide Unicode string to an UTF8 string
std::string utf8_encode(const std::wstring &wstr)
{
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}

int ShaderCompiler::CompileShader(Platform::String^ shaderText, std::string profile, ID3DBlob** blob)
{
	if (!blob)
		return E_INVALIDARG;

	*blob = nullptr;

	const D3D_SHADER_MACRO defines[] =
	{
		"EXAMPLE_DEFINE", "1",
		NULL, NULL
	};

	ID3DBlob* shaderBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	HRESULT hr = D3DCompile(utf8_encode(shaderText->Data()).c_str(), 
		shaderText->Length(),
		NULL,
		defines,
		NULL,
		"main",
		profile.c_str(),
		NULL,
		0,
		&shaderBlob,
		&errorBlob);

	if (FAILED(hr))
	{
		if (errorBlob)
		{
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			errorBlob->Release();
		}

		if (shaderBlob)
			shaderBlob->Release();

		return hr;
	}

	*blob = shaderBlob;

	return hr;
}

ShaderCompiler::ShaderCompiler()
{
}
