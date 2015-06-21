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
streamColor(true),
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

	depthDataCache = ref new Platform::Collections::Vector<Platform::Object^>(500);
	colorBufferCache = ref new Platform::Collections::Vector<Platform::Object^>(500);

	//for (int i = 0; i < 500; ++i) {
	//	auto pixelStore = ref new Platform::Array<byte>(1920 * 1080 * 4);
	//	m_colorBufferCache->SetAt(i, pixelStore);
	//}

	// 60 fps to make sure we're getting all the data
	m_timer.SetFixedTimeStep(true);
	m_timer.SetTargetElapsedSeconds(1.0 / 30.0);

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
	// don't record over an old take
	takeFolder = nullptr;

	// reset frame count
	currentFrame = 1;
	recStartTime = kinectHandler->GetCurrentDTime();

	lDFrame = -1;
	lCFrame = -1;

	// TODO: fix nutso file naming gymnastics
	std::wstringstream padTake;
	padTake << std::setfill(L'0') << std::setw(3) << currentTake;
	std::wstring fname(L"ATRIUM_TAKE_000");
	fname.replace(13, 3, padTake.str().c_str());
	Platform::String^ fNameForWin = ref new Platform::String(fname.c_str());
	currentTake++;

	create_task(sessionFolder->CreateFolderAsync(fNameForWin)).then([this](Windows::Storage::StorageFolder^ folder)
	{
		takeFolder = folder;
	});
}

void KinectRecordMain::PrepToPlayback()
{
	// playback from frame 1
	currentFrame = 1;

	if (takeFolder != nullptr) {
		Platform::Collections::Vector<Platform::String^>^ fileTypes = ref new Platform::Collections::Vector<Platform::String^>();
		fileTypes->Append(".adv");
		QueryOptions^ queryOptions = ref new QueryOptions(CommonFileQuery::OrderByName, fileTypes);
		StorageFileQueryResult^ queryResult = takeFolder->CreateFileQueryWithOptions(queryOptions);
		create_task(queryResult->GetFilesAsync()).then([this](IVectorView<Windows::Storage::StorageFile^>^ files)
		{
			recordFiles = files;
			playbackBuffer = ref new Platform::Collections::Vector<Platform::Object^>(recordFiles->Size);
			for (int frame = 1; frame < MAX_BUFFER_FRAMES; ++frame) {
				if ((recordFiles != nullptr) && (recordFiles->Size > frame)) {
					CacheFrameForPlayback(frame);
				}
			}
		});
	}
}

void KinectRecordMain::StoreFrameForWrite(const int frame, Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ cameraSpacePoints)
{
	saveToDiskBuffer->SetAt(frame, cameraSpacePoints);
}

int KinectRecordMain::CalculateFrameNumber(uint64 startTime, uint64 currentTime)
{
	// t ime is in 10^-7 seconds (originally Windows::Foundation::TimeSpan::Duration)
	uint64 diff = currentTime - startTime;
	return (int)(diff * RELATIVE_TIME_TO_FRAME_MULT + 0.5);
}

void KinectRecordMain::WriteDepthFrameToDisk(int frameIndex, int frame, Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ cameraSpacePoints)
{

	// XXX - nutso file naming gymnastics
	std::wstringstream padFrame;
	padFrame << std::setfill(L'0') << std::setw(5) << frame;
	std::wstring fname(L"KinectStreamDepth_00000.adv");
	fname.replace(18, 5, padFrame.str().c_str());
	auto fNameForWin = ref new Platform::String(fname.c_str());

	// cache frames so they're accessible at time of write
	// XXX copying data a bunch of ways to get it into right format
	std::vector<byte> bytesV(reinterpret_cast<byte*>(cameraSpacePoints->begin()), reinterpret_cast<byte*>(cameraSpacePoints->end()));
	auto bytes = ref new Platform::Array<byte>(&bytesV[0], bytesV.size());
	depthDataCache->SetAt(frameIndex, bytes);

	if (takeFolder != nullptr) {

		auto createFileTask = create_task(takeFolder->CreateFileAsync(fNameForWin));

		createFileTask.then([this, frameIndex](Windows::Storage::StorageFile^ file)
		{
			Windows::Storage::FileIO::WriteBytesAsync(file, safe_cast<Platform::Array<byte>^>(depthDataCache->GetAt(frameIndex)));
			depthDataCache->SetAt(frameIndex, nullptr);
		});

	}
}


