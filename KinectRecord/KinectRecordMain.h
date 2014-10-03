#pragma once

#include "Common\StepTimer.h"
#include "Common\DeviceResources.h"
#include "Content\KinectRender.h"
#include "Content\SampleFpsTextRenderer.h"
#include "KinectHandler.h"

// Renders Direct2D and 3D content on the screen.
namespace KinectRecord
{
	class KinectRecordMain : public DX::IDeviceNotify
	{
	public:
		KinectRecordMain(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		~KinectRecordMain();
		void CreateWindowSizeDependentResources();
		void StartTracking() { m_sceneRenderer->StartTracking(); }
		void TrackingUpdate(int trackingType, float positionX, float positionY);
		void StopTracking() { m_sceneRenderer->StopTracking(); }
		bool IsTracking() { return m_sceneRenderer->IsTracking(); }
		void StreamColor(bool showColor);
		void StartRenderLoop();
		void StopRenderLoop();
		void PrepToRecord();
		void PrepToPlayback();
		void ExportTakeToObj();
		void ExportFrameToObj(Windows::Storage::StorageFolder^ exportFolder, Platform::String^ frameName, Windows::Storage::Streams::IBuffer^ frameData);
		Concurrency::critical_section& GetCriticalSection() { return m_criticalSection; }
		void PlaybackSequence(int frame);
		void UpdateShader(Platform::String^ shaderText, int shaderType) { m_sceneRenderer->UpdateShader(shaderText, shaderType); }

		// IDeviceNotify
		virtual void OnDeviceLost();
		virtual void OnDeviceRestored();

		bool m_isRecording;
		bool m_isPlayingBack;
		bool m_readCspCache;
		bool m_streamColor;
		bool m_isExporting;

		int m_currentFrame;
		int m_currentTake;
		int m_currentExportFrame;

		KinectHandler^ m_kinectHandler;
		Collections::IVectorView<Windows::Storage::StorageFile^>^ m_recordFiles;
		Collections::IVectorView<Windows::Storage::StorageFile^>^ m_exportFiles;
		Windows::Storage::StorageFolder^ m_sessionFolder;
		Windows::Storage::StorageFolder^ m_takeFolder;
		Windows::Storage::StorageFolder^ m_exportFromFolder;
		std::vector<Windows::Storage::StorageFile^> m_shaderFiles;

		std::vector<Windows::Storage::Search::StorageFileQueryResult^> m_shaderQueryResult;

		Platform::Collections::Vector< Platform::Object^ >^ m_cspCache;

		void CacheFrameForPlayback(int frame);
	
	private:
		void ProcessInput();
		void Update();
		bool Render();

		void WriteFrameToDisk(Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ cameraSpacePoints);
		void WriteUVToDisk(Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>^ colorSpacePoints);
		void WriteJpegToDisk(Windows::Storage::Streams::Buffer^ colorData);

		Platform::Array<byte>^ m_pixelStore;

		// Cached pointer to device resources.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		std::unique_ptr<KinectRender> m_sceneRenderer;
		std::unique_ptr<SampleFpsTextRenderer> m_fpsTextRenderer;

		Windows::Foundation::IAsyncAction^ m_renderLoopWorker;
		Concurrency::critical_section m_criticalSection;

		// Rendering loop timer.
		DX::StepTimer m_timer;

		// Track current input pointer position.
		int m_trackingType;
		float m_pointerLocationX;
		float m_pointerLocationY;
	};
}