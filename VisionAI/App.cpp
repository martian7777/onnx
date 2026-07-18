#include "pch.h"
#include "App.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace vai {

App::App() {}

void App::OnLaunched(LaunchActivatedEventArgs const&) {
    // WinUI 3 loads default control styles automatically (no XamlControlsResources).
    window_ = std::make_unique<MainWindow>();
    window_->Activate();
}

} // namespace vai
