#pragma once
#include "App.xaml.g.h"

namespace winrt::VisionAI::implementation
{
    struct App : AppT<App>
    {
        App();
        void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const& e);

    private:
        Microsoft::UI::Xaml::Window window{ nullptr };
    };
}
