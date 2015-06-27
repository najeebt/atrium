//
// DirectXPage.xaml.cpp
// Implementation of the DirectXPage class.
//

#include "pch.h"
#include "DirectXPage.xaml.h"

using namespace KinectRecord;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Graphics::Display;
using namespace Windows::System::Threading;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::Storage::Pickers;
using namespace Windows::Storage;
using namespace Windows::Storage::Search;
using namespace concurrency;


DirectXPage::DirectXPage():
	m_windowVisible(true),
	m_coreInput(nullptr)
{
	InitializeComponent();

	// Register event handlers for page lifecycle.
	CoreWindow^ window = Window::Current->CoreWindow;

	window->VisibilityChanged +=
		ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(this, &DirectXPage::OnVisibilityChanged);

	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

	currentDisplayInformation->DpiChanged +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &DirectXPage::OnDpiChanged);

	currentDisplayInformation->OrientationChanged +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &DirectXPage::OnOrientationChanged);

	DisplayInformation::DisplayContentsInvalidated +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &DirectXPage::OnDisplayContentsInvalidated);

	swapChainPanel->CompositionScaleChanged += 
		ref new TypedEventHandler<SwapChainPanel^, Object^>(this, &DirectXPage::OnCompositionScaleChanged);

	swapChainPanel->SizeChanged +=
		ref new SizeChangedEventHandler(this, &DirectXPage::OnSwapChainPanelSizeChanged);

	// Disable all pointer visual feedback for better performance when touching.
	auto pointerVisualizationSettings = PointerVisualizationSettings::GetForCurrentView();
	pointerVisualizationSettings->IsContactFeedbackEnabled = false; 
	pointerVisualizationSettings->IsBarrelButtonFeedbackEnabled = false;

	recStartTime = ref new Windows::Globalization::Calendar;
	recEndTime = ref new Windows::Globalization::Calendar;

	// At this point we have access to the device. 
	// We can create the device-dependent resources.
	m_deviceResources = std::make_shared<DX::DeviceResources>();
	m_deviceResources->SetSwapChainPanel(swapChainPanel);

	// Register our SwapChainPanel to get independent input pointer events
	auto workItemHandler = ref new WorkItemHandler([this] (IAsyncAction ^)
	{
		// The CoreIndependentInputSource will raise pointer events for the specified device types on whichever thread it's created on.
		m_coreInput = swapChainPanel->CreateCoreIndependentInputSource(
			Windows::UI::Core::CoreInputDeviceTypes::Mouse |
			Windows::UI::Core::CoreInputDeviceTypes::Touch |
			Windows::UI::Core::CoreInputDeviceTypes::Pen 
			);

		// Register for pointer events, which will be raised on the background thread.
		m_coreInput->PointerPressed += ref new TypedEventHandler<Object^, PointerEventArgs^>(this, &DirectXPage::OnPointerPressed);
		m_coreInput->PointerMoved += ref new TypedEventHandler<Object^, PointerEventArgs^>(this, &DirectXPage::OnPointerMoved);
		m_coreInput->PointerReleased += ref new TypedEventHandler<Object^, PointerEventArgs^>(this, &DirectXPage::OnPointerReleased);

		// Begin processing input messages as they're delivered.
		m_coreInput->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessUntilQuit);
	});

	// Run task on a dedicated high priority background thread.
	m_inputLoopWorker = ThreadPool::RunAsync(workItemHandler, WorkItemPriority::High, WorkItemOptions::TimeSliced);

	m_main = std::unique_ptr<KinectRecordMain>(new KinectRecordMain(m_deviceResources));
	m_main->StartRenderLoop();

	m_main->kinectHandler->InitializeDefaultSensor();
}

DirectXPage::~DirectXPage()
{
	// Stop rendering and processing events on destruction.
	m_main->StopRenderLoop();
	m_coreInput->Dispatcher->StopProcessEvents();
}

// Saves the current state of the app for suspend and terminate events.
void DirectXPage::SaveInternalState(IPropertySet^ state)
{
	critical_section::scoped_lock lock(m_main->GetCriticalSection());
	m_deviceResources->Trim();

	// Stop rendering when the app is suspended.
	m_main->StopRenderLoop();

	// Put code to save app state here.
}

