#![cfg(target_os = "windows")]

pub fn initialize() {
    libc_println!("Hooking GL stuff probably");
    std::thread::spawn(move || {
        let mut i = 0;
        loop {
            std::thread::sleep(std::time::Duration::from_secs(1));
            libc_println!("[capsule] saying hi (loop {})", i);
            i += 1;
        }
    });
}
