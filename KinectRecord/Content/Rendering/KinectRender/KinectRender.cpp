#include "pch.h"
#include "KinectRender.h"
#include "..\ShaderCompiler\ShaderCompiler.h"

#include "..\Common\DirectXHelper.h"

#define FRAME_LAG 3

using namespace KinectRecord;

using namespace DirectX;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::System;
using namespace Windows::Security::Cryptography;
using namespace Concurrency;


// Loads vertex and pixel shaders from files and instantiates the cube geometry.
KinectRender::KinectRender(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_loadingComplete(false),
	m_degreesPerSecond(45),
	m_indexCount(0),
	m_tracking(false),
	m_time(0),
	m_vertexBufferData(3),
	m_vertexBuffer(3),
	m_cam(),
	m_shadowMapDimension(4096.f),
	m_deviceResources(deviceResources)
{
	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}

// Initializes view parameters when the window size changes.
void KinectRender::CreateWindowSizeDependentResources()
{
	Size outputSize = m_deviceResources->GetOutputSize();
	float aspectRatio = outputSize.Width / outputSize.Height;
	float fovAngleY = 50.0f * XM_PI / 180.0f;

	// This is a simple example of change that can be made when the app is in
	// portrait or snapped view.
	if (aspectRatio < 1.0f)
	{
		fovAngleY *= 2.0f;
	}

	m_cam.InitProjMatrix(fovAngleY, outputSize.Width, outputSize.Height, 0.01f, 50.0f);
	
	XMStoreFloat4x4(
		&m_constantBufferData.projection,
		XMLoadFloat4x4(&m_cam.Proj()));
	
	// Eye is at (0,0.7,1.5), looking at point (0,-0.1,0) with the up-vector along the y-axis.
	static const XMFLOAT3 eye = { 0.0f, 0.0f, 0.0f };
	static const XMFLOAT3 at = { 0.0f, -0.0f, 8.0f };

	m_cam.Position(eye);
	m_cam.Target(at);

	XMStoreFloat4x4(
		&m_constantBufferData.view,
		XMLoadFloat4x4(&m_cam.View()));

	XMStoreFloat4x4(&m_constantBufferData.model, XMMatrixTranspose(XMMatrixRotationZ(0)));

	XMStoreFloat(&m_sceneConstantBufferData.time, XMLoadFloat(&m_time));

	WindowsPreview::Kinect::CameraSpacePoint p;
	p.X = -0.0;
	p.Y = 3.0;
	p.Z = -0.0;
	UpdateLightPositions(p);
}

void KinectRender::UpdateTime(int time)
{
	if (time < 0) {
		m_time++;
		m_time = int(m_time) % 480;
	}
	else {
		m_time = time;
	}
}

void KinectRender::UpdateLightPositions(WindowsPreview::Kinect::CameraSpacePoint p)
{
	// Set up the light view.
	GCamera lightCam;

	lightCam.InitProjMatrix(XM_PIDIV2, 2048, 2048, 1.0f, 20.0f);

	XMStoreFloat4x4(
		&m_lightViewProjectionBufferData.projection,
		XMLoadFloat4x4(&lightCam.Proj()));

	static const XMFLOAT3 eye = { p.X, p.Y, p.Z };
	static const XMFLOAT3 at = { 0.0f, 0.0f, 3.0f };

	lightCam.Position(eye);
	lightCam.Target(at);

	XMStoreFloat4x4(
		&m_lightViewProjectionBufferData.view,
		XMLoadFloat4x4(&lightCam.View()));

	XMStoreFloat4x4(&m_lightViewProjectionBufferData.model, XMMatrixTranspose(XMMatrixRotationZ(0)));

	// Store the light position to help calculate the shadow offset.
	XMVECTORF32 eyeV = { p.X, p.Y, p.Z, 1.0f };
	XMStoreFloat3(&m_sceneConstantBufferData.lightVec, eyeV);
}

void KinectRender::UpdateVertexBuffer(Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ cameraSpacePoints)
{

	XMStoreFloat(&m_sceneConstantBufferData.time, XMLoadFloat(&m_time));

	if (m_vertexBufferData[0].size() == cameraSpacePoints->Length) {

		//for (int i = 0; i < (FRAME_LAG - 1); ++i) {
		//	m_vertexBufferData[i + 1] = m_vertexBufferData[i];
		//}

		m_vertexBufferData[0].assign(reinterpret_cast<VertexPosition *>(cameraSpacePoints->begin()), reinterpret_cast<VertexPosition *>(cameraSpacePoints->end()));

		D3D11_BOX box{};

		for (int i = 0; i < FRAME_LAG; i++) {
			box.left = 0;
			box.right = m_vertexBufferData[i].size()*sizeof(VertexPosition);
			box.top = 0;
			box.bottom = 1;
			box.front = 0;
			box.back = 1;
			m_deviceResources->GetD3DDeviceContext()->UpdateSubresource(m_vertexBuffer[i].Get(), 0, &box, (uint8_t*)&m_vertexBufferData[i][0], 0, 0);
		}
	}
}

void KinectRender::UpdateColorBuffer(Windows::Storage::Streams::Buffer^ colorData, Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>^ colorSpacePoints)
{

	if (m_colorBufferData.size() == colorSpacePoints->Length) {

		Platform::Array<unsigned char>^ colorBytes = ref new Platform::Array<unsigned char>(colorData->Length);
		CryptographicBuffer::CopyToByteArray(colorData, &colorBytes);

		for (int i = 0; i < KINECT_DEPTH_HEIGHT; ++i) {
			for (int j = 0; j < KINECT_DEPTH_WIDTH; ++j) {
				int k = i*KINECT_DEPTH_WIDTH + j;
				WindowsPreview::Kinect::ColorSpacePoint csp = colorSpacePoints[k];
				if (isinf(csp.X) || csp.X < 0 || csp.Y < 0 || csp.X > 1919 || csp.Y > 1079) {
					m_colorBufferData[k] = { XMFLOAT3(0.0, 0.0, 0.0) };
					continue;
				}
				int coord = (int(csp.Y)*1920 + int(csp.X)) * 4;
				float r = colorBytes[coord] / 256.0;
				float g = colorBytes[coord + 1] / 256.0;
				float b = colorBytes[coord + 2] / 256.0;
				m_colorBufferData[k] = { XMFLOAT3(r, g, b) };
			}
		}

		D3D11_BOX box{};
		box.left = 0;
		box.right = m_colorBufferData.size()*sizeof(VertexColor);
		box.top = 0;
		box.bottom = 1;
		box.front = 0;
		box.back = 1;
		m_deviceResources->GetD3DDeviceContext()->UpdateSubresource(m_colorBuffer.Get(), 0, &box, (uint8_t*)&m_colorBufferData[0], 0, 0);
	}
}

void KinectRender::UpdateTextureCoordinateBuffer(Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>^ colorSpacePoints)
{
	if (m_texCoordBufferData.size() == colorSpacePoints->Length) {

		m_texCoordBufferData.assign(reinterpret_cast<TextureCoordinate *>(colorSpacePoints->begin()), reinterpret_cast<TextureCoordinate *>(colorSpacePoints->end()));

		D3D11_BOX box{};
		box.left = 0;
		box.right = m_texCoordBufferData.size()*sizeof(TextureCoordinate);
		box.top = 0;
		box.bottom = 1;
		box.front = 0;
		box.back = 1;
		m_deviceResources->GetD3DDeviceContext()->UpdateSubresource(m_texCoordBuffer.Get(), 0, &box, (uint8_t*)&m_texCoordBufferData[0], 0, 0);
	}
}

// Called once per frame, rotates the cube and calculates the model and view matrices.
void KinectRender::Update(DX::StepTimer const& timer)
{
	// If the shadow dimension has changed, recreate it.
	D3D11_TEXTURE2D_DESC desc = { 0 };
	if (m_shadowMap != nullptr)
	{
		m_shadowMap->GetDesc(&desc);
		if (m_shadowMapDimension != desc.Height)
		{
			InitShadowMap();
		}
	}
}

// Rotate the 3D cube model a set amount of radians.
void KinectRender::Rotate(float radiansX, float radiansY)
{
	// Prepare to pass the updated model matrix to the shader
	XMStoreFloat4x4(&m_constantBufferData.model, XMMatrixTranspose(XMMatrixRotationY(radiansX)));
	XMStoreFloat4x4(&m_lightViewProjectionBufferData.model, XMMatrixTranspose(XMMatrixRotationY(radiansX)));
}

void KinectRender::StartTracking()
{
	m_tracking = true;
}

// When tracking, the 3D cube can be rotated around its Y axis by tracking pointer position relative to the output screen width.
void KinectRender::TrackingUpdate(int trackingType, float positionX, float positionY)
{
	if (m_tracking)
	{
		if (trackingType == 0) {
			float radiansX = XM_2PI * 2.0f * positionX / m_deviceResources->GetOutputSize().Width;
			float radiansY = XM_2PI * 2.0f * positionY / m_deviceResources->GetOutputSize().Height;
			Rotate(radiansX, radiansY);
		}
		else if (trackingType == 1) {
			m_cam.Move(XMFLOAT3(0.0, 0.0, -0.001*positionY));
			float height = m_deviceResources->GetOutputSize().Height;
			XMStoreFloat4x4(
				&m_constantBufferData.view,
				XMLoadFloat4x4(&m_cam.View()));
		}
		else {
			m_cam.Move(XMFLOAT3(0.0, -0.001*positionY, 0.0));
			float height = m_deviceResources->GetOutputSize().Height;
			XMStoreFloat4x4(
				&m_constantBufferData.view,
				XMLoadFloat4x4(&m_cam.View()));
		}
	}
}

void KinectRender::StopTracking()
{
	m_tracking = false;
}

void KinectRender::UpdateShader(Platform::String^ shaderText, int shaderType)
{
	std::vector<std::string> profiles = { "vs_5_0", "gs_5_0", "ps_5_0" };
	std::string profile = profiles[shaderType];

	if (shaderType == 0) {
		ShaderCompiler::CompileShader(shaderText, profile, &m_vsBlob);
		if (m_vsBlob) {
			DX::ThrowIfFailed(
				m_deviceResources->GetD3DDevice()->CreateVertexShader(
				m_vsBlob->GetBufferPointer(),
				m_vsBlob->GetBufferSize(),
				nullptr,
				&m_vertexShader
				)
				);
		}
	}
	else if (shaderType == 1) {
		ShaderCompiler::CompileShader(shaderText, profile, &m_gsBlob);
		if (m_gsBlob) {
			DX::ThrowIfFailed(
				m_deviceResources->GetD3DDevice()->CreateGeometryShader(
				m_gsBlob->GetBufferPointer(),
				m_gsBlob->GetBufferSize(),
				nullptr,
				&m_geomShader
				)
				);
		}
	}
	else if (shaderType == 2) {
		ShaderCompiler::CompileShader(shaderText, profile, &m_psBlob);
		if (m_psBlob) {
			DX::ThrowIfFailed(
				m_deviceResources->GetD3DDevice()->CreatePixelShader(
				m_psBlob->GetBufferPointer(),
				m_psBlob->GetBufferSize(),
				nullptr,
				&m_pixelShader
				)
				);
		}
	}
}

void KinectRender::UpdateAllConstantBuffers()
{
	auto context = m_deviceResources->GetD3DDeviceContext();

	// Prepare the constant buffer to send it to the graphics device.
	context->UpdateSubresource(
		m_constantBuffer.Get(),
		0,
		NULL,
		&m_constantBufferData,
		0,
		0
		);

	// Prepare the constant buffer to send it to the graphics device.
	context->UpdateSubresource(
		m_sceneConstantBuffer.Get(),
		0,
		NULL,
		&m_sceneConstantBufferData,
		0,
		0
		);

	context->UpdateSubresource(
		m_lightViewProjectionBuffer.Get(),
		0,
		NULL,
		&m_lightViewProjectionBufferData,
		0,
		0
		);
}

void KinectRender::Render()
{
	// Loading is asynchronous. Only draw geometry after it's loaded.
	if (!m_loadingComplete)
	{
		return;
	}

	auto context = m_deviceResources->GetD3DDeviceContext();

	// Clear the back buffer and depth stencil view.
	context->ClearRenderTargetView(m_deviceResources->GetBackBufferRenderTargetView(), DirectX::Colors::Black);
	context->ClearDepthStencilView(m_shadowDepthView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	context->ClearDepthStencilView(m_deviceResources->GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	RenderShadowMap();
	RenderScene();
}

// Renders one frame using the vertex and pixel shaders.
void KinectRender::RenderScene()
{
	auto context = m_deviceResources->GetD3DDeviceContext();

	// Set render targets to the screen.
	ID3D11RenderTargetView *const targets[1] = { m_deviceResources->GetBackBufferRenderTargetView() };
	context->OMSetRenderTargets(1, targets, m_deviceResources->GetDepthStencilView());

	// Set rendering state.
	context->RSSetState(m_drawingRenderState.Get());

	D3D11_VIEWPORT view = m_deviceResources->GetScreenViewport();
	context->RSSetViewports(1, &view);

	UpdateAllConstantBuffers();

	UINT stride = sizeof(VertexPosition);
	UINT offset = 0;
	context->IASetVertexBuffers(
		0,
		1,
		m_vertexBuffer[0].GetAddressOf(),
		&stride,
		&offset
		);

	context->IASetVertexBuffers(
		1,
		1,
		m_colorBuffer.GetAddressOf(),
		&stride,
		&offset
		);

	// additional vertex buffers

	//context->IASetVertexBuffers(
	//	2,
	//	1,
	//	m_vertexBuffer[1].GetAddressOf(),
	//	&stride,
	//	&offset
	//	);

	//context->IASetVertexBuffers(
	//	3,
	//	1,
	//	m_vertexBuffer[2].GetAddressOf(),
	//	&stride,
	//	&offset
	//	);

	context->IASetIndexBuffer(
		m_indexBuffer.Get(),
		DXGI_FORMAT_R16_UINT, // Each index is one 16-bit unsigned integer (short).
		0
		);

	//context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ);

	context->IASetInputLayout(m_inputLayout.Get());

	context->GSSetShader(
		m_geomShader.Get(),
		nullptr,
		0
		);

	// Attach our vertex shader.
	context->VSSetShader(
		m_vertexShader.Get(),
		nullptr,
		0
		);

	// Send the constant buffer to the graphics device.
	context->VSSetConstantBuffers(
		0,
		1,
		m_constantBuffer.GetAddressOf()
		);

	// Send the constant buffer to the graphics device.
	context->VSSetConstantBuffers(
		1,
		1,
		m_sceneConstantBuffer.GetAddressOf()
		);

	// Send the constant buffer to the graphics device.
	context->VSSetConstantBuffers(
		2,
		1,
		m_lightViewProjectionBuffer.GetAddressOf() 
		);

	// Send the constant buffer to the graphics device.
	context->GSSetConstantBuffers(
		1,
		1,
		m_sceneConstantBuffer.GetAddressOf()
		);

	// Send the constant buffer to the graphics device.
	context->GSSetConstantBuffers(
		2,
		1,
		m_lightViewProjectionBuffer.GetAddressOf()
		);

	// Send the constant buffer to the graphics device.
	context->PSSetConstantBuffers(
		1,
		1,
		m_sceneConstantBuffer.GetAddressOf()
		);

	ID3D11SamplerState** comparisonSampler;
	if (true)
	{
		comparisonSampler = m_comparisonSampler_linear.GetAddressOf();
	}
	else
	{
		comparisonSampler = m_comparisonSampler_point.GetAddressOf();
	}

	context->PSSetSamplers(0, 1, comparisonSampler);
	context->PSSetShaderResources(0, 1, m_shadowResourceView.GetAddressOf());

	// Attach our pixel shader.
	context->PSSetShader(
		m_pixelShader.Get(),
		nullptr,
		0
		);

	// one row at a time
	// XXX -- could bundle these for better performance?
	for (int i = 0; i < 422; i++) {
		context->DrawIndexed(
			510 * 12,
			0,
			i*512
			);
	}

}

void KinectRender::CreateDeviceDependentResources()
{
	
	DetermineShadowFeatureSupport();

	// Load shaders asynchronously.
	auto loadGSTask = DX::ReadDataAsync(L"DefaultGeometryShader.cso");
	auto loadVSTask = DX::ReadDataAsync(L"BaseVertexShader.cso");
	auto loadPSTask = DX::ReadDataAsync(L"SimpleColorPixelShader.cso");
	auto loadShadowVSTask = DX::ReadDataAsync(L"ShadowVertexShader.cso");
	auto loadShadowGSTask = DX::ReadDataAsync(L"ShadowGeometryShader.cso");

	auto createGSTask = loadGSTask.then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateGeometryShader(
			&fileData[0],
			fileData.size(),
			nullptr,
			&m_geomShader
			)
			);

	});

	// After the vertex shader file is loaded, create the shader and input layout.
	auto createVSTask = loadVSTask.then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateVertexShader( 
				&fileData[0],
				fileData.size(),
				nullptr,
				&m_vertexShader
				)
			);

		static const D3D11_INPUT_ELEMENT_DESC vertexDesc [] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateInputLayout(
				vertexDesc,
				ARRAYSIZE(vertexDesc),
				&fileData[0],
				fileData.size(),
				&m_inputLayout
				)
			);
	});

	// simplified vertex shader for lights
	auto createShadowVSTask = loadShadowVSTask.then([this](const std::vector<byte>& fileData)
	{
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateVertexShader(
			&fileData[0],
			fileData.size(),
			nullptr,
			&m_shadowVertexShader
			)
			);
	});

	// simplified vertex shader for lights
	auto createShadowGSTask = loadShadowGSTask.then([this](const std::vector<byte>& fileData)
	{
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateGeometryShader(
			&fileData[0],
			fileData.size(),
			nullptr,
			&m_shadowGeometryShader
			)
			);
	});

	// After the pixel shader file is loaded, create the shader and constant buffer.
	auto createPSTask = loadPSTask.then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreatePixelShader(
				&fileData[0],
				fileData.size(),
				nullptr,
				&m_pixelShader
				)
			);

		CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelViewProjectionConstantBuffer) , D3D11_BIND_CONSTANT_BUFFER);
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateBuffer(
				&constantBufferDesc,
				nullptr,
				&m_constantBuffer
				)
			);

		CD3D11_BUFFER_DESC sceneConstantBufferDesc(sizeof(SceneConstants) , D3D11_BIND_CONSTANT_BUFFER);
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateBuffer(
				&sceneConstantBufferDesc,
				nullptr,
				&m_sceneConstantBuffer
				)
			);

		CD3D11_BUFFER_DESC lightConstantBufferDesc(sizeof(ModelViewProjectionConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateBuffer(
			&lightConstantBufferDesc,
			nullptr,
			&m_lightViewProjectionBuffer
			)
			);

		Rotate(0, 0);
	});

	// Once both shaders are loaded, create the mesh.
	auto createGeometryTask = (createPSTask && createVSTask && createShadowVSTask && createShadowGSTask).then([this] () {

		// Start with a flat plane of points.
		if (m_vertexBufferData[0].empty()) {
			for (int i = -212; i < 212; ++i) {
				for (int j = -256; j < 256; ++j) {
					m_vertexBufferData[0].push_back({ XMFLOAT3(j*0.01f, i*0.01f, 0.0f) });
				}
			}
		}

		m_vertexBufferData[1] = m_vertexBufferData[0];
		m_vertexBufferData[2] = m_vertexBufferData[1];

		// vertex position buffer
		D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
		vertexBufferData.pSysMem = &m_vertexBufferData[0][0];
		vertexBufferData.SysMemPitch = 0;
		vertexBufferData.SysMemSlicePitch = 0;
		CD3D11_BUFFER_DESC vertexBufferDesc(m_vertexBufferData[0].size()*sizeof(VertexPosition), D3D11_BIND_VERTEX_BUFFER);

		for (int i = 0; i < FRAME_LAG; ++i) {
			DX::ThrowIfFailed(
				m_deviceResources->GetD3DDevice()->CreateBuffer(
				&vertexBufferDesc,
				&vertexBufferData,
				&m_vertexBuffer[i]
				)
				);
		}

		// Start with a gradient colored plane of points.
		if (m_colorBufferData.empty()) {
			for (int i = 0; i < 424; ++i) {
				for (int j = 0; j < 512; ++j) {
					m_colorBufferData.push_back({ XMFLOAT3(j / 512.0, i / 424.0, 0.0f) });
				}
			}
		}
		// vertex color buffer
		D3D11_SUBRESOURCE_DATA colorBufferData = { 0 };
		colorBufferData.pSysMem = &m_colorBufferData[0];
		colorBufferData.SysMemPitch = 0;
		colorBufferData.SysMemSlicePitch = 0;
		CD3D11_BUFFER_DESC colorBufferDesc(m_colorBufferData.size()*sizeof(VertexColor), D3D11_BIND_VERTEX_BUFFER);
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateBuffer(
			&colorBufferDesc,
			&colorBufferData,
			&m_colorBuffer
			)
			);

		std::vector<unsigned short> indexCount;
		for (unsigned int i = 1; i < KINECT_DEPTH_HEIGHT - 1; ++i) {
			for (unsigned int j = 1; j < KINECT_DEPTH_WIDTH - 1; ++j) {
				int index = i*KINECT_DEPTH_WIDTH + j;

				// standard tris
				//indexCount.push_back(index);
				//indexCount.push_back(index + 1);
				//indexCount.push_back(index + KINECT_DEPTH_WIDTH);

				//indexCount.push_back(index);
				//indexCount.push_back(index - 1);
				//indexCount.push_back(index - KINECT_DEPTH_WIDTH);

				// adj tris
				indexCount.push_back(index);
				indexCount.push_back(index - KINECT_DEPTH_WIDTH + 1);
				indexCount.push_back(index + 1);
				indexCount.push_back(index + KINECT_DEPTH_WIDTH + 1);
				indexCount.push_back(index + KINECT_DEPTH_WIDTH);
				indexCount.push_back(index + KINECT_DEPTH_WIDTH - 1);

				indexCount.push_back(index);
				indexCount.push_back(index + KINECT_DEPTH_WIDTH - 1);
				indexCount.push_back(index - 1);
				indexCount.push_back(index - KINECT_DEPTH_WIDTH - 1);
				indexCount.push_back(index - KINECT_DEPTH_WIDTH);
				indexCount.push_back(index - KINECT_DEPTH_WIDTH + 1);
			}
		}

		// POINTLIST rendering index construction
		//for (unsigned int i = 0; i < m_vertexBufferData.size(); ++i) {
		//	indexCount.push_back(i);
		//}

		m_indexCount = indexCount.size();

		D3D11_SUBRESOURCE_DATA indexBufferData = {0};
		indexBufferData.pSysMem = &indexCount[0];
		indexBufferData.SysMemPitch = 0;
		indexBufferData.SysMemSlicePitch = 0;
		CD3D11_BUFFER_DESC indexBufferDesc(indexCount.size() * sizeof(unsigned short), D3D11_BIND_INDEX_BUFFER);
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateBuffer(
				&indexBufferDesc,
				&indexBufferData,
				&m_indexBuffer
				)
			);
	});

	auto createShadowBuffersTask = (createGeometryTask).then([this]()
	{
		if (m_deviceSupportsD3D9Shadows)
		{
			// Init shadow map resources

			InitShadowMap();

			// Init samplers

			ID3D11Device1* pD3DDevice = m_deviceResources->GetD3DDevice();

			D3D11_SAMPLER_DESC comparisonSamplerDesc;
			ZeroMemory(&comparisonSamplerDesc, sizeof(D3D11_SAMPLER_DESC));
			comparisonSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
			comparisonSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
			comparisonSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
			comparisonSamplerDesc.BorderColor[0] = 1.0f;
			comparisonSamplerDesc.BorderColor[1] = 1.0f;
			comparisonSamplerDesc.BorderColor[2] = 1.0f;
			comparisonSamplerDesc.BorderColor[3] = 1.0f;
			comparisonSamplerDesc.MinLOD = 0.f;
			comparisonSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
			comparisonSamplerDesc.MipLODBias = 0.f;
			comparisonSamplerDesc.MaxAnisotropy = 0;
			comparisonSamplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
			comparisonSamplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT;

			// Point filtered shadows can be faster, and may be a good choice when
			// rendering on hardware with lower feature levels. This sample has a
			// UI option to enable/disable filtering so you can see the difference
			// in quality and speed.

			DX::ThrowIfFailed(
				pD3DDevice->CreateSamplerState(
				&comparisonSamplerDesc,
				&m_comparisonSampler_point
				)
				);

			comparisonSamplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
			DX::ThrowIfFailed(
				pD3DDevice->CreateSamplerState(
				&comparisonSamplerDesc,
				&m_comparisonSampler_linear
				)
				);

			D3D11_SAMPLER_DESC linearSamplerDesc;
			ZeroMemory(&linearSamplerDesc, sizeof(D3D11_SAMPLER_DESC));
			linearSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
			linearSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
			linearSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			linearSamplerDesc.MinLOD = 0;
			linearSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
			linearSamplerDesc.MipLODBias = 0.f;
			linearSamplerDesc.MaxAnisotropy = 0;
			linearSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			linearSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

			DX::ThrowIfFailed(
				pD3DDevice->CreateSamplerState(
				&linearSamplerDesc,
				&m_linearSampler
				)
				);

			// Init render states for back/front face culling

			D3D11_RASTERIZER_DESC drawingRenderStateDesc;
			ZeroMemory(&drawingRenderStateDesc, sizeof(D3D11_RASTERIZER_DESC));
			drawingRenderStateDesc.CullMode = D3D11_CULL_BACK;
			drawingRenderStateDesc.FillMode = D3D11_FILL_SOLID;
			drawingRenderStateDesc.DepthClipEnable = true; // Feature level 9_1 requires DepthClipEnable == true
			DX::ThrowIfFailed(
				pD3DDevice->CreateRasterizerState(
				&drawingRenderStateDesc,
				&m_drawingRenderState
				)
				);

			D3D11_RASTERIZER_DESC shadowRenderStateDesc;
			ZeroMemory(&shadowRenderStateDesc, sizeof(D3D11_RASTERIZER_DESC));
			shadowRenderStateDesc.CullMode = D3D11_CULL_FRONT;
			shadowRenderStateDesc.FillMode = D3D11_FILL_SOLID;
			shadowRenderStateDesc.DepthClipEnable = true;

			DX::ThrowIfFailed(
				pD3DDevice->CreateRasterizerState(
				&shadowRenderStateDesc,
				&m_shadowRenderState
				)
				);
		}
	});

	// Once the cube is loaded, the object is ready to be rendered.
	createShadowBuffersTask.then([this] () {
		m_loadingComplete = true;
	});
}

