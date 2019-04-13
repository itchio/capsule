pub fn get_last_error_string() -> String {
    unsafe {
        use ::std::ptr;
        use ::winapi::um::{errhandlingapi, winbase};

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
            (BUF_SIZE + 1) as u32,
            ptr::null_mut(),
        );

        let mut str = String::from_utf16(&buf).unwrap();
        str.truncate(n as usize);
        str
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
