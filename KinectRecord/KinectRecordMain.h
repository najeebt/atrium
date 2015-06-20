#pragma once

#define MAX_BUFFER_FRAMES 100
#define RELATIVE_TIME_TO_FRAME_MULT 0.000003

#include "Common\StepTimer.h"
#include "Common\DeviceResources.h"
#include "Content\Rendering\KinectRender\KinectRender.h"
#include "Content\Rendering\TextRender\SampleFpsTextRenderer.h"
#include "Content\KinectHandler\KinectHandler.h"

///<summary>
/// Records and plays back data streams from the Kinect.
///</summary>
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
		void StartRenderLoop();
		void StopRenderLoop();
		Concurrency::critical_section& GetCriticalSection() { return m_criticalSection; }

		// recording
		void PrepToRecord();
		void StreamColor(bool showColor);

		// playback
		void PrepToPlayback();
		void CacheFrameForPlayback(int frame);

		// export
		void ExportTakeToObj();
		void ExportFrameToObj(Windows::Storage::StorageFolder^ exportFolder, Platform::String^ frameName, Windows::Storage::Streams::IBuffer^ frameData);
		
		void UpdateShader(Platform::String^ shaderText, int shaderType) { m_sceneRenderer->UpdateShader(shaderText, shaderType); }

		// IDeviceNotify
		virtual void OnDeviceLost();
		virtual void OnDeviceRestored();

		// kinect input handler
		KinectHandler^ m_kinectHandler;

		// state management (should probably be single-variable state enum)
		bool m_isRecording;
		bool m_isPlayingBack;
		bool m_isExporting;

		bool m_streamColor;

		int m_currentFrame;
		int m_currentTake;
		int m_currentExportFrame;

		int m_lDFrame;
		int m_lCFrame;

		// playback buffer stores MAX_BUFFER_FRAMES frames of point data
		Platform::Collections::Vector< Platform::Object^ >^ m_playbackBuffer;
		Platform::Collections::Vector< Platform::Object^ >^ m_saveToDiskBuffer;

		//// holding onto data until it's written
		Platform::Collections::Vector< Platform::Object^ >^ m_depthDataCache;
		Platform::Collections::Vector< Platform::Object^ >^ m_colorBufferCache;

		// winrt file management
		Collections::IVectorView<Windows::Storage::StorageFile^>^ m_recordFiles;
		Collections::IVectorView<Windows::Storage::StorageFile^>^ m_exportFiles;
		Windows::Storage::StorageFolder^ m_sessionFolder;
		Windows::Storage::StorageFolder^ m_takeFolder;
		Windows::Storage::StorageFolder^ m_exportFromFolder;
		Windows::Storage::StorageFolder^ m_exportToFolder;
		std::vector<Windows::Storage::StorageFile^> m_shaderFiles;

		// tracking for recompiling shaders
		std::vector<Windows::Storage::Search::StorageFileQueryResult^> m_shaderQueryResult;
	
	private:
		void ProcessInput();
		void Update();
		bool Render();

		void StoreFrameForWrite(const int frame, Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ cameraSpacePoints);
		void WriteDepthFrameToDisk(const Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ cameraSpacePoints);
		void WriteDepthUVFrameToDisk(int frame, Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ cameraSpacePoints, Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>^ colorSpacePoints);
		void WriteUVToDisk(const Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>^ colorSpacePoints);
		void WriteJpegToDisk(int frame, Windows::Storage::Streams::Buffer^ colorData);

		int CalculateFrameNumber(uint64 startTime, uint64 currentTime);


		Platform::Array<byte>^ m_pixelStore;

		// device resources.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		// concurrency
		Windows::Foundation::IAsyncAction^ m_renderLoopWorker;
		Concurrency::critical_section m_criticalSection;

		// renderers
		std::unique_ptr<KinectRender> m_sceneRenderer;
		std::unique_ptr<SampleFpsTextRenderer> m_fpsTextRenderer;

		// Rendering loop timer.
		DX::StepTimer m_timer;

		// Track current input pointer position.
		int m_trackingType;
		float m_pointerLocationX;
		float m_pointerLocationY;
	};
}