void KinectRecordMain::WriteDepthUVFrameToDisk(int frameIndex, int frame, Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ cameraSpacePoints, Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>^ colorSpacePoints)
{

	// XXX - nutso file naming gymnastics
	std::wstringstream padFrame;
	padFrame << std::setfill(L'0') << std::setw(5) << frame;
	std::wstring fname(L"KinectStreamDepth_00000.adv");
	fname.replace(18, 5, padFrame.str().c_str());
	auto fNameForWin = ref new Platform::String(fname.c_str());

	// cache frames so they're accessible at time of write
	// XXX copying data a bunch of ways to get it into right format
	std::vector<byte> bytesV(reinterpret_cast<byte*>(cameraSpacePoints->begin()), reinterpret_cast<byte*>(cameraSpacePoints->end()));
	bytesV.insert(bytesV.end(), reinterpret_cast<byte*>(colorSpacePoints->begin()), reinterpret_cast<byte*>(colorSpacePoints->end()));
	auto bytes = ref new Platform::Array<byte>(&bytesV[0], bytesV.size());
	depthDataCache->SetAt(frameIndex, bytes);

	if (takeFolder != nullptr) {

		auto createFileTask = create_task(takeFolder->CreateFileAsync(fNameForWin));

		createFileTask.then([this, frameIndex](Windows::Storage::StorageFile^ file)
		{
			Windows::Storage::FileIO::WriteBytesAsync(file, safe_cast<Platform::Array<byte>^>(depthDataCache->GetAt(frameIndex)));
			depthDataCache->SetAt(frameIndex, nullptr);
		});

	}
}

void KinectRecordMain::WriteJpegToDisk(int frameIndex, int frame, Windows::Storage::Streams::Buffer^ colorData)
{

	// XXX - nutso file naming gymnastics
	std::wstringstream padFrame;
	padFrame << std::setfill(L'0') << std::setw(5) << frame;
	std::wstring fname(L"KinectPic_00000.jpg");
	fname.replace(10, 5, padFrame.str().c_str());
	Platform::String^ fNameForWin = ref new Platform::String(fname.c_str());


	// cache frames so they're accessible at time of write
	auto reader = DataReader::FromBuffer(colorData);
	auto pixelStore = ref new Platform::Array<byte>(1920 * 1080 * 4);
	reader->ReadBytes(pixelStore);
	colorBufferCache->SetAt(frameIndex, pixelStore);

	auto createFileTask = create_task(takeFolder->CreateFileAsync(fNameForWin));
	
	createFileTask.then([this, frameIndex](Windows::Storage::StorageFile^ file)
	{
		return file->OpenAsync(FileAccessMode::ReadWrite);
	}).then([this, frameIndex](IRandomAccessStream^ stream) {
		return BitmapEncoder::CreateAsync(BitmapEncoder::JpegEncoderId, stream);
	}).then([this, frameIndex](BitmapEncoder^ encoder) {
		encoder->SetPixelData(BitmapPixelFormat::Rgba8, BitmapAlphaMode::Ignore, 1920, 1080, 96.0, 96.0, safe_cast<Platform::Array<byte>^>(colorBufferCache->GetAt(frameIndex)));
		encoder->FlushAsync();
		colorBufferCache->SetAt(frameIndex, nullptr);
	});
}

void KinectRecordMain::CacheFrameForPlayback(int frame)
{
	StorageFile^ frameFile = recordFiles->GetAt(frame);
	create_task(FileIO::ReadBufferAsync(frameFile)).then([this, frame](IBuffer^ buffer) {
		Platform::Array<unsigned char>^ bytes = ref new Platform::Array<unsigned char>(buffer->Length);
		CryptographicBuffer::CopyToByteArray(buffer, &bytes);
		Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ csps =
			ref new Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>(reinterpret_cast<WindowsPreview::Kinect::CameraSpacePoint *>(bytes->begin()), DEPTH_PIXEL_COUNT);
		playbackBuffer->SetAt(frame - 1, csps);
	});
}

