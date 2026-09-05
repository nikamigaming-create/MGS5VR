#pragma once
namespace mgs5vr {
using ExitCleanup = void(*)() noexcept;
// Run app-owned cleanup before Windows stops worker threads and enters DLL teardown.
void installProcessExitHook(ExitCleanup cleanup);
}
