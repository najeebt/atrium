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
handUnread(false),
m_kinectSensor(),
depthFrameReader(),
depthFrameSource(),
colorFrameReader(),
colorFrameSource(),
bodyFrameReader(),
bodyFrameSource(),
coordinateMapper(),
bodies(),
hands(),
DepthOutFile(NULL),
UVOutFile(NULL)
{

	errno_t err;

	m_cameraSpacePoints = ref new Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>(DEPTH_PIXEL_COUNT);
	m_colorSpacePoints = ref new Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>(DEPTH_PIXEL_COUNT);
	m_depthData = ref new Platform::Array<uint16>(DEPTH_PIXEL_COUNT);

	m_colorBuffer = ref new Windows::Storage::Streams::Buffer(1920 * 1080 * sizeof(unsigned char) * 4);

	hands = ref new Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>(2);

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
	if (m_kinectSensor)
	{
		m_kinectSensor->Close();
	}
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
	if (depthFrameReader == nullptr)
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

void KinectHandler::BodyReader_FrameArrived(Kinect::BodyFrameReader^ sender, Kinect::BodyFrameArrivedEventArgs^ e)
{

	Kinect::BodyFrame^ pBodyFrame = e->FrameReference->AcquireFrame();

	if (pBodyFrame != nullptr)
	{
		
		pBodyFrame->GetAndRefreshBodyData(bodies);

		for (int i = 0; i < pBodyFrame->BodyCount; i++)
		{
			Kinect::Body^ body = bodies->GetAt(i);
			if (body->IsTracked) {
				handUnread = true;
				Kinect::Joint rhand = body->Joints->Lookup(Kinect::JointType::HandTipRight);
				Kinect::Joint lhand = body->Joints->Lookup(Kinect::JointType::HandTipLeft);

				hands->set(0, rhand.Position);
				hands->set(1, lhand.Position);
			}
		}
	}

	return;
}

/// <summary>
/// Initializes the default Kinect sensor
/// </summary>
/// <returns>indicates success or failure</returns>
void KinectHandler::InitializeDefaultSensor()
{

	m_kinectSensor = Kinect::KinectSensor::GetDefault();

	if (!m_kinectSensor)
	{
		return;
	}

	m_kinectSensor->Open();

	depthFrameSource = m_kinectSensor->DepthFrameSource;
	depthFrameReader = depthFrameSource->OpenReader();
	depthFrameReader->FrameArrived += ref new Windows::Foundation::TypedEventHandler<WindowsPreview::Kinect::DepthFrameReader ^, WindowsPreview::Kinect::DepthFrameArrivedEventArgs ^>(this, &KinectHandler::DepthReader_FrameArrived);

	colorFrameSource = m_kinectSensor->ColorFrameSource;
	colorFrameReader = colorFrameSource->OpenReader();
	//colorFrameReader->FrameArrived += ref new Windows::Foundation::TypedEventHandler<WindowsPreview::Kinect::ColorFrameReader ^, WindowsPreview::Kinect::ColorFrameArrivedEventArgs ^>(this, &KinectHandler::ColorReader_FrameArrived);

	bodyFrameSource = m_kinectSensor->BodyFrameSource;
	bodies = ref new Platform::Collections::Vector<WindowsPreview::Kinect::Body^>(bodyFrameSource->BodyCount);
	bodyFrameReader = bodyFrameSource->OpenReader();
	//bodyFrameReader->FrameArrived += ref new Windows::Foundation::TypedEventHandler<WindowsPreview::Kinect::BodyFrameReader ^, WindowsPreview::Kinect::BodyFrameArrivedEventArgs ^>(this, &KinectHandler::BodyReader_FrameArrived);

	coordinateMapper = m_kinectSensor->CoordinateMapper;
}