#include "pch.h"
#include "KinectRecordMain.h"
#include "Common\DirectXHelper.h"

using namespace KinectRecord;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Imaging;
using namespace Windows::System::Threading;
using namespace Windows::Storage;
using namespace Windows::Storage::Search;
using namespace Windows::Storage::Streams;
using namespace Windows::Security::Cryptography;
using namespace Collections;
using namespace Concurrency;

// Loads and initializes application assets when the application is loaded.
KinectRecordMain::KinectRecordMain(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
m_deviceResources(deviceResources),
isRecording(false),
isPlayingBack(false),
streamColor(false),
currentFrame(1),
currentTake(1),
shaderFiles(3),
shaderQueryResult(3),
m_pointerLocationX(0.0f)
{
	// Register to be notified if the Device is lost or recreated
	m_deviceResources->RegisterDeviceNotify(this);

	// DirectX renderer initialization
	m_sceneRenderer = std::unique_ptr<KinectRender>(new KinectRender(m_deviceResources));
	m_fpsTextRenderer = std::unique_ptr<SampleFpsTextRenderer>(new SampleFpsTextRenderer(m_deviceResources));

	// Kinect input/playback classes
	kinectHandler = ref new KinectHandler();
	playbackBuffer = ref new Platform::Collections::Vector< Platform::Object^ >();

	// 60 fps to make sure we're getting all the data
	m_timer.SetFixedTimeStep(true);
	m_timer.SetTargetElapsedSeconds(1.0 / 60.0);

}

KinectRecordMain::~KinectRecordMain()
{
	// Deregister device notification
	m_deviceResources->RegisterDeviceNotify(nullptr);
}

// Updates application state when the window size changes (e.g. device orientation change)
void KinectRecordMain::CreateWindowSizeDependentResources()
{
	m_sceneRenderer->CreateWindowSizeDependentResources();
}

void KinectRecordMain::StartRenderLoop()
{
	// If the animation render loop is already running then do not start another thread.
	if (m_renderLoopWorker != nullptr && m_renderLoopWorker->Status == AsyncStatus::Started)
	{
		return;
	}

	// Create a task that will be run on a background thread.
	auto workItemHandler = ref new WorkItemHandler([this](IAsyncAction ^ action)
	{
		// Calculate the updated frame and render once per vertical blanking interval.
		while (action->Status == AsyncStatus::Started)
		{
			critical_section::scoped_lock lock(m_criticalSection);
			Update();
			if (Render())
			{
				m_deviceResources->Present();
			}
		}
	});

	// Run task on a dedicated high priority background thread.
	m_renderLoopWorker = ThreadPool::RunAsync(workItemHandler, WorkItemPriority::High, WorkItemOptions::TimeSliced);
}

void KinectRecordMain::StopRenderLoop()
{
	m_renderLoopWorker->Cancel();
}

void KinectRecordMain::PrepToRecord()
{
	// reset frame count
	currentFrame = 1;
	recStartTime = kinectHandler->GetCurrentDTime();

	// TODO: fix nutso file naming gymnastics
	std::wstringstream padTake;
	padTake << std::setfill(L'0') << std::setw(3) << currentTake;
	std::wstring fname(L"ATRIUM_TAKE_000.adv");
	fname.replace(12, 3, padTake.str().c_str());
	Platform::String^ fNameForWin = ref new Platform::String(fname.c_str());
	currentTake++;

	takeFile = create_task(sessionFolder->CreateFileAsync(fNameForWin)).get();
	takeStream = create_task(takeFile->OpenAsync(FileAccessMode::ReadWrite)).get();

	auto outputStream = takeStream->GetOutputStreamAt(0);
	auto writer = ref new DataWriter(outputStream);
	writer->WriteUInt64(recStartTime);

	create_task(writer->StoreAsync()).then([this, writer](unsigned int bytesStored) {
		writer->FlushAsync();
	});
}

void KinectRecordMain::EndRecording()
{
}

void KinectRecordMain::Record()
{
	if (kinectHandler->HasUnreadDepthData()) {

		int frameByteCount = DEPTH_PIXEL_COUNT * sizeof(CameraSpacePoint) + sizeof(uint64);

		auto outputStream = takeStream->GetOutputStreamAt(frameByteCount*(currentFrame - 1) + sizeof(uint64));
		auto writer = ref new DataWriter(outputStream);

		auto frameTime = kinectHandler->GetCurrentDTime();
		writer->WriteUInt64(frameTime);

		if (streamColor) {

			auto cameraSpacePoints = kinectHandler->GetCurrentDepthData();
			writer->WriteBytes(Platform::ArrayReference<byte>(reinterpret_cast<byte*>(cameraSpacePoints->begin()), DEPTH_PIXEL_COUNT*sizeof(CameraSpacePoint)));

			auto colorSpacePoints = kinectHandler->GetCurrentUVData();
			writer->WriteBytes(Platform::ArrayReference<byte>(reinterpret_cast<byte*>(cameraSpacePoints->begin()), DEPTH_PIXEL_COUNT*sizeof(ColorSpacePoint)));

			writer->WriteBuffer(kinectHandler->GetCurrentColorData());
		}
		else {
			auto cameraSpacePoints = kinectHandler->GetCurrentDepthData();
			writer->WriteBytes(Platform::ArrayReference<byte>(reinterpret_cast<byte*>(cameraSpacePoints->begin()), DEPTH_PIXEL_COUNT*sizeof(CameraSpacePoint)));
		}

		currentFrame++;

		// flush what you wrote
		create_task(writer->StoreAsync()).then([this, writer](unsigned int bytesStored) {
			writer->FlushAsync();
		});
	}
}

void KinectRecordMain::PrepToPlayback()
{
	// playback from frame 1
	currentFrame = 1;

	if (takeFile != nullptr) {
		
		if (takeStream == nullptr) {
			takeStream = create_task(takeFile->OpenAsync(FileAccessMode::Read)).get();
		}

		auto basicProperties = create_task(takeFile->GetBasicPropertiesAsync()).get();
		int frameByteCount = DEPTH_PIXEL_COUNT * sizeof(CameraSpacePoint) + sizeof(uint64);

		auto size = basicProperties->Size;
		int numFrames = (size - sizeof(uint64)) / frameByteCount;

		playbackBuffer = ref new Platform::Collections::Vector<Platform::Object^>(numFrames);
		for (int frame = 1; frame < MAX_BUFFER_FRAMES; ++frame) {
			CacheFrameForPlayback(frame);
		}
	}
}

int KinectRecordMain::CalculateFrameNumber(uint64 startTime, uint64 currentTime)
{
	// t ime is in 10^-7 seconds (originally Windows::Foundation::TimeSpan::Duration)
	uint64 diff = currentTime - startTime;
	return (int)(diff * RELATIVE_TIME_TO_FRAME_MULT);
}

void KinectRecordMain::CacheFrameForPlayback(int frame)
{
	int frameByteCount = DEPTH_PIXEL_COUNT * sizeof(CameraSpacePoint) + sizeof(uint64);

	auto inputStream = takeStream->GetInputStreamAt(frameByteCount*(frame - 1) + sizeof(uint64));
	auto takeReader = ref new DataReader(inputStream);

	unsigned int bytesLoaded = create_task(takeReader->LoadAsync(frameByteCount)).get();

	if (bytesLoaded == frameByteCount) {

		// read frame time
		uint64 frameTime = takeReader->ReadUInt64();

		// read camera space points
		Platform::Array<unsigned char>^ bytes = ref new Platform::Array<unsigned char>(DEPTH_PIXEL_COUNT * 12);
		takeReader->ReadBytes(bytes);
		auto csps = ref new Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>(reinterpret_cast<WindowsPreview::Kinect::CameraSpacePoint *>(bytes->begin()), DEPTH_PIXEL_COUNT);

		playbackBuffer->SetAt(frame - 1, csps);
	}
}

int KinectRecordMain::ExportFrameToObj(uint64 startTime, uint64 frameTime, int prevFrame, Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ csps)
{

	int frameNumber = CalculateFrameNumber(startTime, frameTime);
	frameNumber = frameNumber == prevFrame ? frameNumber + 1 : frameNumber;

	// XXX - nutso file naming gymnastics
	std::wstringstream padFrame;
	padFrame << std::setfill(L'0') << std::setw(3) << frameNumber;
	std::wstring fname(L"ATRIUM_FRAME_000.obj");
	fname.replace(13, 3, padFrame.str().c_str());
	Platform::String^ fNameForWin = ref new Platform::String(fname.c_str());

	create_task(exportToFolder->CreateFileAsync(fNameForWin)).then([this, frameNumber, csps](Windows::Storage::StorageFile^ objFile) {

		auto lines = ref new Platform::Collections::Vector<Platform::String^>();

		std::vector<int> indexMap(DEPTH_PIXEL_COUNT, -1);

		// points
		int vindex = 1;
		for (int i = 0; i < csps->Length; ++i) {
			std::wstring wline;
			if (std::isinf(csps[i].X)) {
				continue;
			}
			else {
				wline = std::wstring(L"v " + std::to_wstring(csps[i].X) + L" " + std::to_wstring(csps[i].Y) + L" " + std::to_wstring(csps[i].Z));
				indexMap[i] = vindex;
				vindex++;
			}
			auto line = ref new Platform::String(wline.c_str());
			lines->Append(line);
		}

		// face texture coordinates
		/*for (int i = 0; i < KINECT_DEPTH_HEIGHT; ++i) {
		for (int j = 0; j < KINECT_DEPTH_WIDTH; ++j) {

		int index = i*KINECT_DEPTH_WIDTH + j;
		if (indexMap[index] == -1) {
		continue;
		}

		std::wstring wline;
		WindowsPreview::Kinect::ColorSpacePoint p = uvs[index];
		wline = std::wstring(L"vt " + std::to_wstring(p.X*COLOR_WIDTH_MULT) + L" " + std::to_wstring(p.Y*COLOR_HEIGHT_MULT));
		auto line = ref new Platform::String(wline.c_str());
		lines->Append(line);
		}
		}*/

		// faces
		for (int i = 1; i < (KINECT_DEPTH_HEIGHT - 1); ++i) {
			for (int j = 1; j < (KINECT_DEPTH_WIDTH - 1); ++j) {
				int index = i * KINECT_DEPTH_WIDTH + j;
				std::wstring wline;

				// upper left face
				int v1 = index + 1;
				int v2 = index + 2;
				int v3 = index + KINECT_DEPTH_WIDTH + 1;

				int i1 = indexMap[v1];
				int i2 = indexMap[v2];
				int i3 = indexMap[v3];

				if (i1 > 0 && i2 > 0 && i3 > 0) {
					wline = std::wstring(L"f " + std::to_wstring(i1) + L"/" + std::to_wstring(i1) + L" " + std::to_wstring(i2) + L"/" + std::to_wstring(i2) + L" " + std::to_wstring(i3) + L"/" + std::to_wstring(i3));
					auto lineA = ref new Platform::String(wline.c_str());
					lines->Append(lineA);
				}

				// lower right face
				v1 = index + 1;
				v2 = index;
				v3 = index - KINECT_DEPTH_WIDTH + 1;

				i1 = indexMap[v1];
				i2 = indexMap[v2];
				i3 = indexMap[v3];

				if (i1 > 0 && i2 > 0 && i3 > 0) {
					wline = std::wstring(L"f " + std::to_wstring(i1) + L"/" + std::to_wstring(i1) + L" " + std::to_wstring(i2) + L"/" + std::to_wstring(i2) + L" " + std::to_wstring(i3) + L"/" + std::to_wstring(i3));
					auto lineA = ref new Platform::String(wline.c_str());
					lines->Append(lineA);
				}
			}
		}

		Windows::Storage::FileIO::WriteLinesAsync(objFile, lines);
	});

	return frameNumber;
}

void KinectRecordMain::ExportTakeToObj()
{
	if (exportFromFile != nullptr && exportToFolder != nullptr) {

		isExporting = true;

		if (takeStream == nullptr) {
			takeStream = create_task(exportFromFile->OpenAsync(FileAccessMode::Read)).get();
		}
		auto inputStream = takeStream->GetInputStreamAt(0);
		auto takeReader = ref new DataReader(inputStream);

		int frameByteCount = DEPTH_PIXEL_COUNT * sizeof(CameraSpacePoint) + sizeof(uint64);

		unsigned int bytesLoaded = create_task(takeReader->LoadAsync(sizeof(uint64))).get();
		uint64 exportStartTime = 0;

		if (bytesLoaded == sizeof(uint64)) {
			// ERROR DIALOGUE?
			exportStartTime = takeReader->ReadUInt64();
		}
		else {
			return;
		}

		// now we start loading the actual data
		bytesLoaded = create_task(takeReader->LoadAsync(frameByteCount)).get();
		int prevFrame = 0;
		while (bytesLoaded == frameByteCount) {

			// read frame time
			uint64 frameTime = takeReader->ReadUInt64();

			// read camera space points
			Platform::Array<unsigned char>^ bytes = ref new Platform::Array<unsigned char>(DEPTH_PIXEL_COUNT * 12);
			takeReader->ReadBytes(bytes);
			auto csps = ref new Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>(reinterpret_cast<WindowsPreview::Kinect::CameraSpacePoint *>(bytes->begin()), DEPTH_PIXEL_COUNT);

			prevFrame = ExportFrameToObj(exportStartTime, frameTime, prevFrame, csps);

			// try buffering a new frame
			bytesLoaded = create_task(takeReader->LoadAsync(frameByteCount)).get();
		}

		isExporting = false;
	}
}

void KinectRecordMain::StreamColor(bool newStreamColor)
{
	streamColor = newStreamColor;
	kinectHandler->SetStreamColor(streamColor);
	if (!streamColor) {
		m_sceneRenderer->ResetColorBuffer();
	}
}

// Updates the application state once per frame.
void KinectRecordMain::Update()
{
	ProcessInput();

	// Update scene objects.
	m_timer.Tick([&]()
	{

		// live feed from kinect
		if (kinectHandler->HasUnreadDepthData() && !isPlayingBack && !isRecording && !isExporting) {

			m_sceneRenderer->Update(m_timer);

			// DirectX rendering
			m_sceneRenderer->UpdateVertexBuffer(kinectHandler->GetCurrentDepthData());
			if (kinectHandler->HasUnreadHandData()) {
				m_sceneRenderer->UpdateLightPositions(kinectHandler->GetHands()->get(0));
			}
			m_sceneRenderer->UpdateTime(-1);
			if (streamColor && kinectHandler->HasUnreadColorData()) {
				m_sceneRenderer->UpdateColorBuffer(kinectHandler->GetCurrentColorData(), kinectHandler->GetCurrentUVData());
			}

			// advance to the next frame
			currentFrame++;
		}
		// playback
		else if (isPlayingBack) {

			m_sceneRenderer->Update(m_timer);

			if (playbackBuffer->Size >= currentFrame) {
				Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ csps = safe_cast<Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^>(playbackBuffer->GetAt(currentFrame - 1));
				if (csps != nullptr) {
					m_sceneRenderer->UpdateVertexBuffer(csps);
					m_sceneRenderer->UpdateTime(currentFrame);
					playbackBuffer->SetAt(currentFrame - 1, nullptr);

					task<void> cache_frame([this] {
						CacheFrameForPlayback((currentFrame + 98) % (playbackBuffer->Size - 1));
					});
					cache_frame.then([] {});
					currentFrame = currentFrame == (playbackBuffer->Size - 1) ? 1 : currentFrame + 1;
				}
			}
		}
		m_fpsTextRenderer->Update(m_timer);
	});
}

void KinectRecordMain::TrackingUpdate(int trackingType, float positionX, float positionY)
{
	m_trackingType = trackingType;
	m_pointerLocationX = positionX;
	m_pointerLocationY = positionY;
}

// Process all input from the user before updating game state
void KinectRecordMain::ProcessInput()
{
	// TODO: Add per frame input handling here.
	m_sceneRenderer->TrackingUpdate(m_trackingType, m_pointerLocationX, m_pointerLocationY);
}

// Renders the current frame according to the current application state.
// Returns true if the frame was rendered and is ready to be displayed.
bool KinectRecordMain::Render()
{
	// Don't try to render anything before the first Update.
	if (m_timer.GetFrameCount() == 0)
	{
		return false;
	}

	m_sceneRenderer->Render();
	m_fpsTextRenderer->Render();

	return true;
}

// Notifies renderers that device resources need to be released.
void KinectRecordMain::OnDeviceLost()
{
	m_sceneRenderer->ReleaseDeviceDependentResources();
	m_fpsTextRenderer->ReleaseDeviceDependentResources();
}

// Notifies renderers that device resources may now be recreated.
void KinectRecordMain::OnDeviceRestored()
{
	m_sceneRenderer->CreateDeviceDependentResources();
	m_fpsTextRenderer->CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}
