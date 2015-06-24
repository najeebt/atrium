#include "pch.h"
#include "KinectHandler.h"

//using namespace WindowsPreview::Kinect;

using namespace Windows::System;

KinectHandler::KinectHandler() :
depthUnread(false),
colorUnread(false),
handUnread(false),
streamColor(true),
kinectSensor(),
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
nDFrames(0),
nCFrames(0)
{

	errno_t err;

	// allocating containers -- these get updated per frame
	cameraSpacePoints = ref new Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>(DEPTH_PIXEL_COUNT);
	colorSpacePoints = ref new Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>(DEPTH_PIXEL_COUNT);
	depthFrameContainer = ref new Platform::Array<uint16>(DEPTH_PIXEL_COUNT);
	colorFrameContainer = ref new Windows::Storage::Streams::Buffer(COLOR_PIXEL_COUNT * sizeof(unsigned char) * 4);

	dTimes = ref new Platform::Array<Windows::Foundation::TimeSpan>(HANDLER_BUFFER);
	cTimes = ref new Platform::Array<Windows::Foundation::TimeSpan>(HANDLER_BUFFER);

	hands = ref new Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>(2);
}

KinectHandler::~KinectHandler()
{
	// close the Kinect Sensor
	if (kinectSensor)
	{
		kinectSensor->Close();
	}
}

Windows::Storage::Streams::Buffer^ KinectHandler::GetCurrentColorData()
{
	colorUnread = false;
	return colorFrameContainer;
}

Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ KinectHandler::GetCurrentDepthData()
{
	depthUnread = false;
	return cameraSpacePoints;
}

uint64 KinectHandler::GetCurrentCTime()
{
	return cTimes->get(latestCFrame).Duration;
}

uint64 KinectHandler::GetCurrentDTime()
{
	return dTimes->get(latestDFrame).Duration;
}

Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>^ KinectHandler::GetCurrentUVData()
{
	coordinateMapper->MapDepthFrameToColorSpace(depthFrameContainer, colorSpacePoints);
	return colorSpacePoints;
}

void KinectHandler::SetStreamColor(bool newStreamColor)
{
	streamColor = newStreamColor;
}

/// <summary>
/// The big kahuna of incoming Kinect data handling, per-frame
/// </summary>
void KinectHandler::MultiSource_FrameArrived(WindowsPreview::Kinect::MultiSourceFrameReader^ sender, WindowsPreview::Kinect::MultiSourceFrameArrivedEventArgs^ e)
{
	auto multiRef = e->FrameReference;

	{
		auto frame = multiRef->AcquireFrame();

		if (frame == nullptr) return;

		auto pColorFrame = frame->ColorFrameReference->AcquireFrame();

		if (pColorFrame != nullptr && streamColor)
		{
			auto nTime = pColorFrame->RelativeTime;

			// each frame comes with a frame description which contains metadata about the frame size,
			// but Atrium is, for the moment, optimized for the Kinect sensor v2.
			// In example Kinect code, you would read metadata on frame dimensions here.

			latestCFrame = nCFrames % HANDLER_BUFFER;

			pColorFrame->CopyConvertedFrameDataToBuffer(colorFrameContainer, WindowsPreview::Kinect::ColorImageFormat::Rgba);

			cTimes->set(latestCFrame, nTime);

			nCFrames++;
			colorUnread = true;
		}

		auto pDepthFrame = frame->DepthFrameReference->AcquireFrame();

		if (pDepthFrame != nullptr) {

			auto nTime = pDepthFrame->RelativeTime;

			// each frame comes with a frame description which contains metadata about the frame size,
			// but Atrium is, for the moment, optimized for the Kinect sensor v2.
			// In example Kinect code, you would read metadata on frame dimensions here.

			latestDFrame = nDFrames % HANDLER_BUFFER;

			// Copy depth data
			pDepthFrame->CopyFrameDataToArray(depthFrameContainer);
			coordinateMapper->MapDepthFrameToCameraSpace(depthFrameContainer, cameraSpacePoints);

			dTimes->set(latestDFrame, nTime);

			nDFrames++;
			depthUnread = true;
		}

		return;
	}
}

