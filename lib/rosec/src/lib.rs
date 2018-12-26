#![no_std]

use core::panic::PanicInfo;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

extern {
    fn k_printf(msg: *const u8);
}

#[no_mangle]
pub extern "C" fn hello_rust() {
    let msg = b"Hello from Rust!\0";
    unsafe {
        k_printf(msg.as_ptr());
    }
}
