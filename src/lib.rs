use std::ffi::{CString, c_void};
use std::io::Error;
use log::trace;

pub struct CioFlag;

impl CioFlag {
    pub const READABLE: i32 = (1<<0);
    pub const WRITABLE: i32 = (1<<1);
}

pub trait CioWrapper {
    fn getfd(&self) -> i32;
    fn get_wrapper(&self) -> *mut c_void;
}

/**
 * CioStream
 */

pub struct CioStream {
    pub stream: *mut cio_sys::cio_stream,
    pub auto_drop: bool,
}

unsafe impl Send for CioStream {}
unsafe impl Sync for CioStream {}

impl Drop for CioStream {
    fn drop(&mut self) {
        unsafe {
            if self.auto_drop {
                trace!("drop CioStream:{}", self.getfd());
                cio_sys::cio_stream_drop(self.stream);
            }
        }
    }
}

impl CioWrapper for CioStream {
    fn getfd(&self) -> i32 {
        return self.getfd();
    }

    fn get_wrapper(&self) -> *mut c_void {
        return self.stream as *mut c_void;
    }
}

impl CioStream {
    pub fn connect(addr: &str) -> Result<CioStream, Error> {
        let addr = CString::new(addr).unwrap();
        unsafe {
            let stream = cio_sys::cio_stream_connect(addr.as_ptr());
            if stream.is_null() {
                Err(Error::last_os_error())
            } else {
                Ok(CioStream { stream: stream, auto_drop: true })
            }
        }
    }

    pub fn getfd(&self) -> i32 {
        unsafe { return cio_sys::cio_stream_getfd(self.stream); }
    }

    pub fn recv(&self, buf: &mut [u8]) ->i32 {
        unsafe {
            return cio_sys::cio_stream_recv(
                self.stream, buf.as_ptr() as *mut c_void, buf.len() as u64);
        }
    }

    pub fn send(&self, buf: &[u8]) -> i32 {
        unsafe {
            return cio_sys::cio_stream_send(
                self.stream, buf.as_ptr() as *const c_void, buf.len() as u64);
        }
    }
}

/**
 * CioListener
 */

pub struct CioListener {
    pub listener: *mut cio_sys::cio_listener,
}

unsafe impl Send for CioListener {}
unsafe impl Sync for CioListener {}

impl Drop for CioListener {
    fn drop(&mut self) {
        unsafe {
            trace!("drop CioListener:{}", self.getfd());
            cio_sys::cio_listener_drop(self.listener);
        }
    }
}

impl CioWrapper for CioListener {
    fn getfd(&self) -> i32 {
        return self.getfd();
    }

    fn get_wrapper(&self) -> *mut c_void {
        return self.listener as *mut c_void;
    }
}

impl CioListener {
    pub fn bind(addr: &str) -> Result<CioListener, Error> {
        let addr = CString::new(addr).unwrap();
        unsafe {
            let listener = cio_sys::cio_listener_bind(addr.as_ptr());
            if listener.is_null() {
                Err(Error::last_os_error())
            } else {
                Ok(CioListener { listener: listener })
            }
        }
    }

    pub fn getfd(&self) -> i32 {
        unsafe { return cio_sys::cio_listener_getfd(self.listener); }
    }

    pub fn accept(&self) -> Result<CioStream, Error> {
        unsafe {
            let stream = cio_sys::cio_listener_accept(self.listener);
            if stream.is_null() {
                Err(Error::last_os_error())
            } else {
                Ok(CioStream { stream: stream, auto_drop: true })
            }
        }
    }
}

/**
 * CioEvent
 */

pub struct CioEvent {
    pub ev: *mut cio_sys::cio_event,
}

impl CioEvent {
    pub fn is_readable(&self) -> bool {
        unsafe {
            if cio_sys::cioe_is_readable(self.ev) == 1 {
                true
            } else {
                false
            }
        }
    }

    pub fn is_writable(&self) -> bool {
        unsafe {
            if cio_sys::cioe_is_writable(self.ev) == 1 {
                true
            } else {
                false
            }
        }
    }

    pub fn get_token(&self) -> i32 {
        unsafe { return cio_sys::cioe_get_token(self.ev); }
    }

    pub fn getfd(&self) -> i32 {
        unsafe { return cio_sys::cioe_getfd(self.ev); }
    }
}

/**
 * Cio
 */

pub struct Cio {
    pub ctx: *mut cio_sys::cio,
}

unsafe impl Send for Cio {}
unsafe impl Sync for Cio {}

impl Drop for Cio {
    fn drop(&mut self) {
        unsafe {
            trace!("drop Cio");
            cio_sys::cio_drop(self.ctx);
        }
    }
}

impl Cio {
    pub fn new() -> Result<Cio, Error> {
        unsafe {
            let ctx = cio_sys::cio_new();
            if ctx.is_null() {
                Err(Error::last_os_error())
            } else {
                Ok(Cio { ctx: ctx })
            }
        }
    }

    pub fn register<T>(&self, wr: &T, token: i32, flags: i32) -> i32
    where T: CioWrapper,
    {
        unsafe {
            return cio_sys::cio_register(
                self.ctx, wr.getfd(), token, flags, wr.get_wrapper());
        }
    }

    pub fn unregister<T>(&self, wr: &T) -> i32
    where T: CioWrapper,
    {
        unsafe { return cio_sys::cio_unregister(self.ctx, wr.getfd()); }
    }

    pub fn poll(&self, usec: u64) -> i32 {
        unsafe {
            return cio_sys::cio_poll(self.ctx, usec);
        }
    }

    pub fn cio_iter(&self) -> Option<CioEvent> {
        unsafe {
            let ev = cio_sys::cio_iter(self.ctx);
            if ev.is_null() {
                None
            } else {
                Some(CioEvent { ev: ev })
            }
        }
    }
}
