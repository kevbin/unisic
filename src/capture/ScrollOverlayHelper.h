#pragma once

// Only the overlay UI uses XWayland. Capture stays in the Wayland parent and
// uses its authorized ScreenCast/PipeWire stream. stdin EOF owns the lifetime.
int runScrollOverlayHelper(int argc, char *argv[], const char *platform = "xcb");
