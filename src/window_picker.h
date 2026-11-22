// Manual application access prompt. No filter changes before user confirmation.
#pragma once

static BOOLEAN window_picker_active = FALSE;
static BOOLEAN window_picker_finishing = FALSE;
static HWND window_picker_overlay = NULL;
static WINDOWPLACEMENT window_picker_placement;

static VOID _app_window_picker_select (HWND hwnd);

static VOID _app_window_picker_cancel (HWND hwnd)
{
	if (!window_picker_active)
		return;

	window_picker_active = FALSE;
	if (window_picker_overlay)
	{
		HWND overlay = window_picker_overlay;
		window_picker_overlay = NULL;
		DestroyWindow (overlay);
	}
	SetCursor (LoadCursorW (NULL, IDC_ARROW));
	SetWindowPlacement (hwnd, &window_picker_placement);
	SetForegroundWindow (hwnd);
}

static LRESULT CALLBACK _app_window_picker_proc (HWND overlay, UINT msg, WPARAM wparam, LPARAM lparam)
{
	HWND hwnd = (HWND)GetWindowLongPtrW (overlay, GWLP_USERDATA);
	switch (msg)
	{
		case WM_CREATE:
			SetWindowLongPtrW (overlay, GWLP_USERDATA, (LONG_PTR)((LPCREATESTRUCTW)lparam)->lpCreateParams);
			return 0;
		case WM_LBUTTONDOWN:
			window_picker_finishing = TRUE;
			ShowWindow (overlay, SW_HIDE);
			_app_window_picker_select (hwnd);
			window_picker_finishing = FALSE;
			return 0;
		case WM_KEYDOWN:
			if (wparam != VK_ESCAPE)
				break;
			FALLTHROUGH;
		case WM_RBUTTONDOWN:
		case WM_CLOSE:
			_app_window_picker_cancel (hwnd);
			return 0;
		case WM_ACTIVATE:
			if (LOWORD (wparam) == WA_INACTIVE && !window_picker_finishing)
				_app_window_picker_cancel (hwnd);
			return 0;
	}
	return DefWindowProcW (overlay, msg, wparam, lparam);
}

static VOID _app_window_picker_begin (HWND hwnd)
{
	WNDCLASSW wc = {0};
	if (window_picker_active)
		return;

	wc.lpfnWndProc = &_app_window_picker_proc;
	wc.hInstance = GetModuleHandleW (NULL);
	wc.hCursor = LoadCursorW (NULL, IDC_CROSS);
	wc.hbrBackground = (HBRUSH)GetStockObject (BLACK_BRUSH);
	wc.lpszClassName = L"simplewall.WindowPicker";
	if (!RegisterClassW (&wc) && GetLastError () != ERROR_CLASS_ALREADY_EXISTS)
		return;

	window_picker_placement.length = sizeof (WINDOWPLACEMENT);
	if (!GetWindowPlacement (hwnd, &window_picker_placement))
		return;
	window_picker_overlay = CreateWindowExW (
		WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
		wc.lpszClassName, L"Uygulama se\u00e7 - Esc: iptal", WS_POPUP,
		GetSystemMetrics (SM_XVIRTUALSCREEN), GetSystemMetrics (SM_YVIRTUALSCREEN),
		GetSystemMetrics (SM_CXVIRTUALSCREEN), GetSystemMetrics (SM_CYVIRTUALSCREEN),
		NULL, NULL, wc.hInstance, hwnd);
	if (!window_picker_overlay)
		return;

	// Capture the next click without activating or clicking the target program.
	SetLayeredWindowAttributes (window_picker_overlay, 0, 16, LWA_ALPHA);
	window_picker_active = TRUE;
	ShowWindow (hwnd, SW_MINIMIZE);
	ShowWindow (window_picker_overlay, SW_SHOW);
	SetForegroundWindow (window_picker_overlay);
	SetFocus (window_picker_overlay);
	SetCursor (LoadCursorW (NULL, IDC_CROSS));
}

