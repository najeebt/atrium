#pragma once

namespace Kinect = WindowsPreview::Kinect;
using namespace Windows::Foundation;

ref class DepthFrameCache sealed
{
public:
	DepthFrameCache();

	property Windows::Foundation::TimeSpan nTime;
	property Platform::Array<WindowsPreview::Kinect::CameraSpacePoint>^ csps;
};

