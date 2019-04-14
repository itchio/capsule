#![cfg(windows)]

use std::mem;
use std::os::windows::process::CommandExt;
use std::path::PathBuf;
use std::process::Command;
use std::ptr;
use winapi::shared::minwindef;
use winapi::um::{
  handleapi, jobapi2, libloaderapi, memoryapi, processthreadsapi, synchapi, tlhelp32, winbase,
  winnt, wow64apiset,
};

pub unsafe fn run<'a>(matches: clap::ArgMatches<'a>) {
  let exec = matches.value_of("exec").unwrap();

  let args: Vec<&str> = matches.values_of("args").unwrap_or_default().collect();

  let mut cmd = Command::new(exec);
  cmd.args(args);
  cmd.creation_flags(winbase::CREATE_SUSPENDED);
  let mut child = cmd.spawn().expect("Command failed to start");
  println!("Created suspended process, pid = {}", child.id());

  let process_id: minwindef::DWORD = child.id();
  let process_handle = processthreadsapi::OpenProcess(
    winnt::PROCESS_ALL_ACCESS,
    0, /* inheritHandles */
    process_id,
  );
  assert_non_null!("OpenProcess", process_handle);
  let process_handle = guard_handle!(process_handle);

  let host_is_32bit = cfg!(target_pointer_width = "32");
  let target_is_32bit = host_is_32bit || {
    let mut is_wow64: minwindef::BOOL = 0;
    wow64apiset::IsWow64Process(*process_handle, &mut is_wow64);
    is_wow64 == 1
  };

  let job_object = jobapi2::CreateJobObjectW(ptr::null_mut(), ptr::null_mut());
  assert_non_null!("CreateJobObjectW", job_object);
  let job_object = guard_handle!(job_object);

  {
    let mut job_object_info = mem::zeroed::<winnt::JOBOBJECT_EXTENDED_LIMIT_INFORMATION>();
    job_object_info.BasicLimitInformation.LimitFlags = winnt::JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    let ret = jobapi2::SetInformationJobObject(
      *job_object,
      winnt::JobObjectExtendedLimitInformation,
      mem::transmute(&job_object_info),
      mem::size_of_val(&job_object_info) as u32,
    );
    assert_non_zero!("SetInformationJobObject", ret);

    // TODO: io completion port
  }

  {
    let ret = jobapi2::AssignProcessToJobObject(*job_object, *process_handle);
    assert_non_zero!("AssignProcessToJobObject", ret);
    println!("Assigned process to job object {:x}", *job_object as usize);
  }

  println!(
    "Target process is {}",
    if target_is_32bit { "32-bit" } else { "64-bit" },
  );
  let lib = match determine_lib_path(matches, host_is_32bit, target_is_32bit) {
    Ok(x) => x,
    Err(e) => panic!("{:?}", e),
  };
  println!("Will inject ({:?})", lib);
  let lib = std::fs::canonicalize(lib).expect("could not canonicalize lib path");

  let mut thread_id: Option<minwindef::DWORD> = None;
  {
    let snap = tlhelp32::CreateToolhelp32Snapshot(tlhelp32::TH32CS_SNAPTHREAD, 0);
    if snap == handleapi::INVALID_HANDLE_VALUE {
      panic!("Could not create toolhelp32 snapshot");
    }
    let snap = guard_handle!(snap);

    let mut te: tlhelp32::THREADENTRY32 = mem::zeroed();
    te.dwSize = mem::size_of::<tlhelp32::THREADENTRY32>() as u32;
    if tlhelp32::Thread32First(*snap, &mut te) == 0 {
      panic!("Could not grab Thread32First");
    }

    let mut num_threads = 0;
    loop {
      if te.th32OwnerProcessID == process_id {
        println!("Found thread {}", te.th32ThreadID);
        thread_id = Some(te.th32ThreadID);
        num_threads += 1;
      }

      if tlhelp32::Thread32Next(*snap, &mut te) == 0 {
        break;
      }
    }
    println!("Found {} threads total", num_threads);
  }

  {
    let k32 = libloaderapi::LoadLibraryW(wstrz!("kernel32.dll").as_ptr());
    assert_non_null!("LoadLibraryW(kernel32.dll)", k32);
    let k32 = guard_library!(k32);

    let loadlibrary = libloaderapi::GetProcAddress(*k32, const_cstr!("LoadLibraryW").as_ptr());
    assert_non_null!("GetProcAddress(LoadLibraryW)", loadlibrary);

    let dll_u16 = widestring::U16CString::from_os_str(lib.as_os_str()).unwrap();
    // .len() returns the number of wchars, excluding the null terminator
    let dll_u16_num_bytes = 2 * (dll_u16.len() + 1);

    let addr = memoryapi::VirtualAllocEx(
      *process_handle,
      ptr::null_mut(),
      dll_u16_num_bytes,
      winnt::MEM_COMMIT,
      winnt::PAGE_READWRITE,
    );
    assert_non_null!("VirtualAllocEx", addr);
    let addr = scopeguard::guard(addr, |addr| {
      memoryapi::VirtualFreeEx(
        *process_handle,
        addr,
        dll_u16_num_bytes,
        winnt::MEM_DECOMMIT,
      );
    });

    let mut n = 0;
    let ret = memoryapi::WriteProcessMemory(
      *process_handle,
      *addr,
      mem::transmute(dll_u16.as_ptr()),
      dll_u16_num_bytes,
      &mut n,
    );
    assert_non_zero!("WriteProcessMemory (return code)", ret);
    assert_non_zero!("WriteProcessMemory (written bytes)", n);

    let thread = processthreadsapi::CreateRemoteThread(
      *process_handle,
      ptr::null_mut(),
      0,
      mem::transmute(loadlibrary),
      *addr,
      0,
      ptr::null_mut(),
    );
    assert_non_null!("CreateRemoteThread", thread);
    let thread = guard_handle!(thread);

    synchapi::WaitForSingleObject(*thread, winbase::INFINITE);
  }

  {
    let thread_handle = processthreadsapi::OpenThread(
      winnt::THREAD_RESUME,
      0, /* inheritHandles */
      thread_id.expect("child process should have at least one thread"),
    );
    let thread_handle = guard_handle!(thread_handle);

    println!("Resuming thread...");
    processthreadsapi::ResumeThread(*thread_handle);
  }

  println!("Now waiting for child...");
  child.wait().expect("Non-zero exit code");
}

