# Experimental window selection and public SDK compatibility

## What changed

- A transparent selection overlay identifies the process behind a clicked window.
- The normal notification UI supports a manual prompt without invented network addresses or ports. Address-based rule creation is disabled for this prompt.
- Application access still requires an Allow or Block decision in the notification.
- An experimental adapter allows the 3.9 source to compile with the older public routine SDK. Fixes cover version-resource argument ordering, package SID conversion, toolbar image preservation, bounded string length and window Z order.

## Limitations

- This is not the private/current SDK used for upstream releases. The adapter has not been comprehensively audited or tested across the supported operating systems.
- New labels are currently Turkish, with no localization integration yet.
- Desktop/taskbar and shared ApplicationFrameHost/svchost targets are rejected. AppContainer tokens use package identity when available; other processes fall back to executable paths.
- A window's process may differ from the process making network requests (for example a WebView2 child). The picker does not grant access to child processes or shared hosts automatically.
- Package updates can change executable paths. Picking a visible window is not a guarantee that every packaged application can be identified correctly.
- Closing or cancelling a manual prompt does not approve network access; the app may already have pre-existing rules.
- Existing upstream update checks refer to upstream releases and may replace this modified build.

## Build

Prerequisites: Git, Windows PowerShell 5.1 or newer, Visual Studio C++ Build Tools with a Windows SDK. The project targets MSVC v145 (Visual Studio 2026).

From the repository root:

```powershell
powershell -NoProfile -File tools/public-sdk/build.ps1
```

The script downloads the pinned public routine source and CppWinRT package into ignored dependency directories, prepares a patched SDK copy, and builds Release x64. It does not install the executable or change firewall settings. Output: `bin/64/simplewall.exe`.

Run isolated compatibility tests:

```powershell
powershell -NoProfile -File tools/public-sdk/build.ps1 -SelfTest
```

The test executable returns before profile or WFP initialization. Its output is in `temp/selftest-bin/simplewall.exe.test.txt`. Never distribute the test executable as the application.

## Validation and remaining work

The local development build passed 400 file-version reads, loading all 17 toolbar PNG resources, toolbar image preserve/change/clear checks, and a SID conversion/input-integrity test. The published test uses a synthetic SID. Release x64 compilation succeeded. Startup and toolbar rendering were observed on one Windows machine. This is not comprehensive end-to-end or security validation; allow/block, cancellation, DPI/multiple displays and packaged-app behavior need broader testing.

Back up your existing profile and application before trying a custom build. No installer or binary release is provided here.

## Source provenance

- simplewall 3.9 base: `2daccefdea552e3c40ec783d6642530f6e27958a`, [upstream](https://github.com/henrypp/simplewall/tree/2daccefdea552e3c40ec783d6642530f6e27958a).
- Public routine source: `f768aeaaf69b63819fe0500ed64f456b60ba02f7`, [repository](https://github.com/henrypp/routine/tree/f768aeaaf69b63819fe0500ed64f456b60ba02f7). The prepare script verifies the expected routine.c SHA-256 and patches a copy, preserving the SDK license.
- The initial compatibility adapter and preparation files came from [this historical simplewall revision](https://github.com/henrypp/simplewall/tree/b72c13a0ecc1d42de8c163958393587e261f9886/tools/public-sdk), subsequently amended with the fixes described above. That revision is separate from the 3.9 base; this is not an endorsement by upstream.

## Privacy of this source publication

Only reviewed source changes, generic build scripts and documentation are included. No local profiles, personal firewall rules, configuration, user-specific paths, screenshots, crash dumps, logs, application data, backup archives, executables or debug symbols are included. Upstream resources and original public attribution remain intact.
