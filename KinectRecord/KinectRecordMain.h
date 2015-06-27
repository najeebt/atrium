#pragma once

#define MAX_BUFFER_FRAMES 100
#define RELATIVE_TIME_TO_FRAME_MULT 0.000003
#define DEPTH_FRAME_BYTE_COUNT 512*424 + 20

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
		void EndRecording();
		void StreamColor(bool showColor);

		void Record();

		// playback
		void PrepToPlayback();
		void CacheFrameForPlayback(int frame);

		// export
		void ExportTakeToObj();
		int ExportFrameToObj(uint64 startTime, uint64 frameTime, int prevFrame, Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ csps);

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
		
		int lastTakeDuration;

		// playback buffer stores MAX_BUFFER_FRAMES frames of point data
		Platform::Collections::Vector< Platform::Object^ >^ playbackBuffer;
		Platform::Collections::Vector< Platform::Object^ >^ saveToDiskBuffer;

		// winrt file management
		Collections::IVectorView<Windows::Storage::StorageFile^>^ recordFiles;
		Collections::IVectorView<Windows::Storage::StorageFile^>^ exportFiles;
		Windows::Storage::StorageFolder^ sessionFolder;
		Windows::Storage::StorageFile^ takeFile;
		Windows::Storage::StorageFile^ exportFromFile;
		Windows::Storage::StorageFolder^ exportToFolder;
		std::vector<Windows::Storage::StorageFile^> shaderFiles;

		Windows::Storage::StorageFile^ recordFile;
		Windows::Storage::Streams::IRandomAccessStream^ recordStream;

		Windows::Storage::Streams::IRandomAccessStream^ takeStream;

		// tracking for recompiling shaders
		std::vector<Windows::Storage::Search::StorageFileQueryResult^> shaderQueryResult;
	
	private:
		void ProcessInput();
		void Update();
		bool Render();

		int CalculateFrameNumber(uint64 startTime, uint64 currentTime);

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