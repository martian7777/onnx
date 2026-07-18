#include "pch.h"
#include "App.h"

// Entry point (the XAML markup compiler normally generates this; in a pure-code
// app we provide it ourselves). Self-contained app: WinRT activation comes from
// the embedded reg-free SxS manifest, so no bootstrapper / installed runtime.
int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    winrt::Microsoft::UI::Xaml::Application::Start([](auto&&) {
        winrt::make<vai::App>();
    });
    return 0;
}
