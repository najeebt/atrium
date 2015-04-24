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
HDFaceFrameReader(),
HDFaceFrameSource(),
coordinateMapper(),
bodies(),
hands(),
DepthOutFile(NULL),
UVOutFile(NULL),
nextDFrameToRead(0),
nextCFrameToRead(0),
nDFrames(0),
nCFrames(0)
{

	errno_t err;

	m_cameraSpacePoints = ref new Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>(DEPTH_PIXEL_COUNT);
	m_colorSpacePoints = ref new Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>(DEPTH_PIXEL_COUNT);
	m_depthData = ref new Platform::Array<uint16>(DEPTH_PIXEL_COUNT);

	m_colorBuffer = ref new Windows::Storage::Streams::Buffer(1920 * 1080 * sizeof(unsigned char) * 4);

	m_cspCache = ref new Platform::Collections::Vector<DepthFrameCache^>(HANDLER_BUFFER);
	m_colorBufferCache = ref new Platform::Collections::Vector<ColorFrameCache^>(HANDLER_BUFFER);
	m_depthDataCache = ref new Platform::Collections::Vector<Platform::Object^>(HANDLER_BUFFER);

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

Windows::Storage::Streams::Buffer^ KinectHandler::GetCurrentColorData()
{
	colorUnread = false;
	return m_colorBuffer;
}

Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ KinectHandler::GetCurrentDepthData()
{
	depthUnread = false;
	return m_cameraSpacePoints;
}

DepthFrameCache^ KinectHandler::GetBufferedDepthData()
{
	depthUnread = false;
	DepthFrameCache^ ret = m_cspCache->GetAt(nextDFrameToRead);
	nextDFrameToRead++;
	nextDFrameToRead %= HANDLER_BUFFER;
	return ret;
}

ColorFrameCache^ KinectHandler::GetBufferedColorData()
{
	colorUnread = false;
	ColorFrameCache^ ret = m_colorBufferCache->GetAt(nextCFrameToRead);
	nextCFrameToRead++;
	nextCFrameToRead %= HANDLER_BUFFER;
	return ret;
}

Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>^ KinectHandler::GetBufferedUVData()
{
	Platform::Array<uint16>^ depthData = safe_cast<Platform::Array<uint16>^>(m_depthDataCache->GetAt(nextDFrameToRead));
	coordinateMapper->MapDepthFrameToColorSpace(depthData, m_colorSpacePoints);
	return m_colorSpacePoints;
}

Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>^ KinectHandler::GetCurrentUVData()
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
		Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ cameraSpacePts = ref new Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>(DEPTH_PIXEL_COUNT);
		Platform::Array<uint16>^ depthData = ref new Platform::Array<uint16>(DEPTH_PIXEL_COUNT);
		pDepthFrame->CopyFrameDataToArray(depthData);
		coordinateMapper->MapDepthFrameToCameraSpace(depthData, cameraSpacePts);

		m_cameraSpacePoints = cameraSpacePts;
		m_depthData = depthData;

		DepthFrameCache^ dfc = ref new DepthFrameCache();
		dfc->nTime = nTime;
		dfc->csps = cameraSpacePts;

		m_cspCache->SetAt(nDFrames % HANDLER_BUFFER, dfc);
		m_depthDataCache->SetAt(nDFrames % HANDLER_BUFFER, depthData);

		nDFrames++;
		depthUnread = true;
	}

	return;
}

void KinectHandler::ColorReader_FrameArrived(Kinect::ColorFrameReader^ sender, Kinect::ColorFrameArrivedEventArgs^ e)
{

	Kinect::ColorFrame^ pColorFrame = e->FrameReference->AcquireFrame();

	if (pColorFrame != nullptr)
	{

		auto nTime = pColorFrame->RelativeTime;

		// get color frame data
		auto pColorFrameDescription = pColorFrame->FrameDescription;
		auto nColorWidth = pColorFrameDescription->Width;
		auto nColorHeight = pColorFrameDescription->Height;
		auto imageFormat = pColorFrame->RawColorImageFormat;

		Windows::Storage::Streams::Buffer^ colorBuffer = ref new Windows::Storage::Streams::Buffer(1920 * 1080 * sizeof(unsigned char) * 4);;
		pColorFrame->CopyConvertedFrameDataToBuffer(colorBuffer, WindowsPreview::Kinect::ColorImageFormat::Rgba);

		m_colorBuffer = colorBuffer;

		ColorFrameCache^ cfc = ref new ColorFrameCache();
		cfc->buffer = colorBuffer;
		cfc->nTime = nTime;

		m_colorBufferCache->SetAt(nCFrames % HANDLER_BUFFER, cfc);

		nCFrames++;
		colorUnread = true;
	}

	return;
}

/// <summary>
/// Returns the length of a vector from origin
/// </summary>
double KinectHandler::VectorLength(WindowsPreview::Kinect::CameraSpacePoint point)
{
	auto result = pow(point.X, 2) + pow(point.Y, 2) + pow(point.Z, 2);

	result = sqrt(result);

	return result;
}

