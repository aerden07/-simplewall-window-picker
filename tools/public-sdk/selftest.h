// Test-only entry point: runs before app/profile/WFP initialization.
static INT sw_compat_selftest (VOID)
{
    WCHAR own_path[32768], system_path[32768], report_path[32768];
    FILE *report = NULL;
    INT failures = 0;
    const WCHAR *names[] = {L"kernel32.dll", L"user32.dll", L"shell32.dll"};
    GetModuleFileNameW (NULL, own_path, RTL_NUMBER_OF (own_path));
    wcscpy_s (report_path, RTL_NUMBER_OF (report_path), own_path);
    wcscat_s (report_path, RTL_NUMBER_OF (report_path), L".test.txt");
    _wfopen_s (&report, report_path, L"w");
    if (!report) return 2;
    CoInitializeEx (NULL, COINIT_APARTMENTTHREADED);
    {
        const WCHAR *sid_text=L"S-1-15-2-1-2-3-4-5-6-7";
        PSID sid=NULL;
        PR_STRING converted=NULL;
        BOOL passed=FALSE;
        if(ConvertStringSidToSidW(sid_text,&sid))
        {
            NTSTATUS status=_r_str_fromsid(&converted,sid);
            passed=NT_SUCCESS(status) && IsValidSid(sid) && converted && wcscmp(converted->buffer,sid_text)==0;
            if(converted) _r_obj_dereference(converted);
            LocalFree(sid);
        }
        fprintf(report,"Windows package SID conversion and input integrity: %s\n",passed?"PASS":"FAIL");
        if(!passed) failures++;
    }
    for (INT iteration = 0; iteration < 100; iteration++)
    {
        for (INT file = 0; file < 4; file++)
        {
            ITEM_APP_INFO info = {0};
            if (file == 0)
                wcscpy_s (system_path, RTL_NUMBER_OF (system_path), own_path);
            else
            {
                GetSystemDirectoryW (system_path, RTL_NUMBER_OF (system_path));
                wcscat_s (system_path, RTL_NUMBER_OF (system_path), L"\\");
                wcscat_s (system_path, RTL_NUMBER_OF (system_path), names[file - 1]);
            }
            info.path = _r_obj_createstring (system_path);
            __try
            {
                _app_getfileversioninfo (&info);
                if (!info.version_info || !info.version_info->length) failures++;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                fprintf (report, "Exception: %08lx iteration %d file %d\n", GetExceptionCode (), iteration, file);
                failures++;
            }
            if (info.version_info) _r_obj_dereference (info.version_info);
            _r_obj_dereference (info.path);
        }
    }
    fprintf (report, "Production file-version reader: 400 calls, %d failures\n", failures);
    for (INT id = IDP_ADD; id <= IDP_SEARCH_DARK; id++)
    {
        HBITMAP bitmap = NULL;
        NTSTATUS status = _r_res_loadimage (&bitmap, GetModuleHandleW (NULL), L"PNG", MAKEINTRESOURCE (id), &GUID_ContainerFormatPng, 24, 24);
        fprintf (report, "PNG %d: status %08lx, bitmap %s\n", id, status, bitmap ? "OK" : "MISSING");
        if (!NT_SUCCESS (status) || !bitmap) failures++;
        if (bitmap) DeleteObject (bitmap);
    }
    {
        INITCOMMONCONTROLSEX cc={sizeof(cc),ICC_BAR_CLASSES};
        HWND parent, toolbar;
        HIMAGELIST images;
        TBBUTTON button={0};
        const INT expected[]={2,5,I_IMAGENONE};
        const INT requested[]={I_DEFAULT,5,I_IMAGENONE};
        InitCommonControlsEx(&cc);
        images=ImageList_Create(24,24,ILC_COLOR32,6,1);
        ImageList_SetImageCount(images,6);
        parent=CreateWindowExW(0,L"STATIC",L"Toolbar regression",WS_OVERLAPPED,0,0,400,100,NULL,NULL,GetModuleHandleW(NULL),NULL);
        toolbar=CreateWindowExW(0,TOOLBARCLASSNAMEW,NULL,WS_CHILD,0,0,400,40,parent,(HMENU)42,GetModuleHandleW(NULL),NULL);
        if(!parent || !toolbar) failures++;
        else
        {
            SendMessageW(toolbar,TB_BUTTONSTRUCTSIZE,sizeof(TBBUTTON),0);
            SendMessageW(toolbar,TB_SETIMAGELIST,0,(LPARAM)images);
            button.iBitmap=2; button.idCommand=100; button.fsState=TBSTATE_ENABLED;
            SendMessageW(toolbar,TB_ADDBUTTONS,1,(LPARAM)&button);
            for(INT step=0;step<3;step++)
            {
                TBBUTTON actual={0};
                BOOLEAN updated=_r_toolbar_setbutton(parent,42,100,L"Localized text",BTNS_AUTOSIZE,0,requested[step]);
                BOOL read=(BOOL)SendMessageW(toolbar,TB_GETBUTTON,0,(LPARAM)&actual);
                BOOL passed=updated && read && actual.iBitmap==expected[step];
                fprintf(report,"Toolbar step %d: expected %d, actual %d: %s\n",step,expected[step],actual.iBitmap,passed?"PASS":"FAIL");
                if(!passed) failures++;
            }
        }
        if(parent) DestroyWindow(parent);
        if(images) ImageList_Destroy(images);
    }
    fprintf (report, "TOTAL FAILURES: %d\nNo profile or WFP initialization performed.\n", failures);
    fclose (report);
    CoUninitialize ();
    return failures ? 1 : 0;
}
