#![no_std]

use core::panic::PanicInfo;

extern {
    fn k_printf(msg: *const u8);

    #[link_name="panic"]
    fn k_panic(msg: *const u8) -> !;
}

#[panic_handler]
fn panic(info: &PanicInfo) -> ! {
    let msg = match info.payload().downcast_ref::<&str>() {
        Some(s) => s.as_ptr(),
        _ => b"Rust panic\0".as_ptr(),
    };
    unsafe {
        k_panic(msg);
    }
}

#[no_mangle]
pub extern "C" fn hello_rust() {
    let msg = b"Hello from Rust!\0";
    unsafe {
        k_printf(msg.as_ptr());
    }
}
