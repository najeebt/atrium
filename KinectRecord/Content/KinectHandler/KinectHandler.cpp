#include "pch.h"
#include "KinectHandler.h"

//using namespace WindowsPreview::Kinect;

using namespace Windows::System;

KinectHandler::KinectHandler() :
startTime(0),
lastCounter(0),
nFramesSinceUpdate(0),
nFrames(1),
freq(0),
nNextStatusTime(0),
saveScreenshot(false),
depthUnread(false),
colorUnread(false),
kinectSensor(),
depthFrameReader(),
depthFrameSource(),
colorFrameReader(),
colorFrameSource(),
coordinateMapper(),
DepthOutFile(NULL),
UVOutFile(NULL)
{
	
	errno_t err;

	m_cameraSpacePoints = ref new Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>(DEPTH_PIXEL_COUNT);
	m_colorSpacePoints = ref new Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>(DEPTH_PIXEL_COUNT);
	m_depthData = ref new Platform::Array<uint16>(DEPTH_PIXEL_COUNT);

	m_colorBuffer = ref new Windows::Storage::Streams::Buffer(1920*1080 * sizeof(unsigned char) * 4);


	depthSpacePoints = ref new Platform::Array<WindowsPreview::Kinect::DepthSpacePoint>(DEPTH_PIXEL_COUNT);
	for (int i = 0; i < KINECT_DEPTH_HEIGHT; ++i) {
		for (int j = 0; j < KINECT_DEPTH_WIDTH; ++j) {
			depthSpacePoints[i * KINECT_DEPTH_WIDTH + j].X = i;
			depthSpacePoints[i * KINECT_DEPTH_WIDTH + j].Y = j;
		}
	}

}

KinectHandler::~KinectHandler()
{
	// close the Kinect Sensor
	if (kinectSensor)
	{
		kinectSensor->Close();
	}
}

bool KinectHandler::HasUnreadDepthData()
{
	return depthUnread;
}

bool KinectHandler::HasUnreadColorData()
{
	return colorUnread;
}

Windows::Storage::Streams::Buffer^ KinectHandler::GetColorData()
{
	colorUnread = false;
	return m_colorBuffer;
}

Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ KinectHandler::Get3DData()
{
	depthUnread = false;
	return m_cameraSpacePoints;
}

Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>^ KinectHandler::GetUVData()
{
	coordinateMapper->MapDepthFrameToColorSpace(m_depthData, m_colorSpacePoints);
	return m_colorSpacePoints;
}

void KinectHandler::DepthReader_FrameArrived(Kinect::DepthFrameReader^ sender, Kinect::DepthFrameArrivedEventArgs^ e)
{
	if (this->depthFrameReader == nullptr)
	{
		return;
	}

	Kinect::DepthFrame^ pDepthFrame = e->FrameReference->AcquireFrame();

	if (pDepthFrame != nullptr)
	{
		auto nTime = pDepthFrame->RelativeTime;
		 
		auto pDepthFrameDescription = pDepthFrame->FrameDescription;
		auto nDepthWidth = pDepthFrameDescription->Width;
		auto nDepthHeight = pDepthFrameDescription->Height;
		auto nDepthMinReliableDistance = pDepthFrame->DepthMinReliableDistance;
		auto nDepthMaxReliableDistance = pDepthFrame->DepthMaxReliableDistance;

		// Copy depth data
		pDepthFrame->CopyFrameDataToArray(m_depthData);
		coordinateMapper->MapDepthFrameToCameraSpace(m_depthData, m_cameraSpacePoints);
		//coordinateMapper->MapDepthPointsToColorSpace(depthSpacePoints, m_depthData, m_colorSpacePoints);
		
		nFrames++;
		depthUnread = true;
	}

	return;
}

void KinectHandler::ColorReader_FrameArrived(Kinect::ColorFrameReader^ sender, Kinect::ColorFrameArrivedEventArgs^ e)
{
	
	Kinect::ColorFrame^ pColorFrame = e->FrameReference->AcquireFrame();

	if (pColorFrame != nullptr)
	{

		// get color frame data
		auto pColorFrameDescription = pColorFrame->FrameDescription;
		auto nColorWidth = pColorFrameDescription->Width;
		auto nColorHeight = pColorFrameDescription->Height;
		auto imageFormat = pColorFrame->RawColorImageFormat;

		pColorFrame->CopyConvertedFrameDataToBuffer(m_colorBuffer, WindowsPreview::Kinect::ColorImageFormat::Rgba);

		colorUnread = true;
	}

	return;
}

/// <summary>
/// Initializes the default Kinect sensor
/// </summary>
/// <returns>indicates success or failure</returns>
void KinectHandler::InitializeDefaultSensor()
{

	this->kinectSensor = Kinect::KinectSensor::GetDefault();

	if (!this->kinectSensor)
	{
		return;
	}

	if (kinectSensor)
	{
		this->kinectSensor->Open();

		if (this->kinectSensor != nullptr)
		{
			this->depthFrameSource = this->kinectSensor->DepthFrameSource;
			this->depthFrameReader = this->depthFrameSource->OpenReader();
			this->depthFrameReader->FrameArrived += ref new Windows::Foundation::TypedEventHandler<WindowsPreview::Kinect::DepthFrameReader ^, WindowsPreview::Kinect::DepthFrameArrivedEventArgs ^>(this, &KinectHandler::DepthReader_FrameArrived);
		}

		if (this->kinectSensor != nullptr)
		{
			this->colorFrameSource = this->kinectSensor->ColorFrameSource;
			this->colorFrameReader = this->colorFrameSource->OpenReader();
			this->colorFrameReader->FrameArrived += ref new Windows::Foundation::TypedEventHandler<WindowsPreview::Kinect::ColorFrameReader ^, WindowsPreview::Kinect::ColorFrameArrivedEventArgs ^>(this, &KinectHandler::ColorReader_FrameArrived);
		}

		this->coordinateMapper = kinectSensor->CoordinateMapper;
	}
}

