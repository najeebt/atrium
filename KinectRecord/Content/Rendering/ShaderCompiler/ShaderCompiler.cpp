//
// Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements.  
// See the NOTICE file distributed with this work for additional information* regarding copyright ownership.
// The ASF licenses this file to you under the Apache License, Version 2.0 (the* "License"); 
// you may not use this file except in compliance with the License.  You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License 
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and limitations under the License.
//

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
