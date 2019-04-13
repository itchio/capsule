#[macro_use]
extern crate wstr;
#[macro_use]
extern crate const_cstr;
#[macro_use]
extern crate wincap;

use clap::{App, Arg};
use std::os::windows::process::CommandExt;
use std::path::Path;
use std::process::Command;
use std::ptr;
use winapi::shared::minwindef;
use winapi::um::{
    handleapi, libloaderapi, memoryapi, processthreadsapi, synchapi, tlhelp32, winbase, winnt,
};

fn main() {
    let matches = App::new("capsulerun")
        .version("master")
        .author("Amos Wenger <amos@itch.io>")
        .about("Your very own libcapsule injector")
        .arg(
            Arg::with_name("dll")
                .long("dll")
                .help("Path of the dll to inject")
                .takes_value(true)
                .required(true),
        )
        .arg(
            Arg::with_name("exec")
                .help("The executable to run")
                .required(true)
                .index(1),
        )
        .arg(
            Arg::with_name("args")
                .help("Arguments to pass to the executable")
                .multiple(true)
                .index(2),
        )
        .get_matches();

    let exec = matches.value_of("exec").unwrap();
    let dll = Path::new(matches.value_of("dll").unwrap());
    let dll = std::fs::canonicalize(dll).expect("could not canonicalize DLL path");

    let args: Vec<&str> = matches.values_of("args").unwrap_or_default().collect();

    let mut cmd = Command::new(exec);
    cmd.args(args);
    cmd.creation_flags(winbase::CREATE_SUSPENDED);
    let mut child = cmd.spawn().expect("Command failed to start");
    println!("Created suspended process, pid = {}", child.id());

    let process_id: minwindef::DWORD = child.id();
    let mut thread_id: Option<minwindef::DWORD> = None;
    unsafe {
        let snap = tlhelp32::CreateToolhelp32Snapshot(tlhelp32::TH32CS_SNAPTHREAD, 0);
        if snap == handleapi::INVALID_HANDLE_VALUE {
            panic!("Could not create toolhelp32 snapshot");
        }
        let snap = guard_handle!(snap);

        let mut te: tlhelp32::THREADENTRY32 = std::mem::uninitialized();
        te.dwSize = std::mem::size_of::<tlhelp32::THREADENTRY32>() as u32;
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

    unsafe {
        let k32 = libloaderapi::LoadLibraryW(wstrz!("kernel32.dll").as_ptr());
        assert_non_null!("LoadLibraryW(kernel32.dll)", k32);
        let k32 = guard_library!(k32);

        let loadlibrary = libloaderapi::GetProcAddress(*k32, const_cstr!("LoadLibraryW").as_ptr());
        assert_non_null!("GetProcAddress(LoadLibraryW)", loadlibrary);

        let h = processthreadsapi::OpenProcess(
            winnt::PROCESS_ALL_ACCESS,
            0, /* inheritHandles */
            process_id,
        );
        assert_non_null!("OpenProcess", loadlibrary);
        let h = guard_handle!(h);

        let dll_u16 = widestring::U16CString::from_os_str(dll.as_os_str()).unwrap();
        // .len() returns the number of wchars, excluding the null terminator
        let dll_u16_num_bytes = 2 * (dll_u16.len() + 1);

        let addr = memoryapi::VirtualAllocEx(
            *h,
            ptr::null_mut(),
            dll_u16_num_bytes,
            winnt::MEM_COMMIT,
            winnt::PAGE_READWRITE,
        );
        assert_non_null!("VirtualAllocEx", addr);
        let addr = scopeguard::guard(addr, |addr| {
            memoryapi::VirtualFreeEx(*h, addr, dll_u16_num_bytes, winnt::MEM_DECOMMIT);
        });

        let mut n = 0;
        let ret = memoryapi::WriteProcessMemory(
            *h,
            *addr,
            std::mem::transmute(dll_u16.as_ptr()),
            dll_u16_num_bytes,
            &mut n,
        );
        assert_non_zero!("WriteProcessMemory (return code)", ret);
        assert_non_zero!("WriteProcessMemory (written bytes)", n);

        let thread = processthreadsapi::CreateRemoteThread(
            *h,
            ptr::null_mut(),
            0,
            std::mem::transmute(loadlibrary),
            *addr,
            0,
            ptr::null_mut(),
        );
        assert_non_null!("CreateRemoteThread", thread);
        let thread = guard_handle!(thread);

        synchapi::WaitForSingleObject(*thread, winbase::INFINITE);
    }

    unsafe {
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
