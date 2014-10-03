#pragma once

#include "..\Common\DeviceResources.h"
#include "ShaderStructures.h"
#include "..\Common\StepTimer.h"

#include "GCamera.h"

namespace KinectRecord
{
	// This sample renderer instantiates a basic rendering pipeline.
	class KinectRender
	{
	public:
		KinectRender(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		void CreateDeviceDependentResources();
		void CreateWindowSizeDependentResources();
		void ReleaseDeviceDependentResources();
		void Update(DX::StepTimer const& timer);
		void Render();
		void RenderScene();
		void StartTracking();
		void TrackingUpdate(int trackingType, float positionX, float positionY);
		void StopTracking();
		void ResetColorBuffer();
		bool IsTracking() { return m_tracking; }
		void UpdateVertexBuffer(Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ cameraSpacePoints);
		void UpdateTextureCoordinateBuffer(Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>^ colorSpacePoints);
		void UpdateColorBuffer(Windows::Storage::Streams::Buffer^ colorData, Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>^ colorSpacePoints);
		void UpdateShader(Platform::String^ shaderText, int shaderType);
		void UpdateTime(int time);
		void RenderShadowMap();
		void UpdateAllConstantBuffers();
		ID3D11DepthStencilView* GetShadowDepthView() { return m_shadowDepthView.Get(); }

		// shadow support
		void DetermineShadowFeatureSupport();
		void InitShadowMap();

	private:
		void Rotate(float radiansX, float radiansY);

		// Cached pointer to device resources.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		GCamera m_cam;
		ID3DBlob *m_psBlob;
		ID3DBlob *m_gsBlob;
		ID3DBlob *m_vsBlob;

		byte* GetPointerToPixelData(Windows::Storage::Streams::IBuffer^ buffer);

		// Direct3D resources for cube geometry.
		Microsoft::WRL::ComPtr<ID3D11InputLayout>	m_inputLayout;
		std::vector< Microsoft::WRL::ComPtr<ID3D11Buffer> >	m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_colorBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_texCoordBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_indexBuffer;
		Microsoft::WRL::ComPtr<ID3D11VertexShader>	m_vertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader>	m_pixelShader;
		Microsoft::WRL::ComPtr<ID3D11GeometryShader>	m_geomShader;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_constantBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_sceneConstantBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_lightViewProjectionBuffer;

		// System resources for cube geometry.
		ModelViewProjectionConstantBuffer	m_constantBufferData;
		ModelViewProjectionConstantBuffer   m_lightViewProjectionBufferData;
		SceneConstants m_sceneConstantBufferData;
		uint32	m_indexCount;
		std::vector< std::vector<VertexPosition> > m_vertexBufferData;
		std::vector<VertexColor> m_colorBufferData;
		std::vector<TextureCoordinate> m_texCoordBufferData;

		// Variables used with the rendering loop.
		bool	m_loadingComplete;
		float	m_degreesPerSecond;
		bool	m_tracking;

		float m_time;

		// shadow support
		bool m_deviceSupportsD3D9Shadows;

		// Controls the size of the shadow map.
		float   m_shadowMapDimension;

		// Shadow buffer Direct3D resources.
		Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_shadowMap;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   m_shadowDepthView;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_shadowResourceView;
		Microsoft::WRL::ComPtr<ID3D11SamplerState>       m_comparisonSampler_point;
		Microsoft::WRL::ComPtr<ID3D11SamplerState>       m_comparisonSampler_linear;
		Microsoft::WRL::ComPtr<ID3D11SamplerState>       m_linearSampler;

		// Direct3D resources for displaying the shadow map.
		Microsoft::WRL::ComPtr<ID3D11Buffer>        m_vertexBufferQuad;
		Microsoft::WRL::ComPtr<ID3D11Buffer>        m_indexBufferQuad;
		Microsoft::WRL::ComPtr<ID3D11Buffer>        m_vertexBufferFloor;
		Microsoft::WRL::ComPtr<ID3D11Buffer>        m_indexBufferFloor;
		Microsoft::WRL::ComPtr<ID3D11VertexShader>	m_shadowVertexShader;
		Microsoft::WRL::ComPtr<ID3D11GeometryShader>	m_shadowGeometryShader;
		D3D11_VIEWPORT                              m_shadowViewport;

		// Render states for front face/back face culling.
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_shadowRenderState;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_drawingRenderState;


	};
}

