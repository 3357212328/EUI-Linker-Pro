#include "app.h"
#include "app_controller.h"

#include <cstdlib>
#include <windows.h>

int main(int /*argc*/, char** /*argv*/) {
    // Prefer Skia (DirectWrite crisp font hinting & GPU acceleration) when compiled in,
    // gracefully falling back if another renderer is active.
#ifdef SLINT_PREFER_SKIA_BACKEND
    _putenv_s("SLINT_BACKEND", "winit-skia");
#endif

    // Ensure common controls / shell COM visual styles behave properly on Windows.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    auto window = AppWindow::create();
    fl::AppController controller(window);

    window->show();
    controller.onWindowShown();

    slint::run_event_loop();
    return 0;
}