void KinectRecordMain::ExportFrameToObj(Windows::Storage::StorageFolder^ exportFolder, Platform::String^ frameName, Windows::Storage::Streams::IBuffer^ frameData)
{
	// XXX - nutso file naming gymnastics
	std::wstring fname(frameName->Data());
	fname.replace(0, 17, L"Frame");
	fname.replace(12, 3, L"obj");
	auto fNameForWin = ref new Platform::String(fname.c_str());

	create_task(exportFolder->CreateFileAsync(fNameForWin)).then([this, frameData](Windows::Storage::StorageFile^ objFile) {

		std::vector<int> indexMap(DEPTH_PIXEL_COUNT, -1);

		auto bytes = ref new Platform::Array<unsigned char>(frameData->Length);

		// the CryptographicBuffer library just happens to have this utility
		CryptographicBuffer::CopyToByteArray(frameData, &bytes);

		// first half of the file stores point coordinates
		auto csps = ref new Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>(reinterpret_cast<WindowsPreview::Kinect::CameraSpacePoint *>(bytes->begin()), DEPTH_PIXEL_COUNT);
		auto uvs = ref new Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>(reinterpret_cast<WindowsPreview::Kinect::ColorSpacePoint *>(bytes->begin() + DEPTH_PIXEL_COUNT * 12), DEPTH_PIXEL_COUNT);
		auto lines = ref new Platform::Collections::Vector<Platform::String^>();

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
		for (int i = 0; i < KINECT_DEPTH_HEIGHT; ++i) {
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
		}

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

		// this should only get incremented here
		currentExportFrame++;
		if (currentExportFrame < exportFiles->Size) {
			auto frameFile = exportFiles->GetAt(currentExportFrame);
			create_task(Windows::Storage::FileIO::ReadBufferAsync(frameFile)).then([this, frameFile](Windows::Storage::Streams::IBuffer^ buffer) {
				ExportFrameToObj(this->exportToFolder, frameFile->Name, buffer);
			});
		}
		else {
			isExporting = false;
		}
	});
}

void KinectRecordMain::ExportTakeToObj()
{
	if (exportFromFolder != nullptr && exportToFolder != nullptr) {

		isExporting = true;

		Platform::Collections::Vector<Platform::String^>^ fileTypes = ref new Platform::Collections::Vector<Platform::String^>();
		fileTypes->Append(".adv");
		QueryOptions^ queryOptions = ref new QueryOptions(CommonFileQuery::OrderByName, fileTypes);
		StorageFileQueryResult^ queryResult = exportFromFolder->CreateFileQueryWithOptions(queryOptions);
		create_task(queryResult->GetFilesAsync()).then([this](IVectorView<Windows::Storage::StorageFile^>^ files)
		{
			currentExportFrame = 0;
			exportFiles = files;
			Windows::Storage::StorageFile^ frameFile = files->GetAt(currentExportFrame);
			create_task(Windows::Storage::FileIO::ReadBufferAsync(frameFile)).then([this, frameFile](Windows::Storage::Streams::IBuffer^ buffer) {
				ExportFrameToObj(this->exportToFolder, frameFile->Name, buffer);
			});
		});
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
		// stream data to disk during recording
		else if (kinectHandler->HasUnreadDepthData() && isRecording) {

			int dFrame = CalculateFrameNumber(recStartTime, kinectHandler->GetCurrentDTime());
			dFrame = dFrame == lDFrame ? dFrame + 1 : dFrame;
			if (dFrame > lDFrame) {
				if (streamColor) {
					//WriteDepthUVFrameToDisk(currentFrame, currentFrame, kinectHandler->GetCurrentDepthData(), kinectHandler->GetCurrentUVData());
					WriteJpegToDisk(currentFrame, currentFrame, kinectHandler->GetCurrentColorData());
				}
				else {
					WriteDepthFrameToDisk(currentFrame, currentFrame, kinectHandler->GetCurrentDepthData());
				}
				lDFrame = dFrame;
				currentFrame++;
			}
			
			//WriteDepthUVFrameToDisk(currentFrame, kinectHandler->GetCurrentDepthData(), kinectHandler->GetCurrentUVData());
			//WriteJpegToDisk(currentFrame, kinectHandler->GetCurrentColorData());


			// advance to the next frame
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
					CacheFrameForPlayback((currentFrame + 98) % (playbackBuffer->Size - 1) + 1);
					currentFrame = currentFrame == (recordFiles->Size - 1) ? 1 : currentFrame + 1;
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
