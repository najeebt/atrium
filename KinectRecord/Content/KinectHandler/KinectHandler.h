#pragma once

#define KINECT_DEPTH_WIDTH 512
#define KINECT_DEPTH_HEIGHT 424
#define KINECT_COLOR_WIDTH 1920
#define KINECT_COLOR_HEIGHT 1080
#define DEPTH_PIXEL_COUNT 512*424
#define COLOR_PIXEL_COUNT 1920*1080
#define COLOR_WIDTH_MULT .000521
#define COLOR_HEIGHT_MULT .000926

#define HANDLER_BUFFER 30
#define BUFFER_MAX_SIZE 30

namespace Kinect = WindowsPreview::Kinect;
using namespace Windows::Foundation;
using namespace Microsoft::Kinect::Face;

// Safe release for interfaces
template<class Interface>
inline void SafeRelease(Interface *& pInterfaceToRelease)
{
	if (pInterfaceToRelease != NULL)
	{
		pInterfaceToRelease->Release();
		pInterfaceToRelease = NULL;
	}
}

ref class KinectHandler sealed
{
public:
	KinectHandler();

	void	KinectHandler::InitializeDefaultSensor();

	bool HasUnreadDepthData() { return depthUnread; }
	bool HasUnreadColorData() { return colorUnread; }
	bool HasUnreadHandData() { return handUnread; }

	Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ GetCurrentDepthData();
	Windows::Storage::Streams::Buffer^ GetCurrentColorData();
	Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>^ GetCurrentUVData();
	
	uint64 GetCurrentCTime();
	uint64 GetCurrentDTime();
	
	Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ GetHands() { handUnread = false; return hands; }

	Windows::Foundation::Collections::IVectorView<WindowsPreview::Kinect::CameraSpacePoint>^ GetHDFacePoints();

private:

	~KinectHandler();

	// Recording infrastructure
	bool					depthUnread;
	bool					colorUnread;
	bool					handUnread;
	
	int nCFrames;
	int nDFrames;
	
	Platform::Array<Windows::Foundation::TimeSpan>^ dTimes;
	Platform::Array<Windows::Foundation::TimeSpan>^ cTimes;

	int latestDFrame;
	int latestCFrame;

	// Primary Kinect object
	WindowsPreview::Kinect::KinectSensor^ kinectSensor;

	WindowsPreview::Kinect::DepthFrameSource^ depthFrameSource;
	WindowsPreview::Kinect::DepthFrameReader^ depthFrameReader;

	WindowsPreview::Kinect::ColorFrameSource^ colorFrameSource;
	WindowsPreview::Kinect::ColorFrameReader^ colorFrameReader;
	
	WindowsPreview::Kinect::BodyFrameSource^ bodyFrameSource;
	WindowsPreview::Kinect::BodyFrameReader^ bodyFrameReader;

	WindowsPreview::Kinect::MultiSourceFrameReader^ multiSourceFrameReader;

	Microsoft::Kinect::Face::HighDefinitionFaceFrameSource^ HDFaceFrameSource;
	Microsoft::Kinect::Face::HighDefinitionFaceFrameReader^ HDFaceFrameReader;

	/// <summary>
	/// FaceAlignment is the result of tracking a face, it has face animations location and orientation
	/// </summary>
	FaceAlignment^ currentFaceAlignment;

	/// <summary>
	/// FaceModel is a result of capturing a face
	/// </summary>
	FaceModel^ currentFaceModel;

	/// <summary>
	/// Indices don't change, save them one time is enough
	/// </summary>
	Windows::Foundation::Collections::IVectorView<UINT>^ cachedFaceIndices;

	/// <summary>
	/// The currently tracked body
	/// </summary>
	WindowsPreview::Kinect::Body^ currentTrackedBody;

	/// <summary>
	/// The currently tracked body Id
	/// </summary>
	UINT64 currentTrackingId;

	// Coordinate mapper
	WindowsPreview::Kinect::CoordinateMapper^	coordinateMapper;

	/// <summary>
	/// Pre-allocated container for incoming depth data.
	/// </summary>
	Platform::Array<uint16>^ depthFrameContainer;

	/// <summary>
	/// Pre-allocated container for incoming color data.
	/// </summary>
	Windows::Storage::Streams::Buffer^ colorFrameContainer;

	/// <summary>
	/// Up-to-date X, Y, Z points in meters per frame
	/// These are erased/updated with every new frame that comes in at 30fps (target)
	/// </summary>
	Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ cameraSpacePoints;

	/// <summary>
	/// Up-to-date mapping of depth points to their corresponding pixel in the color capture
	/// These are erased/updated with every new frame that comes in at 30fps (target)
	/// </summary>
	Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>^ colorSpacePoints;

	Platform::Collections::Vector<WindowsPreview::Kinect::Body ^>^ bodies;
	Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ hands;

	void MultiSource_FrameArrived(WindowsPreview::Kinect::MultiSourceFrameReader^ sender, WindowsPreview::Kinect::MultiSourceFrameArrivedEventArgs^ e);
	void DepthReader_FrameArrived(WindowsPreview::Kinect::DepthFrameReader^ sender, WindowsPreview::Kinect::DepthFrameArrivedEventArgs^ e);
	void ColorReader_FrameArrived(WindowsPreview::Kinect::ColorFrameReader^ sender, WindowsPreview::Kinect::ColorFrameArrivedEventArgs^ e);
	void BodyReader_FrameArrived(WindowsPreview::Kinect::BodyFrameReader^ sender, WindowsPreview::Kinect::BodyFrameArrivedEventArgs^ e);
	void HDFaceReader_FrameArrived(HighDefinitionFaceFrameReader^ sender, HighDefinitionFaceFrameArrivedEventArgs^ e);

	WindowsPreview::Kinect::Body^ KinectHandler::FindBodyWithTrackingId(UINT64 trackingId);
	WindowsPreview::Kinect::Body^ KinectHandler::FindClosestBody();
	double KinectHandler::VectorLength(WindowsPreview::Kinect::CameraSpacePoint point);
};

