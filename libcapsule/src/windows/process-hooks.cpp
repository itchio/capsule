
#include <capsule.h>

///////////////////////////////////////////////
// CreateProcessA
///////////////////////////////////////////////

typedef BOOL (CAPSULE_STDCALL *CreateProcessA_t)(
  LPCSTR                lpApplicationName,
  LPSTR                 lpCommandLine,
  LPSECURITY_ATTRIBUTES lpProcessAttributes,
  LPSECURITY_ATTRIBUTES lpThreadAttributes,
  BOOL                  bInheritHandles,
  DWORD                 dwCreationFlags,
  LPVOID                lpEnvironment,
  LPCSTR                lpCurrentDirectory,
  LPSTARTUPINFO         lpStartupInfo,
  LPPROCESS_INFORMATION lpProcessInformation
);
CreateProcessA_t CreateProcessA_real;
SIZE_T CreateProcessA_hookId = 0;

BOOL CAPSULE_STDCALL CreateProcessA_hook (
  LPCSTR                lpApplicationName,
  LPSTR                 lpCommandLine,
  LPSECURITY_ATTRIBUTES lpProcessAttributes,
  LPSECURITY_ATTRIBUTES lpThreadAttributes,
  BOOL                  bInheritHandles,
  DWORD                 dwCreationFlags,
  LPVOID                lpEnvironment,
  LPCSTR                lpCurrentDirectory,
  LPSTARTUPINFO         lpStartupInfo,
  LPPROCESS_INFORMATION lpProcessInformation
) {
  capsule_log("CreateProcessA_hook called with %s %s", lpApplicationName, lpCommandLine);
  return CreateProcessA_real(
      lpApplicationName,
      lpCommandLine,
      lpProcessAttributes,
      lpThreadAttributes,
      bInheritHandles,
      dwCreationFlags,
      lpEnvironment,
      lpCurrentDirectory,
      lpStartupInfo,
      lpProcessInformation
  );
}

///////////////////////////////////////////////
// CreateProcessW
///////////////////////////////////////////////

typedef BOOL (CAPSULE_STDCALL *CreateProcessW_t)(
  LPCWSTR               lpApplicationName,
  LPWSTR                lpCommandLine,
  LPSECURITY_ATTRIBUTES lpProcessAttributes,
  LPSECURITY_ATTRIBUTES lpThreadAttributes,
  BOOL                  bInheritHandles,
  DWORD                 dwCreationFlags,
  LPVOID                lpEnvironment,
  LPCWSTR               lpCurrentDirectory,
  LPSTARTUPINFO         lpStartupInfo,
  LPPROCESS_INFORMATION lpProcessInformation
);
CreateProcessW_t CreateProcessW_real;
SIZE_T CreateProcessW_hookId = 0;

BOOL CAPSULE_STDCALL CreateProcessW_hook (
  LPCWSTR               lpApplicationName,
  LPWSTR                lpCommandLine,
  LPSECURITY_ATTRIBUTES lpProcessAttributes,
  LPSECURITY_ATTRIBUTES lpThreadAttributes,
  BOOL                  bInheritHandles,
  DWORD                 dwCreationFlags,
  LPVOID                lpEnvironment,
  LPCWSTR               lpCurrentDirectory,
  LPSTARTUPINFO         lpStartupInfo,
  LPPROCESS_INFORMATION lpProcessInformation
) {
  capsule_log("CreateProcessW_hook called with %S %S", lpApplicationName, lpCommandLine);

  cHookMgr.EnableHook(CreateProcessW_hookId, FALSE);

  DWORD err = NktHookLibHelpers::CreateProcessWithDllW(
      lpApplicationName,
      lpCommandLine,
      lpProcessAttributes,
      lpThreadAttributes,
      bInheritHandles,
      dwCreationFlags,
      (LPCWSTR) lpEnvironment,
      lpCurrentDirectory,
      lpStartupInfo,
      lpProcessInformation,
      // oh no.
      L"C:\\msys64\\home\\amos\\Dev\\capsule\\build\\capsule32.dll",
      NULL,
      "capsule_windows_init"
  );

  cHookMgr.EnableHook(CreateProcessW_hookId, TRUE);

  capsule_log("CreateProcessWithDLLW succeeded? %d", SUCCEEDED(err));

  return SUCCEEDED(err) ? 1 : 0;
}

void capsule_install_process_hooks () {
  DWORD err;

  HMODULE kernel = LoadLibrary(L"kernel32.dll");
  if (!kernel) {
    capsule_log("Could not load kernel32.dll");
    return;
  }

  // CreateProcessA
  {
    LPVOID CreateProcessA_addr = NktHookLibHelpers::GetProcedureAddress(kernel, "CreateProcessA");
    if (!CreateProcessA_addr) {
      capsule_log("Could not find CreateProcessA");
      return;
    }

    err = cHookMgr.Hook(
      &CreateProcessA_hookId,
      (LPVOID *) &CreateProcessA_real,
      CreateProcessA_addr,
      CreateProcessA_hook,
      0
    );
    if (err != ERROR_SUCCESS) {
      capsule_log("Hooking CreateProcessA derped with error %d (%x)", err, err);
      return;
    }
    capsule_log("Installed CreateProcessA hook");
  }

  // CreateProcessW
  {
    LPVOID CreateProcessW_addr = NktHookLibHelpers::GetProcedureAddress(kernel, "CreateProcessW");
    if (!CreateProcessW_addr) {
      capsule_log("Could not find CreateProcessW");
      return;
    }

    err = cHookMgr.Hook(
      &CreateProcessW_hookId,
      (LPVOID *) &CreateProcessW_real,
      CreateProcessW_addr,
      CreateProcessW_hook,
      0
    );
    if (err != ERROR_SUCCESS) {
      capsule_log("Hooking CreateProcessW derped with error %d (%x)", err, err);
      return;
    }
    capsule_log("Installed CreateProcessW hook");
  }
}