/// <summary>
/// Isolated depth event handler. Legacy. MultiSource_FrameArrived above now does this work.
/// </summary>
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

		// each frame comes with a frame description which contains metadata about the frame size,
		// but Atrium is, for the moment, optimized for the Kinect sensor v2.
		// In example Kinect code, you would see that metadata getting pulled from the frame here.

		latestDFrame = nDFrames % HANDLER_BUFFER;

		// Copy depth data
		pDepthFrame->CopyFrameDataToArray(depthFrameContainer);
		coordinateMapper->MapDepthFrameToCameraSpace(depthFrameContainer, cameraSpacePoints);

		dTimes->set(latestDFrame, nTime);

		nDFrames++;
		depthUnread = true;
	}

	return;
}

/// <summary>
/// Isolated depth event handler. Legacy. MultiSource_FrameArrived above now does this work.
/// </summary>
void KinectHandler::ColorReader_FrameArrived(Kinect::ColorFrameReader^ sender, Kinect::ColorFrameArrivedEventArgs^ e)
{

	Kinect::ColorFrame^ pColorFrame = e->FrameReference->AcquireFrame();

	if (pColorFrame != nullptr)
	{

		auto nTime = pColorFrame->RelativeTime;

		// each frame comes with a frame description which contains metadata about the frame size,
		// but Atrium is, for the moment, optimized for the Kinect sensor v2.
		// In example Kinect code, you would see that metadata getting pulled from the frame here.

		latestCFrame = nCFrames % HANDLER_BUFFER;

		pColorFrame->CopyConvertedFrameDataToBuffer(colorFrameContainer, WindowsPreview::Kinect::ColorImageFormat::Rgba);

		cTimes->set(latestCFrame, nTime);

		nCFrames++;
		colorUnread = true;
	}

	return;
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
/// Returns the length of a vector from origin
/// </summary>
double KinectHandler::VectorLength(WindowsPreview::Kinect::CameraSpacePoint point)
{
	auto result = pow(point.X, 2) + pow(point.Y, 2) + pow(point.Z, 2);

	result = sqrt(result);

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
	if (currentTrackedBody != nullptr)
	{
		currentTrackedBody = FindBodyWithTrackingId(currentTrackingId);

		if (currentTrackedBody != nullptr)
		{
			// We still see the person we're tracking, make no change.
			return;
		}
	}

	auto selectedBody = FindClosestBody();

	if (selectedBody == nullptr)
	{
		currentTrackedBody = nullptr;
		currentTrackingId = 0;

		return;
	}

	currentTrackedBody = selectedBody;
	auto trackingID = selectedBody->TrackingId;;
	currentTrackingId = trackingID;

	HDFaceFrameSource->TrackingId = currentTrackingId;

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

	kinectSensor = Kinect::KinectSensor::GetDefault();

	if (!kinectSensor)
	{
		return;
	}

	//multiSourceFrameReader = kinectSensor->OpenMultiSourceFrameReader(WindowsPreview::Kinect::FrameSourceTypes::Depth | WindowsPreview::Kinect::FrameSourceTypes::Color);
	//multiSourceFrameReader->MultiSourceFrameArrived += ref new Windows::Foundation::TypedEventHandler<WindowsPreview::Kinect::MultiSourceFrameReader^, WindowsPreview::Kinect::MultiSourceFrameArrivedEventArgs ^>(this, &KinectHandler::MultiSource_FrameArrived);

	depthFrameSource = kinectSensor->DepthFrameSource;
	depthFrameReader = depthFrameSource->OpenReader();
	depthFrameReader->FrameArrived += ref new TypedEventHandler<WindowsPreview::Kinect::DepthFrameReader ^, WindowsPreview::Kinect::DepthFrameArrivedEventArgs ^>(this, &KinectHandler::DepthReader_FrameArrived);

	/*currentFaceModel = ref new FaceModel();
	currentFaceAlignment = ref new FaceAlignment();
	cachedFaceIndices = FaceModel::TriangleIndices;
*/
	coordinateMapper = kinectSensor->CoordinateMapper;

	kinectSensor->Open();
}