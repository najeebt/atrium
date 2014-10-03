#pragma once

namespace KinectRecord
{
	// Constant buffer used to send MVP matrices to the vertex shader.
	struct ModelViewProjectionConstantBuffer
	{
		DirectX::XMFLOAT4X4 model;
		DirectX::XMFLOAT4X4 view;
		DirectX::XMFLOAT4X4 projection;
	};

	// Used to send per-vertex data to the vertex shader.
	struct VertexPosition
	{
		DirectX::XMFLOAT3 pos;
	};

	struct VertexColor
	{
		DirectX::XMFLOAT3 color;
	};

	// Used to send per-vertex data to the vertex shader.
	struct TextureCoordinate
	{
		DirectX::XMFLOAT2 pos;
	};

	struct SceneConstants
	{
		float time;
		DirectX::XMFLOAT3 lightVec;
	};
}