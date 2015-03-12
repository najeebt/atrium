#pragma once

#define KINECT_DEPTH_WIDTH 512
#define KINECT_DEPTH_HEIGHT 424
#define KINECT_COLOR_WIDTH 1920
#define KINECT_COLOR_HEIGHT 1080
#define DEPTH_PIXEL_COUNT 512*424
#define COLOR_PIXEL_COUNT 1920*1080

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
	Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ Get3DData();
	Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>^ GetUVData();
	Windows::Storage::Streams::Buffer^ GetColorData();
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

