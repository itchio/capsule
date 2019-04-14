#[macro_use]
extern crate libc_print;

use ::std::ptr;
use ::winapi::shared::minwindef;
use ::winapi::um::{errhandlingapi, processthreadsapi, psapi, winbase};

pub fn get_last_error_string() -> String {
    unsafe {
        const BUF_SIZE: usize = 1024;
        let mut buf = [0u16; BUF_SIZE];

        let n = winbase::FormatMessageW(
            winbase::FORMAT_MESSAGE_IGNORE_INSERTS
                | winbase::FORMAT_MESSAGE_FROM_SYSTEM
                | winbase::FORMAT_MESSAGE_ARGUMENT_ARRAY,
            ptr::null(),
            errhandlingapi::GetLastError(),
            0,
            buf.as_mut_ptr(),
            BUF_SIZE as u32,
            ptr::null_mut(),
        );

        let mut str = String::from_utf16_lossy(&buf);
        str.truncate(n as usize);
        str
    }
}

pub fn get_module_if_loaded(name: &str) -> minwindef::HMODULE {
    let name = name.to_lowercase();

    unsafe {
        let sizeof_hmodule = std::mem::size_of::<minwindef::HMODULE>();
        let p = processthreadsapi::GetCurrentProcess();

        let mut modules = {
            let mut bytes_needed: minwindef::DWORD = 0;
            psapi::EnumProcessModules(p, ptr::null_mut(), 0, &mut bytes_needed);
            libc_println!("need {} bytes to enum process modules", bytes_needed);
            let num_entries_needed = bytes_needed as usize / sizeof_hmodule;
            libc_println!(
                "need {} entries to enum process modules",
                num_entries_needed
            );
            let mut modules = Vec::<minwindef::HMODULE>::with_capacity(num_entries_needed);
            modules.resize(num_entries_needed, ptr::null_mut());
            modules
        };

        {
            let mut bytes_fetched: minwindef::DWORD = 0;
            let ret = psapi::EnumProcessModules(
                p,
                modules.as_mut_ptr(),
                (modules.len() * sizeof_hmodule) as u32,
                &mut bytes_fetched,
            );
            assert_non_zero!("EnumProcessModules", ret);

            let num_entries_fetched = bytes_fetched as usize / sizeof_hmodule;
            modules.resize(num_entries_fetched, ptr::null_mut());
        }

        for module in modules {
            {
                const BUF_SIZE: usize = 1024;
                let mut buf = [0u16; BUF_SIZE];

                let n = psapi::GetModuleBaseNameW(p, module, buf.as_mut_ptr(), BUF_SIZE as u32);

                let mut str = String::from_utf16_lossy(&buf);
                str.truncate(n as usize);
                libc_println!(" -> {}", str);
                if str.to_lowercase() == name {
                    return module;
                }
            }
        }
        ptr::null_mut()
    }
}

#[macro_export]
macro_rules! guard_handle {
    ($x:ident) => {
        ::scopeguard::guard($x, |h| {
            ::winapi::um::handleapi::CloseHandle(h);
        });
    };
}

#[macro_export]
macro_rules! guard_library {
    ($x: ident) => {
        ::scopeguard::guard($x, |l| {
            ::winapi::um::libloaderapi::FreeLibrary(l);
        });
    };
}

#[macro_export]
macro_rules! assert_non_null {
    ($desc: literal, $ptr: ident) => {
        if $ptr.is_null() {
            panic!(
                "[{}] Unexpected null pointer: {}",
                $desc,
                $crate::get_last_error_string()
            );
        }
    };
}

#[macro_export]
macro_rules! assert_non_zero {
    ($desc: literal, $num: ident) => {
        if $num == 0 {
            panic!(
                "[{}] Expected non-zero value: {}",
                $desc,
                $crate::get_last_error_string()
            );
        }
    };
}