// Loads the current state of the app for resume events.
void DirectXPage::LoadInternalState(IPropertySet^ state)
{
	// Put code to load app state here.

	// Start rendering when the app is resumed.
	m_main->StartRenderLoop();
}

// Window event handlers.

void DirectXPage::OnVisibilityChanged(CoreWindow^ sender, VisibilityChangedEventArgs^ args)
{
	m_windowVisible = args->Visible;
	if (m_windowVisible)
	{
		m_main->StartRenderLoop();
	}
	else
	{
		m_main->StopRenderLoop();
	}
}

// DisplayInformation event handlers.

void DirectXPage::OnDpiChanged(DisplayInformation^ sender, Object^ args)
{
	critical_section::scoped_lock lock(m_main->GetCriticalSection());
	m_deviceResources->SetDpi(sender->LogicalDpi);
	m_main->CreateWindowSizeDependentResources();
}

void DirectXPage::OnOrientationChanged(DisplayInformation^ sender, Object^ args)
{
	critical_section::scoped_lock lock(m_main->GetCriticalSection());
	m_deviceResources->SetCurrentOrientation(sender->CurrentOrientation);
	m_main->CreateWindowSizeDependentResources();
}


void DirectXPage::OnDisplayContentsInvalidated(DisplayInformation^ sender, Object^ args)
{
	critical_section::scoped_lock lock(m_main->GetCriticalSection());
	m_deviceResources->ValidateDevice();
}

// Called when the app bar button is clicked.
void DirectXPage::AppBarButton_Click(Object^ sender, RoutedEventArgs^ e)
{
	// Use the app bar if it is appropriate for your app. Design the app bar, 
	// then fill in event handlers (like this one).
}

void DirectXPage::StorePointerEvent(PointerEventArgs^ e)
{
	m_baseX = e->CurrentPoint->Position.X;
	m_baseY = e->CurrentPoint->Position.Y;
}

void DirectXPage::OnPointerPressed(Object^ sender, PointerEventArgs^ e)
{
	// When the pointer is pressed begin tracking the pointer movement.
	StorePointerEvent(e);
	m_main->StartTracking();
}

void DirectXPage::OnPointerMoved(Object^ sender, PointerEventArgs^ e)
{
	// Update the pointer tracking code.
	if (m_main->IsTracking())
	{
		float x = m_baseX - e->CurrentPoint->Position.X;
		float y = m_baseY - e->CurrentPoint->Position.Y;

		int ttype = 0;
		
		if (m_cameraMoveType == 1) {
			ttype = 2;
		}
		else if (e->CurrentPoint->Properties->IsLeftButtonPressed) {
			ttype = 0;
		}
		else {
			ttype = 1;
		}
		m_main->TrackingUpdate(e->CurrentPoint->Properties->IsRightButtonPressed, x, y);
	}
}

void DirectXPage::OnPointerReleased(Object^ sender, PointerEventArgs^ e)
{
	// Stop tracking pointer movement when the pointer is released.
	m_main->StopTracking();
}

void DirectXPage::OnCompositionScaleChanged(SwapChainPanel^ sender, Object^ args)
{
	critical_section::scoped_lock lock(m_main->GetCriticalSection());
	m_deviceResources->SetCompositionScale(sender->CompositionScaleX, sender->CompositionScaleY);
	m_main->CreateWindowSizeDependentResources();
}

void DirectXPage::OnSwapChainPanelSizeChanged(Object^ sender, SizeChangedEventArgs^ e)
{
	critical_section::scoped_lock lock(m_main->GetCriticalSection());
	m_deviceResources->SetLogicalSize(e->NewSize);
	m_main->CreateWindowSizeDependentResources();
}


void KinectRecord::DirectXPage::Button_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{

}


void KinectRecord::DirectXPage::CheckBox_Checked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	m_main->StreamColor(ColorCheckBox->IsChecked->Value);
}

