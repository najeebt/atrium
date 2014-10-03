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
m_isRecording(false),
m_isPlayingBack(false),
m_streamColor(false),
m_currentFrame(1),
m_currentTake(1),
m_readCspCache(true),
m_shaderFiles(3),
m_shaderQueryResult(3),
m_pointerLocationX(0.0f)
{
	// Register to be notified if the Device is lost or recreated
	m_deviceResources->RegisterDeviceNotify(this);

	m_sceneRenderer = std::unique_ptr<KinectRender>(new KinectRender(m_deviceResources));

	m_fpsTextRenderer = std::unique_ptr<SampleFpsTextRenderer>(new SampleFpsTextRenderer(m_deviceResources));

	m_pixelStore = ref new Platform::Array<byte>(1920 * 1080 * 4);

	m_kinectHandler = ref new KinectHandler();
	
	m_cspCache = ref new Platform::Collections::Vector< Platform::Object^ >();

	// TODO: Change the timer settings if you want something other than the default variable timestep mode.
	// e.g. for 60 FPS fixed timestep update logic, call:
	
	m_timer.SetFixedTimeStep(true);
	m_timer.SetTargetElapsedSeconds(1.0 / 24);
	
}

KinectRecordMain::~KinectRecordMain()
{
	// Deregister device notification
	m_deviceResources->RegisterDeviceNotify(nullptr);
}

// Updates application state when the window size changes (e.g. device orientation change)
void KinectRecordMain::CreateWindowSizeDependentResources()
{
	// TODO: Replace this with the size-dependent initialization of your app's content.
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
	m_takeFolder = nullptr;

	// reset frame count
	m_currentFrame = 1;

	// TODO: fix nutso file naming gymnastics
	std::wstringstream padTake;
	padTake << std::setfill(L'0') << std::setw(3) << m_currentTake;
	std::wstring fname(L"CAMCORDER_TAKE_000");
	fname.replace(16, 3, padTake.str().c_str());
	Platform::String^ fNameForWin = ref new Platform::String(fname.c_str());
	m_currentTake++;

	create_task(m_sessionFolder->CreateFolderAsync(fNameForWin)).then([this](Windows::Storage::StorageFolder^ folder)
	{
		m_takeFolder = folder;
	});
}

void KinectRecordMain::PrepToPlayback()
{
	// playback from frame 1
	m_currentFrame = 1;

	if (m_takeFolder != nullptr) {
		Platform::Collections::Vector<Platform::String^>^ fileTypes = ref new Platform::Collections::Vector<Platform::String^>();
		fileTypes->Append(".adv");
		QueryOptions^ queryOptions = ref new QueryOptions(CommonFileQuery::OrderByName, fileTypes);
		StorageFileQueryResult^ queryResult = m_takeFolder->CreateFileQueryWithOptions(queryOptions);
		create_task(queryResult->GetFilesAsync()).then([this](IVectorView<Windows::Storage::StorageFile^>^ files)
		{
			m_recordFiles = files;
			m_cspCache = ref new Platform::Collections::Vector<Platform::Object^>(m_recordFiles->Size);
			for (int frame = 1; frame < 100; ++frame) {
				// XXX - probably buffer (as in the video streaming sense) frame data here and playback at steady speed
				if ((m_recordFiles != nullptr) && (m_recordFiles->Size > frame)) {
					CacheFrameForPlayback(frame);
				}
			}
			//PlaybackSequence(1);
		});
	}
}

void KinectRecordMain::WriteFrameToDisk(Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ cameraSpacePoints)
{
	// XXX - nutso file naming gymnastics
	std::wstringstream padFrame;
	padFrame << std::setfill(L'0') << std::setw(5) << m_currentFrame;
	std::wstring fname(L"KinectStreamDepth_00000.adv");
	fname.replace(18, 5, padFrame.str().c_str());
	Platform::String^ fNameForWin = ref new Platform::String(fname.c_str());

	if (m_takeFolder != nullptr) {
		create_task(m_takeFolder->CreateFileAsync(fNameForWin)).then([this, cameraSpacePoints](Windows::Storage::StorageFile^ file)
		{
			std::vector<byte> bytesV(reinterpret_cast<byte*>(cameraSpacePoints->begin()), reinterpret_cast<byte*>(cameraSpacePoints->end()));
			Platform::Array<byte>^ bytes = ref new Platform::Array<byte>(&bytesV[0], bytesV.size());
			Windows::Storage::FileIO::WriteBytesAsync(file, bytes);
		});

		// advance to the next frame
		m_currentFrame++;

	}
}

