#pragma once

namespace Kinect = WindowsPreview::Kinect;
using namespace Windows::Foundation;

ref class ColorFrameCache sealed
{
public:
	ColorFrameCache();

	property Windows::Foundation::TimeSpan nTime;
	property Windows::Storage::Streams::Buffer^ buffer;
};

