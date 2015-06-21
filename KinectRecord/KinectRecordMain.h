﻿#pragma once

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
		KinectHandler^ kinectHandler;

		// state management (should probably be single-variable state enum)
		bool isRecording;
		bool isPlayingBack;
		bool isExporting;

		bool streamColor;

		int currentFrame;
		int currentTake;
		int currentExportFrame;
		uint64 recStartTime;

		int lDFrame;
		int lCFrame;

		// playback buffer stores MAX_BUFFER_FRAMES frames of point data
		Platform::Collections::Vector< Platform::Object^ >^ playbackBuffer;
		Platform::Collections::Vector< Platform::Object^ >^ saveToDiskBuffer;

		//// holding onto data until it's written
		Platform::Collections::Vector< Platform::Object^ >^ depthDataCache;
		Platform::Collections::Vector< Platform::Object^ >^ colorBufferCache;

		// winrt file management
		Collections::IVectorView<Windows::Storage::StorageFile^>^ recordFiles;
		Collections::IVectorView<Windows::Storage::StorageFile^>^ exportFiles;
		Windows::Storage::StorageFolder^ sessionFolder;
		Windows::Storage::StorageFolder^ takeFolder;
		Windows::Storage::StorageFolder^ exportFromFolder;
		Windows::Storage::StorageFolder^ exportToFolder;
		std::vector<Windows::Storage::StorageFile^> shaderFiles;

		// tracking for recompiling shaders
		std::vector<Windows::Storage::Search::StorageFileQueryResult^> shaderQueryResult;
	
	private:
		void ProcessInput();
		void Update();
		bool Render();

		void StoreFrameForWrite(const int frame, Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ cameraSpacePoints);
		void WriteDepthFrameToDisk(int frameIndex, int frame, Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ cameraSpacePoints);
		void WriteDepthUVFrameToDisk(int frameIndex, int frame, Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ cameraSpacePoints, Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>^ colorSpacePoints);
		void WriteUVToDisk(const Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>^ colorSpacePoints);
		void WriteJpegToDisk(int frameIndex, int frame, Windows::Storage::Streams::Buffer^ colorData);

		int CalculateFrameNumber(uint64 startTime, uint64 currentTime);


		Platform::Array<byte>^ pixelStore;

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