void KinectRender::DetermineShadowFeatureSupport()
{
	ID3D11Device1* pD3DDevice = m_deviceResources->GetD3DDevice();

	D3D11_FEATURE_DATA_D3D9_SHADOW_SUPPORT isD3D9ShadowSupported;
	ZeroMemory(&isD3D9ShadowSupported, sizeof(isD3D9ShadowSupported));
	pD3DDevice->CheckFeatureSupport(
		D3D11_FEATURE_D3D9_SHADOW_SUPPORT,
		&isD3D9ShadowSupported,
		sizeof(D3D11_FEATURE_D3D9_SHADOW_SUPPORT)
		);

	if (isD3D9ShadowSupported.SupportsDepthAsTextureWithLessEqualComparisonFilter)
	{
		m_deviceSupportsD3D9Shadows = true;
	}
	else
	{
		m_deviceSupportsD3D9Shadows = false;
	}
}

// Initialize a new shadow buffer with size equal to m_shadowMapDimension.
void KinectRender::InitShadowMap()
{
	ID3D11Device1* pD3DDevice = m_deviceResources->GetD3DDevice();

	D3D11_TEXTURE2D_DESC shadowMapDesc;
	ZeroMemory(&shadowMapDesc, sizeof(D3D11_TEXTURE2D_DESC));
	shadowMapDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	shadowMapDesc.MipLevels = 1;
	shadowMapDesc.ArraySize = 1;
	shadowMapDesc.SampleDesc.Count = 1;
	shadowMapDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
	shadowMapDesc.Height = static_cast<UINT>(m_shadowMapDimension);
	shadowMapDesc.Width = static_cast<UINT>(m_shadowMapDimension);

	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
	ZeroMemory(&depthStencilViewDesc, sizeof(D3D11_DEPTH_STENCIL_VIEW_DESC));
	depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.Texture2D.MipSlice = 0;

	D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
	ZeroMemory(&shaderResourceViewDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
	shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	shaderResourceViewDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	shaderResourceViewDesc.Texture2D.MipLevels = 1;

	HRESULT hr = pD3DDevice->CreateTexture2D(
		&shadowMapDesc,
		nullptr,
		&m_shadowMap
		);

	hr = pD3DDevice->CreateDepthStencilView(
		m_shadowMap.Get(),
		&depthStencilViewDesc,
		&m_shadowDepthView
		);

	hr = pD3DDevice->CreateShaderResourceView(
		m_shadowMap.Get(),
		&shaderResourceViewDesc,
		&m_shadowResourceView
		);

	if (FAILED(hr))
	{
		shadowMapDesc.Format = DXGI_FORMAT_R16_TYPELESS;
		depthStencilViewDesc.Format = DXGI_FORMAT_D16_UNORM;
		shaderResourceViewDesc.Format = DXGI_FORMAT_R16_UNORM;

		DX::ThrowIfFailed(
			pD3DDevice->CreateTexture2D(
			&shadowMapDesc,
			nullptr,
			&m_shadowMap
			)
			);

		DX::ThrowIfFailed(
			pD3DDevice->CreateDepthStencilView(
			m_shadowMap.Get(),
			&depthStencilViewDesc,
			&m_shadowDepthView
			)
			);

		DX::ThrowIfFailed(
			pD3DDevice->CreateShaderResourceView(
			m_shadowMap.Get(),
			&shaderResourceViewDesc,
			&m_shadowResourceView
			)
			);
	}

	// Init viewport for shadow rendering.
	RtlZeroMemory(&m_shadowViewport, sizeof(D3D11_VIEWPORT));
	m_shadowViewport.Height = m_shadowMapDimension;
	m_shadowViewport.Width = m_shadowMapDimension;
	m_shadowViewport.MinDepth = 0.f;
	m_shadowViewport.MaxDepth = 1.f;
}

// Loads the shadow buffer with shadow information.
void KinectRender::RenderShadowMap()
{
	auto context = m_deviceResources->GetD3DDeviceContext();

	// Ensure the depth buffer is not bound for input before using it as a render target.
	// If you don't do this, the Direct3D runtime will have to force an unbind which 
	// causes warnings in the debug output.
	ID3D11ShaderResourceView* nullSrv = nullptr;
	context->PSSetShaderResources(0, 1, &nullSrv);

	UpdateAllConstantBuffers();

	// Render all the objects in the scene that can cast shadows onto themselves or onto other objects.

	// Only bind the ID3D11DepthStencilView for output.
	context->OMSetRenderTargets(
		0,
		nullptr,
		m_shadowDepthView.Get()
		);

	// Set rendering state.
	context->RSSetState(m_shadowRenderState.Get());
	context->RSSetViewports(1, &m_shadowViewport);

	// Bind the vertex shader used to generate the depth map.
	context->VSSetShader(
		m_shadowVertexShader.Get(),
		nullptr,
		0
		);

	context->GSSetShader(
		m_shadowGeometryShader.Get(),
		nullptr,
		0
		);

	// Send the constant buffers to the Graphics device.
	context->VSSetConstantBuffers(
		0,
		1,
		m_lightViewProjectionBuffer.GetAddressOf()
		);

	// The vertex shader and rasterizer stages generate all the depth data that
	// we need. Set the pixel shader to null to disable the pixel shader and 
	// output-merger stages.
	ID3D11PixelShader* nullPS = nullptr;
	context->PSSetShader(
		nullPS,
		nullptr,
		0
		);

	// one row at a time
	// XXX -- could bundle these for better performance?
	for (int i = 0; i < 422; i++) {
		context->DrawIndexed(
			510 * 12,
			0,
			i * 512
			);
	}

}
void KinectRender::ResetColorBuffer()
{
	// Start with a gradient colored plane of points.
	m_colorBufferData.clear();
	for (int i = 0; i < 424; ++i) {
		for (int j = 0; j < 512; ++j) {
			m_colorBufferData.push_back({ XMFLOAT3(j / 512.0, i / 424.0, 0.0f) });
		}
	}
	D3D11_BOX box{};
	box.left = 0;
	box.right = m_colorBufferData.size()*sizeof(VertexColor);
	box.top = 0;
	box.bottom = 1;
	box.front = 0;
	box.back = 1;
	m_deviceResources->GetD3DDeviceContext()->UpdateSubresource(m_colorBuffer.Get(), 0, &box, (uint8_t*)&m_colorBufferData[0], 0, 0);
}

void KinectRender::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;
	m_vertexShader.Reset();
	m_inputLayout.Reset();
	m_pixelShader.Reset();
	m_constantBuffer.Reset();
	m_sceneConstantBuffer.Reset();
	for (int i = 0; i < FRAME_LAG; i++) {
		m_vertexBuffer[i].Reset();
	}
	m_colorBuffer.Reset();
	m_indexBuffer.Reset();

	m_shadowMap.Reset();
	m_shadowDepthView.Reset();
	m_shadowResourceView.Reset();
	m_comparisonSampler_point.Reset();
	m_comparisonSampler_linear.Reset();
	m_linearSampler.Reset();

	m_shadowRenderState.Reset();
	m_drawingRenderState.Reset();
}