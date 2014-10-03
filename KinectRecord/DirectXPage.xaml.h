//
// DirectXPage.xaml.h
// Declaration of the DirectXPage class.
//

#pragma once

#include "DirectXPage.g.h"

#include "Common\DeviceResources.h"
#include "KinectRecordMain.h"

namespace KinectRecord
{
	/// <summary>
	/// A page that hosts a DirectX SwapChainPanel.
	/// </summary>
	public ref class DirectXPage sealed
	{
	public:
		DirectXPage();
		virtual ~DirectXPage();

		void SaveInternalState(Windows::Foundation::Collections::IPropertySet^ state);
		void LoadInternalState(Windows::Foundation::Collections::IPropertySet^ state);

	private:
		// XAML low-level rendering event handler.
		void OnRendering(Platform::Object^ sender, Platform::Object^ args);

		// Window event handlers.
		void OnVisibilityChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::VisibilityChangedEventArgs^ args);

		// DisplayInformation event handlers.
		void OnDpiChanged(Windows::Graphics::Display::DisplayInformation^ sender, Platform::Object^ args);
		void OnOrientationChanged(Windows::Graphics::Display::DisplayInformation^ sender, Platform::Object^ args);
		void OnDisplayContentsInvalidated(Windows::Graphics::Display::DisplayInformation^ sender, Platform::Object^ args);

		// Other event handlers.
		void AppBarButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnCompositionScaleChanged(Windows::UI::Xaml::Controls::SwapChainPanel^ sender, Object^ args);
		void OnSwapChainPanelSizeChanged(Platform::Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e);

		// Track our independent input on a background worker thread.
		Windows::Foundation::IAsyncAction^ m_inputLoopWorker;
		Windows::UI::Core::CoreIndependentInputSource^ m_coreInput;

		// Independent input handling functions.
		void StorePointerEvent(Windows::UI::Core::PointerEventArgs^ e);
		void OnPointerPressed(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e);
		void OnPointerMoved(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e);
		void OnPointerReleased(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e);
		float m_baseX, m_baseY;
		int m_cameraMoveType;

		void SetShaderFile(Windows::Storage::StorageFile^ shaderFile, int shaderType);
		void UpdateShaderIfChanged(Windows::Storage::Search::IStorageQueryResultBase^ sender, Platform::Object^ args);

		// Resources used to render the DirectX content in the XAML page background.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		std::unique_ptr<KinectRecordMain> m_main; 
		bool m_windowVisible;
		void Button_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void CheckBox_Checked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void Record(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void StopRecording(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void PickAFileButton_Click(Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);

		bool m_KinectConnected;
		void TextBlock_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void Button_Click_1(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void FolderDisplay_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void TextBlock_SelectionChanged_1(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void Button_Click_2(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void TextBlock_SelectionChanged_2(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void Slider_ValueChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e);
		void Button_Click_3(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void Button_Click_4(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void Button_Click_5(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
	};
}