/// <summary>
/// Finds the closest body from the sensor if any
/// </summary>
WindowsPreview::Kinect::Body^ KinectHandler::FindClosestBody()
{
	WindowsPreview::Kinect::Body^ result = nullptr;

	double closestBodyDistance = 10000000.0;

	for (auto body : this->bodies)
	{
		if (body->IsTracked)
		{
			auto joints = body->Joints;

			auto currentLocation = joints->Lookup(Kinect::JointType::SpineBase).Position;

			auto currentDistance = VectorLength(currentLocation);

			if (result == nullptr || currentDistance < closestBodyDistance)
			{
				result = body;
				closestBodyDistance = currentDistance;
			}
		}
	}

	return result;
}

/// <summary>
/// Find if there is a body tracked with the given trackingId
/// </summary>
WindowsPreview::Kinect::Body^ KinectHandler::FindBodyWithTrackingId(UINT64 trackingId)
{
	WindowsPreview::Kinect::Body^ result = nullptr;

	for (auto body : this->bodies)
	{
		if (body->IsTracked)
		{
			if (body->TrackingId == trackingId)
			{
				result = body;
				break;
			}
		}
	}

	return result;
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

	// Do we still see the person we're tracking?
	if (m_currentTrackedBody != nullptr)
	{
		m_currentTrackedBody = FindBodyWithTrackingId(m_currentTrackingId);

		if (m_currentTrackedBody != nullptr)
		{
			// We still see the person we're tracking, make no change.
			return;
		}
	}

	WindowsPreview::Kinect::Body^ selectedBody = FindClosestBody();

	if (selectedBody == nullptr)
	{
		m_currentTrackedBody = nullptr;
		m_currentTrackingId = 0;

		return;
	}

	m_currentTrackedBody = selectedBody;
	auto trackingID = selectedBody->TrackingId;;
	m_currentTrackingId = trackingID;

	HDFaceFrameSource->TrackingId = m_currentTrackingId;

	return;
}

void KinectHandler::HDFaceReader_FrameArrived(HighDefinitionFaceFrameReader^ sender, HighDefinitionFaceFrameArrivedEventArgs^ e)
{
	auto frameReference = e->FrameReference;

	{
		auto frame = frameReference->AcquireFrame();

		// We might miss the chance to acquire the frame; it will be null if it's missed.
		// Also ignore this frame if face tracking failed.
		if (frame == nullptr || !frame->IsFaceTracked)
		{
			return;
		}

		frame->GetAndRefreshFaceAlignmentResult(currentFaceAlignment);
		auto faceVertices = this->currentFaceModel->CalculateVerticesForAlignment(this->currentFaceAlignment);
	}
}

Windows::Foundation::Collections::IVectorView<WindowsPreview::Kinect::CameraSpacePoint>^ KinectHandler::GetHDFacePoints()
{
	return currentFaceModel->CalculateVerticesForAlignment(this->currentFaceAlignment);
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

	depthFrameSource = m_kinectSensor->DepthFrameSource;
	depthFrameReader = depthFrameSource->OpenReader();
	depthFrameReader->FrameArrived += ref new TypedEventHandler<WindowsPreview::Kinect::DepthFrameReader ^, WindowsPreview::Kinect::DepthFrameArrivedEventArgs ^>(this, &KinectHandler::DepthReader_FrameArrived);

	colorFrameSource = m_kinectSensor->ColorFrameSource;
	colorFrameReader = colorFrameSource->OpenReader();
	colorFrameReader->FrameArrived += ref new Windows::Foundation::TypedEventHandler<WindowsPreview::Kinect::ColorFrameReader ^, WindowsPreview::Kinect::ColorFrameArrivedEventArgs ^>(this, &KinectHandler::ColorReader_FrameArrived);

	//bodyFrameSource = m_kinectSensor->BodyFrameSource;
	//bodies = ref new Platform::Collections::Vector<WindowsPreview::Kinect::Body^>(bodyFrameSource->BodyCount);
	//bodyFrameReader = bodyFrameSource->OpenReader();
	//bodyFrameReader->FrameArrived += ref new TypedEventHandler<WindowsPreview::Kinect::BodyFrameReader ^, WindowsPreview::Kinect::BodyFrameArrivedEventArgs ^>(this, &KinectHandler::BodyReader_FrameArrived);

	//HDFaceFrameSource = ref new HighDefinitionFaceFrameSource(m_kinectSensor);
	//HDFaceFrameReader = HDFaceFrameSource->OpenReader();
	//HDFaceFrameReader->FrameArrived += ref new TypedEventHandler<HighDefinitionFaceFrameReader^, HighDefinitionFaceFrameArrivedEventArgs^>(this, &KinectHandler::HDFaceReader_FrameArrived);

	currentFaceModel = ref new FaceModel();
	currentFaceAlignment = ref new FaceAlignment();
	cachedFaceIndices = FaceModel::TriangleIndices;

	coordinateMapper = m_kinectSensor->CoordinateMapper;

	m_kinectSensor->Open();
}