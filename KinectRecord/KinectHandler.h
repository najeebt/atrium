#pragma once

#define KINECT_DEPTH_WIDTH 512
#define KINECT_DEPTH_HEIGHT 424
#define KINECT_COLOR_WIDTH 1920
#define KINECT_COLOR_HEIGHT 1080
#define DEPTH_PIXEL_COUNT 512*424
#define COLOR_PIXEL_COUNT 1920*1080

namespace Kinect = WindowsPreview::Kinect;
using namespace Windows::Foundation;

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
	bool HasUnreadDepthData();
	bool HasUnreadColorData();
	int GetNFrames() { return nFrames; }

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
	FILE*					DepthOutFile;
	FILE*					UVOutFile;

	// Current Kinect
	WindowsPreview::Kinect::KinectSensor^ kinectSensor;

	WindowsPreview::Kinect::DepthFrameSource^ depthFrameSource;
	WindowsPreview::Kinect::DepthFrameReader^ depthFrameReader;

	WindowsPreview::Kinect::ColorFrameSource^ colorFrameSource;
	WindowsPreview::Kinect::ColorFrameReader^ colorFrameReader;

	// Coordinate mapper
	WindowsPreview::Kinect::CoordinateMapper^	coordinateMapper;

	// storage for depth data
	Windows::Storage::Streams::Buffer^ pDepthBuffer;
	Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ m_cameraSpacePoints;
	Platform::Array<WindowsPreview::Kinect::ColorSpacePoint>^ m_colorSpacePoints;
	Platform::Array<WindowsPreview::Kinect::DepthSpacePoint>^ depthSpacePoints;
	Platform::Array<uint16>^ m_depthData;
	Windows::Storage::Streams::Buffer^ m_colorBuffer;

	void KinectHandler::DepthReader_FrameArrived(WindowsPreview::Kinect::DepthFrameReader^ sender, WindowsPreview::Kinect::DepthFrameArrivedEventArgs^ e);
	void KinectHandler::ColorReader_FrameArrived(WindowsPreview::Kinect::ColorFrameReader^ sender, WindowsPreview::Kinect::ColorFrameArrivedEventArgs^ e);

	/// <summary>
	/// Handle new depth data
	/// <param name="nTime">timestamp of frame</param>
	/// <param name="pDepthBuffer">pointer to depth frame data</param>
	/// <param name="nDepthWidth">width (in pixels) of input depth image data</param>
	/// <param name="nDepthHeight">height (in pixels) of input depth image data</param>
	/// <param name="nMinDepth">minimum reliable depth</param>
	/// <param name="nMaxDepth">maximum reliable depth</param>
	/// <param name="pColorBuffer">pointer to color frame data</param>
	/// <param name="nColorWidth">width (in pixels) of input color image data</param>
	/// <param name="nColorHeight">height (in pixels) of input color image data</param>
	/// </summary>
	void                    ProcessFrames(INT64 nTime, const UINT16* pDepthBuffer, int nHeight, int nWidth, USHORT nMinDepth, USHORT nMaxDepth, RGBQUAD* pColorBuffer, int nColorWidth, int nColorHeight);

};

