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

//typedef struct DepthFrameCache {
//	Windows::Foundation::TimeSpan fTime;
//	Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ csp;
//};

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
	Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ GetCurrentDepthData();
	Windows::Storage::Streams::Buffer^ GetCurrentColorData();
	Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>^ GetCurrentUVData();
	Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ GetBufferedDepthData();
	Windows::Storage::Streams::Buffer^ GetBufferedColorData();
	Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>^ GetBufferedUVData();
	Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ GetHands() { handUnread = false; return hands; }
	bool HasUnreadDepthData() { return depthUnread; }
	bool HasUnreadColorData() { return colorUnread; }
	bool HasUnreadHandData() { return handUnread; }
	int GetNFrames() { return nFrames; }
	Windows::Foundation::Collections::IVectorView<WindowsPreview::Kinect::CameraSpacePoint>^ GetHDFacePoints();

private:

	~KinectHandler();

	// Recording infrastructure
	INT64                   startTime;
	INT64                   lastCounter;
	int					    nFrames;
	double                  freq;
	DWORD                   nNextStatusTime;
	DWORD                   nFramesSinceUpdate;
	bool                    saveScreenshot;
	bool					depthUnread;
	bool					colorUnread;
	bool					handUnread;
	FILE*					DepthOutFile;
	FILE*					UVOutFile;

	int nextDFrameToRead;
	int nextCFrameToRead;
	int nCFrames;
	int nDFrames;

	// Current Kinect
	WindowsPreview::Kinect::KinectSensor^ m_kinectSensor;

	WindowsPreview::Kinect::DepthFrameSource^ depthFrameSource;
	WindowsPreview::Kinect::DepthFrameReader^ depthFrameReader;

	WindowsPreview::Kinect::ColorFrameSource^ colorFrameSource;
	WindowsPreview::Kinect::ColorFrameReader^ colorFrameReader;
	
	WindowsPreview::Kinect::BodyFrameSource^ bodyFrameSource;
	WindowsPreview::Kinect::BodyFrameReader^ bodyFrameReader;

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
	WindowsPreview::Kinect::Body^ m_currentTrackedBody;

	/// <summary>
	/// The currently tracked body Id
	/// </summary>
	UINT64 m_currentTrackingId;

	// Coordinate mapper
	WindowsPreview::Kinect::CoordinateMapper^	coordinateMapper;

	// storage for depth data
	Windows::Storage::Streams::Buffer^ pDepthBuffer;
	Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ m_cameraSpacePoints;
	Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>^ m_colorSpacePoints;
	Platform::Array<WindowsPreview::Kinect::DepthSpacePoint>^ depthSpacePoints;
	Platform::Array<uint16>^ m_depthData;
	Windows::Storage::Streams::Buffer^ m_colorBuffer;

	// holding onto data until it's written
	Platform::Collections::Vector< Platform::Object^ >^ m_cspCache;
	Platform::Collections::Vector< Platform::Object^ >^ m_colorBufferCache;
	Platform::Collections::Vector< Platform::Object^ >^ m_depthDataCache;
	
	Platform::Collections::Vector<WindowsPreview::Kinect::Body ^>^ bodies;
	Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ hands;

	void DepthReader_FrameArrived(WindowsPreview::Kinect::DepthFrameReader^ sender, WindowsPreview::Kinect::DepthFrameArrivedEventArgs^ e);
	void ColorReader_FrameArrived(WindowsPreview::Kinect::ColorFrameReader^ sender, WindowsPreview::Kinect::ColorFrameArrivedEventArgs^ e);
	void BodyReader_FrameArrived(WindowsPreview::Kinect::BodyFrameReader^ sender, WindowsPreview::Kinect::BodyFrameArrivedEventArgs^ e);
	void HDFaceReader_FrameArrived(HighDefinitionFaceFrameReader^ sender, HighDefinitionFaceFrameArrivedEventArgs^ e);

	WindowsPreview::Kinect::Body^ KinectHandler::FindBodyWithTrackingId(UINT64 trackingId);
	WindowsPreview::Kinect::Body^ KinectHandler::FindClosestBody();
	double KinectHandler::VectorLength(WindowsPreview::Kinect::CameraSpacePoint point);
};