void KinectRecordMain::WriteUVToDisk(Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>^ colorSpacePoints)
{
	// XXX - nutso file naming gymnastics
	std::wstringstream padFrame;
	padFrame << std::setfill(L'0') << std::setw(5) << m_currentFrame;
	std::wstring fname(L"KinectStreamUV_00000.auv");
	fname.replace(15, 5, padFrame.str().c_str());
	Platform::String^ fNameForWin = ref new Platform::String(fname.c_str());

	if (m_takeFolder != nullptr) {
		create_task(m_takeFolder->CreateFileAsync(fNameForWin)).then([this, colorSpacePoints](Windows::Storage::StorageFile^ file)
		{
			std::vector<byte> bytesV(reinterpret_cast<byte*>(colorSpacePoints->begin()), reinterpret_cast<byte*>(colorSpacePoints->end()));
			Platform::Array<byte>^ bytes = ref new Platform::Array<byte>(&bytesV[0], bytesV.size());
			Windows::Storage::FileIO::WriteBytesAsync(file, bytes);
		});
	}
}

void KinectRecordMain::WriteJpegToDisk(Windows::Storage::Streams::Buffer^ colorData)
{
	// XXX - nutso file naming gymnastics
	std::wstringstream padFrame;
	padFrame << std::setfill(L'0') << std::setw(5) << m_currentFrame;
	std::wstring fname(L"KinectPic_00000.jpg");
	fname.replace(10, 5, padFrame.str().c_str());
	Platform::String^ fNameForWin = ref new Platform::String(fname.c_str());

	create_task(m_takeFolder->CreateFileAsync(fNameForWin)).then([this, colorData](Windows::Storage::StorageFile^ file)
	{
		create_task(file->OpenAsync( FileAccessMode::ReadWrite)).then([this, colorData](IRandomAccessStream^ stream)
		{	
			create_task(BitmapEncoder::CreateAsync(BitmapEncoder::JpegEncoderId, stream)).then([this, colorData](BitmapEncoder^ encoder) {
				DataReader^ reader = DataReader::FromBuffer(colorData);
				reader->ReadBytes(m_pixelStore);
				encoder->SetPixelData(BitmapPixelFormat::Rgba8, BitmapAlphaMode::Ignore, 1920, 1080, 96.0, 96.0, m_pixelStore);
				encoder->FlushAsync(); 
			});
		});
	});
}

void KinectRecordMain::PlaybackSequence(int frame)
{
	// XXX - probably buffer (as in the video streaming sense) frame data here and playback at steady speed
	if ((m_recordFiles != nullptr) && (m_recordFiles->Size > frame)) {
		StorageFile^ frameFile = m_recordFiles->GetAt(frame);
		create_task(FileIO::ReadBufferAsync(frameFile)).then([this, frame](IBuffer^ buffer) {
			Platform::Array<unsigned char>^ bytes = ref new Platform::Array<unsigned char>(buffer->Length);
			CryptographicBuffer::CopyToByteArray(buffer, &bytes);
			Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ csps =
				ref new Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>(reinterpret_cast<WindowsPreview::Kinect::CameraSpacePoint *>(bytes->begin()), bytes->Length / 12.0);
			m_cspCache->SetAt(frame - 1, csps);
		}).then([this, frame]() { this->PlaybackSequence(frame + 1); });
	}
}

void KinectRecordMain::CacheFrameForPlayback(int frame)
{
	StorageFile^ frameFile = m_recordFiles->GetAt(frame);
	create_task(FileIO::ReadBufferAsync(frameFile)).then([this, frame](IBuffer^ buffer) {
		Platform::Array<unsigned char>^ bytes = ref new Platform::Array<unsigned char>(buffer->Length);
		CryptographicBuffer::CopyToByteArray(buffer, &bytes);
		Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ csps =
			ref new Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>(reinterpret_cast<WindowsPreview::Kinect::CameraSpacePoint *>(bytes->begin()), bytes->Length / 12.0);
		m_cspCache->SetAt(frame - 1, csps);
	});
}

void KinectRecordMain::ExportFrameToObj(Windows::Storage::StorageFolder^ exportFolder, Platform::String^ frameName, Windows::Storage::Streams::IBuffer^ frameData)
{
	// XXX - nutso file naming gymnastics
	std::wstring fname(frameName->Data());
	fname.replace(0, 17, L"Frame");
	fname.replace(12, 3, L"obj");
	Platform::String^ fNameForWin = ref new Platform::String(fname.c_str());

	create_task(exportFolder->CreateFileAsync(fNameForWin)).then([this, frameData](Windows::Storage::StorageFile^ objFile) {
		Platform::Array<unsigned char>^ bytes = ref new Platform::Array<unsigned char>(frameData->Length);
		CryptographicBuffer::CopyToByteArray(frameData, &bytes);
		Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ csps =
			ref new Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>(reinterpret_cast<WindowsPreview::Kinect::CameraSpacePoint *>(bytes->begin()), bytes->Length / 12.0);
		Platform::Collections::Vector<Platform::String ^>^ lines = ref new Platform::Collections::Vector<Platform::String^>();
		for (int i = 0; i < csps->Length; ++i) {
			std::wstring wline;
			if (std::isinf(csps[i].X)) {
				wline = L"v 0 0 0";
			}
			else {
				wline = std::wstring(L"v " + std::to_wstring(csps[i].X) + L" " + std::to_wstring(csps[i].Y) + L" " + std::to_wstring(csps[i].Z));
			}
			Platform::String^ line = ref new Platform::String(wline.c_str());
			lines->Append(line);
		}
		Windows::Storage::FileIO::WriteLinesAsync(objFile, lines);
		
		// this should only get incremented here
		m_currentExportFrame++;
		if (m_currentExportFrame < m_exportFiles->Size) {
			Windows::Storage::StorageFile^ frameFile = m_exportFiles->GetAt(m_currentExportFrame);
			create_task(Windows::Storage::FileIO::ReadBufferAsync(frameFile)).then([this, frameFile](Windows::Storage::Streams::IBuffer^ buffer) {
				ExportFrameToObj(this->m_exportFromFolder, frameFile->Name, buffer);
			});
		}
		else {
			m_isExporting = false;
		}
	});
}