static VOID _app_window_picker_select (HWND hwnd)
{
	WCHAR image_path[32768], window_class[128];
	DWORD process_id = 0, length = RTL_NUMBER_OF (image_path), error, token_length;
	POINT point;
	HWND target, notification, root;
	HANDLE process, token;
	PR_STRING path, identity = NULL;
	PTOKEN_APPCONTAINER_INFORMATION container;
	LPWSTR sid_string;
	PITEM_APP app;
	PITEM_LOG log;

	if (!GetCursorPos (&point))
	{
		_app_window_picker_cancel (hwnd);
		return;
	}

	target = WindowFromPoint (point);
	_app_window_picker_cancel (hwnd);
	if (!target || target == GetDesktopWindow () || target == GetShellWindow ())
		return;
	root = GetAncestor (target, GA_ROOT);
	GetClassNameW (root, window_class, RTL_NUMBER_OF (window_class));
	if (root == GetShellWindow () || _wcsicmp (window_class, L"WorkerW") == 0 ||
		_wcsicmp (window_class, L"Shell_TrayWnd") == 0 || _wcsicmp (window_class, L"Shell_SecondaryTrayWnd") == 0)
		return;

	GetWindowThreadProcessId (target, &process_id);
	if (!process_id || process_id == GetCurrentProcessId ())
		return;

	process = OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
	if (!process)
	{
		_r_show_errormessage (hwnd, L"Uygulama okunamad\u0131.", GetLastError (), NULL, ET_WINDOWS);
		return;
	}

	if (!QueryFullProcessImageNameW (process, 0, image_path, &length))
	{
		error = GetLastError ();
		CloseHandle (process);
		_r_show_errormessage (hwnd, L"Uygulama yolu okunamad\u0131.", error, NULL, ET_WINDOWS);
		return;
	}
	// Packaged applications must be identified by their AppContainer SID.
	if (OpenProcessToken (process, TOKEN_QUERY, &token))
	{
		token_length = 0;
		GetTokenInformation (token, TokenAppContainerSid, NULL, 0, &token_length);
		if (token_length)
		{
			container = (PTOKEN_APPCONTAINER_INFORMATION)_r_mem_allocate (token_length);
			if (GetTokenInformation (token, TokenAppContainerSid, container, token_length, &token_length) && container->TokenAppContainer)
			{
				if (ConvertSidToStringSidW (container->TokenAppContainer, &sid_string))
				{
					identity = _r_obj_createstring (sid_string);
					LocalFree (sid_string);
				}
			}
			_r_mem_free (container);
		}
		CloseHandle (token);
	}
	CloseHandle (process);

	// Shared hosts cannot reliably identify the application under the pointer.
	if (_wcsicmp (PathFindFileNameW (image_path), L"ApplicationFrameHost.exe") == 0 ||
		_wcsicmp (PathFindFileNameW (image_path), L"svchost.exe") == 0)
	{
		if (identity)
			_r_obj_dereference (identity);
		MessageBoxW (hwnd, L"Bu pencere ortak bir Windows s\u00fcrecine ait. Uygulamay\u0131 simplewall listesinden se\u00e7in.", L"Pencereden se\u00e7", MB_OK | MB_ICONINFORMATION);
		return;
	}

	path = _r_obj_createstring (image_path);
	if (identity)
	{
		app = _app_addapplication (hwnd, DATA_APP_UWP, identity, path, path);
		_r_obj_dereference (identity);
	}
	else
	{
		app = _app_addapplication (hwnd, DATA_UNKNOWN, path, NULL, NULL);
	}
	_r_obj_dereference (path);
	if (!app)
		return;

	// A manual prompt is not a captured connection; do not invent address data.
	log = (PITEM_LOG)_r_obj_allocate (sizeof (ITEM_LOG), &_app_dereferencelog);
	log->app_hash = app->app_hash;
	log->path = _r_obj_reference (app->original_path);
	log->timestamp = _r_unixtime_now ();
	log->is_manual = TRUE;
	log->filter_name = _r_obj_createstring (L"Elle a\u00e7\u0131lan eri\u015fim iste\u011fi");
	_r_obj_swapreference ((PVOID_PTR)&app->notification, log);

	// Deliberately bypass automatic notification cooldown and silent mode.
	notification = _app_notify_getwindow (NULL);
	if (notification)
		_app_notify_show (notification, log);
	else
		notification = _app_notify_getwindow ((PITEM_LOG)_r_obj_reference (log));

	if (notification)
	{
		ShowWindow (notification, SW_SHOW);
		SetForegroundWindow (notification);
	}
	else
	{
		MessageBoxW (hwnd, L"\u0130zin ekran\u0131 a\u00e7\u0131lamad\u0131. \u0130nternet eri\u015fim izni de\u011fi\u015ftirilmedi.", L"Pencereden se\u00e7", MB_OK | MB_ICONERROR);
	}

	_app_listview_updateby_param (hwnd, app->app_hash, PR_SETITEM_UPDATE, TRUE);
	_app_profile_save (hwnd);
	_r_obj_dereference (log);
	_r_obj_dereference (app);
}
