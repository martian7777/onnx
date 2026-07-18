#pragma once
#include "MainWindow.h"
#include <memory>

namespace vai {

// Pure-code WinUI 3 Application (no XAML markup / no markup compiler).
struct App : winrt::Microsoft::UI::Xaml::ApplicationT<App> {
    App();
    void OnLaunched(winrt::Microsoft::UI::Xaml::LaunchActivatedEventArgs const& e);

private:
    std::unique_ptr<MainWindow> window_;
};

} // namespace vai