/// <summary>
/// Handle new depth data
/// <param name="nTime">timestamp of frame</param>
/// <param name="pBuffer">pointer to frame data</param>
/// <param name="nWidth">width (in pixels) of input image data</param>
/// <param name="nHeight">height (in pixels) of input image data</param>
/// <param name="nMinDepth">minimum reliable depth</param>
/// <param name="nMaxDepth">maximum reliable depth</param>
/// </summary>
void KinectHandler::ProcessFrames(INT64 nTime,
	const UINT16* pDepthBuffer, int nDepthWidth,
	int nDepthHeight, USHORT nMinDepth, USHORT nMaxDepth,
	RGBQUAD* pColorBuffer, int nColorWidth, int nColorHeight)
{
	if (0)
	{
		if (!startTime)
		{
			startTime = nTime;
		}

		double fps = 0.0;

		LARGE_INTEGER qpcNow = { 0 };
		if (freq)
		{
			if (QueryPerformanceCounter(&qpcNow))
			{
				if (lastCounter)
				{
					nFramesSinceUpdate++;
					fps = freq * nFramesSinceUpdate / double(qpcNow.QuadPart - lastCounter);
				}
			}
		}

		/*
		WCHAR szStatusMessage[64];
		StringCchPrintf(szStatusMessage, _countof(szStatusMessage), L" FPS = %0.2f    Time = %I64d", fps, (nTime - nStartTime));

		if (SetStatusMessage(szStatusMessage, 1000, false))
		{
			lastCounter = qpcNow.QuadPart;
			nFramesSinceUpdate = 0;
		}
		*/
	}

	// Make sure we've received valid data
	// XXX - right now we're doing no such tests for color data
	if (pDepthBuffer && (nDepthWidth == KINECT_DEPTH_WIDTH) && (nDepthHeight == KINECT_DEPTH_HEIGHT))
	{
		// end pixel is start + width*height - 1
		const UINT16* pBufferEnd = pDepthBuffer + (nDepthWidth * nDepthHeight);

		// map 3D data into appropriate spaces before saving
		// XXX - probably computationally expensive but necessary ftm, we can't access
		// sensor-correct mapping offline (unless we store sensor profile?)
		//pCoordinateMapper->MapDepthFrameToCameraSpace(DEPTH_PIXEL_COUNT, pDepthBuffer, DEPTH_PIXEL_COUNT, CameraSpacePoints);
		//pCoordinateMapper->MapDepthFrameToColorSpace(DEPTH_PIXEL_COUNT, pDepthBuffer, DEPTH_PIXEL_COUNT, ColorSpacePoints);

		// write data to disk
		// XXX - probably need to add error checks on count
		//int count = fwrite(reinterpret_cast<const void *>(CameraSpacePoints), sizeof(CameraSpacePoint), DEPTH_PIXEL_COUNT, DepthOutFile);
		//count = fwrite(reinterpret_cast<const void *>(ColorSpacePoints), sizeof(ColorSpacePoint), DEPTH_PIXEL_COUNT, UVOutFile);

		while (0) //(pBuffer < pBufferEnd)
		{
			USHORT depth = *pDepthBuffer;

			// To convert to a byte, we're discarding the most-significant
			// rather than least-significant bits.
			// We're preserving detail, although the intensity will "wrap."
			// Values outside the reliable depth range are mapped to 0 (black).

			// Note: Using conditionals in this loop could degrade performance.
			// Consider using a lookup table instead when writing production code.

			BYTE intensity = static_cast<BYTE>((depth >= nMinDepth) && (depth <= nMaxDepth) ? (depth) : 0);

			/*
			pRGBX->rgbRed = intensity;
			pRGBX->rgbGreen = intensity;
			pRGBX->rgbBlue = intensity;

			++pRGBX;
			++pBuffer;
			*/

		}

		// Draw the data with Direct2D
		//pDrawDepth->Draw(reinterpret_cast<BYTE*>(pDepthRGBX), cDepthWidth * cDepthHeight * sizeof(RGBQUAD));

		// save color frames to disk
		// XXX - add UI control to turn this off?
		if (1)
		{
			//WCHAR szScreenshotPath[MAX_PATH];

			// Retrieve the path to My Photos
			//GetScreenshotFileName(szScreenshotPath, _countof(szScreenshotPath));

			// Write out the bitmap to disk
			//HRESULT hr = SaveBitmapToFile(reinterpret_cast<BYTE*>(pColorBuffer), nColorWidth, nColorHeight, sizeof(RGBQUAD) * 8, szScreenshotPath);
		}

		nFrames++;
	}
}