void KinectRecord::DirectXPage::Record(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	if (m_main->isRecording) {
		m_main->isRecording = false;
		RecIndicator->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
		RecordButton->Content = "RECORD";

		recEndTime->SetToNow();
		
		DateTime endTime = recEndTime->GetDateTime();
		DateTime startTime = recStartTime->GetDateTime();

		float seconds = (endTime.UniversalTime - startTime.UniversalTime) * 0.0000001;

		TakeDurationValue->Text = seconds.ToString();
		FramesRecordedValue->Text = m_main->currentFrame.ToString();
		FPSValue->Text = ((float)m_main->currentFrame / seconds).ToString();
	}
	else {
		if (m_main->sessionFolder != nullptr) {
			m_main->isRecording = true;
			RecIndicator->Visibility = Windows::UI::Xaml::Visibility::Visible;
			TakeDisplay->Text = m_main->currentTake.ToString();

			recStartTime->SetToNow();
			
			RecordButton->Content = "STOP";

			task<void> record_depth([this] {
				m_main->PrepToRecord();

				while (m_main->isRecording) {
					m_main->Record();
				}
			});

			record_depth.then([this] {
				m_main->EndRecording();
				});
		}
		else {
			Windows::UI::Popups::MessageDialog^ noFileWarning = ref new Windows::UI::Popups::MessageDialog("Please select a folder to record to.");
			noFileWarning->ShowAsync();
		}
	}
}

