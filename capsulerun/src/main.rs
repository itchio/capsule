#[macro_use(defer)]
extern crate scopeguard;

use clap::{App, Arg};
use std::os::windows::process::CommandExt;
use std::process::Command;
use winapi::shared::minwindef;
use winapi::um::{handleapi, processthreadsapi, tlhelp32, winbase, winnt};

fn main() {
    let matches = App::new("capsulerun")
        .version("master")
        .author("Amos Wenger <amos@itch.io>")
        .about("Your very own libcapsule injector")
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
    let args: Vec<&str> = matches.values_of("args").unwrap_or_default().collect();

    let mut cmd = Command::new(exec);
    cmd.args(args);
    cmd.creation_flags(winbase::CREATE_SUSPENDED);
    let mut child = cmd.spawn().expect("Command failed to start");
    println!("Created suspended process, pid = {}", child.id());

    let process_id: minwindef::DWORD = child.id();
    let mut thread_id: Option<minwindef::DWORD> = None;
    unsafe {
        // TODO: figure better way to close handles
        // TODO: error handling
        let snap = tlhelp32::CreateToolhelp32Snapshot(tlhelp32::TH32CS_SNAPTHREAD, 0);
        if snap == handleapi::INVALID_HANDLE_VALUE {
            panic!("Could not create toolhelp32 snapshot");
        }
        defer!({
            handleapi::CloseHandle(snap);
        });

        let mut te: tlhelp32::THREADENTRY32 = std::mem::uninitialized();
        te.dwSize = std::mem::size_of::<tlhelp32::THREADENTRY32>() as u32;
        if tlhelp32::Thread32First(snap, &mut te) == 0 {
            panic!("Could not grab Thread32First");
        }

        let mut num_threads = 0;
        loop {
            if te.th32OwnerProcessID == process_id {
                println!("Found thread {}", te.th32ThreadID);
                thread_id = Some(te.th32ThreadID);
                num_threads += 1;
            }

            if tlhelp32::Thread32Next(snap, &mut te) == 0 {
                break;
            }
        }
        println!("Found {} threads total", num_threads);
    }

    unsafe {
        let thread_handle = processthreadsapi::OpenThread(
            winnt::PROCESS_ALL_ACCESS,
            0, /* inheritHandles */
            thread_id.expect("child process should have at least one thread"),
        );
        defer!({
            handleapi::CloseHandle(thread_handle);
        });

        println!("Resuming thread...");
        processthreadsapi::ResumeThread(thread_handle);
    }

    child.wait().expect("Non-zero exit code");
}