void KinectRecordMain::ExportTakeToObj()
{
	if (m_exportFromFolder != nullptr) {

		m_isExporting = true;

		Platform::Collections::Vector<Platform::String^>^ fileTypes = ref new Platform::Collections::Vector<Platform::String^>();
		fileTypes->Append(".adv");
		QueryOptions^ queryOptions = ref new QueryOptions(CommonFileQuery::OrderByName, fileTypes);
		StorageFileQueryResult^ queryResult = m_exportFromFolder->CreateFileQueryWithOptions(queryOptions);
		create_task(queryResult->GetFilesAsync()).then([this](IVectorView<Windows::Storage::StorageFile^>^ files)
		{
			m_currentExportFrame = 0;
			m_exportFiles = files;
			Windows::Storage::StorageFile^ frameFile = files->GetAt(m_currentExportFrame);
			create_task(Windows::Storage::FileIO::ReadBufferAsync(frameFile)).then([this, frameFile](Windows::Storage::Streams::IBuffer^ buffer) {
				ExportFrameToObj(this->m_exportFromFolder, frameFile->Name, buffer);
			});
		}); 
	}
}

void KinectRecordMain::StreamColor(bool streamColor)
{
	m_streamColor = streamColor;
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
		m_sceneRenderer->Update(m_timer);

		// live feed from kinect
		if (m_kinectHandler->HasUnreadDepthData() && !m_isPlayingBack && !m_isRecording && !m_isExporting) {

			// DirectX rendering
			m_sceneRenderer->UpdateVertexBuffer(m_kinectHandler->Get3DData());
			m_sceneRenderer->UpdateTime(-1);
			if (m_streamColor) {
				m_sceneRenderer->UpdateColorBuffer(m_kinectHandler->GetColorData(), m_kinectHandler->GetUVData());
			}
		}
		// stream data to disk during recording
		else if (m_isRecording) {
			WriteFrameToDisk(m_kinectHandler->Get3DData());
			if (m_streamColor) {
				WriteUVToDisk(m_kinectHandler->GetUVData());
				WriteJpegToDisk(m_kinectHandler->GetColorData());
			}
		}
		// playback
		else if (m_isPlayingBack) {
			if (m_cspCache->Size >= m_currentFrame) {
				Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ csps = safe_cast<Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^>(m_cspCache->GetAt(m_currentFrame - 1));
				if (csps != nullptr) {
					m_sceneRenderer->UpdateVertexBuffer(csps);
					m_sceneRenderer->UpdateTime(m_currentFrame);
					m_cspCache->SetAt(m_currentFrame - 1, nullptr);
					CacheFrameForPlayback((m_currentFrame + 98) % (m_cspCache->Size - 1) + 1);
					m_currentFrame = m_currentFrame == (m_recordFiles->Size - 1) ? 1 : m_currentFrame + 1;
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

//	auto context = m_deviceResources->GetD3DDeviceContext();
//
//	// Reset the viewport to target the whole screen.
//	auto viewport = m_deviceResources->GetScreenViewport();
//	context->RSSetViewports(1, &viewport);
//
//	// Reset render targets to the screen.
//	ID3D11RenderTargetView *const targets[1] = { m_deviceResources->GetBackBufferRenderTargetView() };
//	context->OMSetRenderTargets(1, targets, m_deviceResources->GetDepthStencilView());
//
//	// Clear the back buffer and depth stencil view.
//	//context->ClearRenderTargetView(m_deviceResources->GetBackBufferRenderTargetView(), DirectX::Colors::Black);
//	//context->ClearDepthStencilView(m_sceneRenderer->GetShadowDepthView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
//	//context->ClearDepthStencilView(m_deviceResources->GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
//
//	//// Clear the back buffer and depth stencil view.
//	/*context->ClearRenderTargetView(m_deviceResources->GetBackBufferRenderTargetView(), DirectX::Colors::Black);
//	context->ClearDepthStencilView(m_deviceResources->GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
//*/
//	// Render the scene objects.
//	// TODO: Replace this with your app's content rendering functions.
//	//m_sceneRenderer->RenderShadowMap();
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