void KinectRecord::DirectXPage::StopRecording(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{

}

void KinectRecord::DirectXPage::PickAFileButton_Click(Object^ sender, RoutedEventArgs^ e)
{
	FolderPicker^ openPicker = ref new FolderPicker();
	openPicker->ViewMode = PickerViewMode::List;
	openPicker->SuggestedStartLocation = PickerLocationId::PicturesLibrary;
	openPicker->FileTypeFilter->Append(".adv");

	create_task(openPicker->PickSingleFolderAsync()).then([this](Windows::Storage::StorageFolder^ folder)
	{
		if (folder)
		{
			// create session folder - name by time
			Windows::Globalization::Calendar^ c = ref new Windows::Globalization::Calendar;
			c->SetToNow();
			Platform::String^ dateTime = L"ATRIUM_" + c->YearAsString() + L"_" + c->MonthAsPaddedNumericString(1) + L"_" + c->DayAsPaddedString(1) + L"_" + c->HourAsPaddedString(1) + L"_" + c->MinuteAsPaddedString(1);
			create_task(folder->CreateFolderAsync(dateTime)).then([this](Windows::Storage::StorageFolder^ sessionFolder)
			{
				m_main->sessionFolder = sessionFolder;
				this->FolderDisplay->Text = sessionFolder->Name;
			});
		}
	});
}


void KinectRecord::DirectXPage::TextBlock_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{

}


void KinectRecord::DirectXPage::Button_Click_1(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	if (m_main->isPlayingBack) {
		m_main->isPlayingBack = false;
		PlaybackButton->Content = "REPLAY";
	}
	else {
		if (m_main->sessionFolder != nullptr) {
			task<void> playback([this] {
				m_main->PrepToPlayback();
			});
			playback.then([] {});
			m_main->isPlayingBack = true;
			PlaybackButton->Content = "STOP";
		}
		else {
			FileOpenPicker^ openPicker = ref new FileOpenPicker();
			openPicker->ViewMode = PickerViewMode::List;
			openPicker->SuggestedStartLocation = PickerLocationId::PicturesLibrary;
			openPicker->FileTypeFilter->Append(".adv");

			create_task(openPicker->PickSingleFileAsync()).then([this](Windows::Storage::StorageFile^ file)
			{
				if (file)
				{
					m_main->takeFiles[0] = file;
					task<void> playback([this] {
						m_main->PrepToPlayback();
					});
					playback.then([] {});
					m_main->isPlayingBack = true;
					PlaybackButton->Content = "STOP";
					TimeSlider->Maximum = m_main->playbackBuffer->Size;
				}
			});
		}
	}
}


void KinectRecord::DirectXPage::FolderDisplay_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{

}


void KinectRecord::DirectXPage::TextBlock_SelectionChanged_1(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{

}


void KinectRecord::DirectXPage::Button_Click_2(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	FileOpenPicker^ openPicker = ref new FileOpenPicker();
	openPicker->ViewMode = PickerViewMode::List;
	openPicker->SuggestedStartLocation = PickerLocationId::PicturesLibrary;
	openPicker->FileTypeFilter->Append(".adv");

	create_task(openPicker->PickSingleFileAsync()).then([this](Windows::Storage::StorageFile^ exportFile)
	{
		if (exportFile)
		{
			m_main->exportFromFile = exportFile;
		}
	});
}


void KinectRecord::DirectXPage::TextBlock_SelectionChanged_2(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{

}


void KinectRecord::DirectXPage::Slider_ValueChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e)
{
	if (m_main) {
		m_main->currentFrame = e->NewValue;
	}
}


void KinectRecord::DirectXPage::Button_Click_3(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	FileOpenPicker^ openPicker = ref new FileOpenPicker();
	openPicker->ViewMode = PickerViewMode::List;
	openPicker->SuggestedStartLocation = PickerLocationId::PicturesLibrary;
	openPicker->FileTypeFilter->Append(".hlsl");

	create_task(openPicker->PickSingleFileAsync()).then([this](Windows::Storage::StorageFile^ file)
	{
		if (file)
		{
			SetShaderFile(file, 2);
		}
	});
}

void KinectRecord::DirectXPage::SetShaderFile(Windows::Storage::StorageFile^ shaderFile, int shaderType)
{
	m_main->shaderFiles[shaderType] = shaderFile;

	create_task(shaderFile->GetParentAsync()).then([this, shaderType](Windows::Storage::StorageFolder^ parentFolder)
	{
		Platform::Collections::Vector<Platform::String^>^ fileTypes = ref new Platform::Collections::Vector<Platform::String^>();
		fileTypes->Append(".hlsl");

		QueryOptions^ queryOptions = ref new QueryOptions(CommonFileQuery::OrderByName, fileTypes);
		m_main->shaderQueryResult[shaderType] = parentFolder->CreateFileQueryWithOptions(queryOptions);

		m_main->shaderQueryResult[shaderType]->ContentsChanged += ref new Windows::Foundation::TypedEventHandler<Windows::Storage::Search::IStorageQueryResultBase^, Platform::Object^>(this, &KinectRecord::DirectXPage::UpdateShaderIfChanged);
		m_main->shaderQueryResult[shaderType]->GetFilesAsync();

		UpdateShaderIfChanged(nullptr, nullptr);
	});
}

void KinectRecord::DirectXPage::UpdateShaderIfChanged(IStorageQueryResultBase^ sender, Platform::Object^ args)
{
	for (int i = 0; i < 3; ++i) {
		Windows::Storage::StorageFile^ shaderFile = m_main->shaderFiles[i];
		if (shaderFile != nullptr) {
			create_task(Windows::Storage::FileIO::ReadTextAsync(shaderFile)).then([this, i](Platform::String^ shaderText)
			{
				m_main->UpdateShader(shaderText, i);
			});
		}
	}
}

void KinectRecord::DirectXPage::Button_Click_4(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	FileOpenPicker^ openPicker = ref new FileOpenPicker();
	openPicker->ViewMode = PickerViewMode::List;
	openPicker->SuggestedStartLocation = PickerLocationId::PicturesLibrary;
	openPicker->FileTypeFilter->Append(".hlsl");

	create_task(openPicker->PickSingleFileAsync()).then([this](Windows::Storage::StorageFile^ file)
	{
		if (file)
		{
			SetShaderFile(file, 0);
		}
	});
}


void KinectRecord::DirectXPage::Button_Click_5(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	FileOpenPicker^ openPicker = ref new FileOpenPicker();
	openPicker->ViewMode = PickerViewMode::List;
	openPicker->SuggestedStartLocation = PickerLocationId::PicturesLibrary;
	openPicker->FileTypeFilter->Append(".hlsl");

	create_task(openPicker->PickSingleFileAsync()).then([this](Windows::Storage::StorageFile^ file)
	{
		if (file)
		{
			SetShaderFile(file, 1);
		}
	});
}

void KinectRecord::DirectXPage::Button_Click_6(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	FolderPicker^ openPicker = ref new FolderPicker();
	openPicker->ViewMode = PickerViewMode::List;
	openPicker->SuggestedStartLocation = PickerLocationId::PicturesLibrary;
	openPicker->FileTypeFilter->Append(".adv");

	create_task(openPicker->PickSingleFolderAsync()).then([this](Windows::Storage::StorageFolder^ folder)
	{
		if (folder)
		{
			task<void> export_take([this, folder] {
				m_main->exportToFolder = folder;
				m_main->ExportTakeToObj();
			});
			
			export_take.then([] {});
		}
	});
}