fn determine_lib_path<'a>(
  matches: clap::ArgMatches<'a>,
  host_is_32bit: bool,
  target_is_32bit: bool,
) -> Result<PathBuf, Box<std::error::Error>> {
  match matches.value_of("lib") {
    Some(lib) => Ok(PathBuf::from(lib)),
    _ => {
      let exe_dir = PathBuf::from(std::env::current_exe()?.parent().unwrap());
      let dll_name = PathBuf::from("capsule.dll");

      match target_is_32bit {
        true => match host_is_32bit {
          true => {
            // both are 32
            Ok(exe_dir.join(dll_name))
          }
          false => {
            // target is 32, host is 64
            Ok(
              exe_dir
                .parent()
                .unwrap()
                .parent()
                .unwrap()
                .join("i686-pc-windows-msvc")
                .join("debug")
                .join(dll_name),
            )
          }
        },
        false => match host_is_32bit {
          false => {
            // both are 64
            Ok(exe_dir.join(dll_name))
          }
          true => {
            // target is 64, host is 32
            Ok(
              exe_dir
                .parent()
                .unwrap()
                .parent()
                .unwrap()
                .join("x86-64-pc-windows-msvc")
                .join("debug")
                .join(dll_name),
            )
          }
        },
      }
    }
  }
}
