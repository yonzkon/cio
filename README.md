# Cio

To write a echo server in C.
```c
const int TOKEN_LISTENER = 1;
const int TOKEN_STREAM = 2;

struct cio_listener *listener = tcp_listener_bind("127.0.0.1:6000");
struct cio *ctx = cio_new();
cio_register(ctx, cio_listener_get_fd(listener), TOKEN_LISTENER, CIOF_READABLE, listener);

for (;;) {
    assert(cio_poll(ctx, 100 * 1000) == 0);
    struct cio_event *ev;
    while ((ev = cio_iter(ctx))) {
        switch (cioe_get_token(ev)) {
            case TOKEN_LISTENER: {
                struct cio_listener *listener = cioe_get_wrapper(ev);
                if (cioe_is_readable(ev)) {
                    struct cio_stream *new_stream = cio_listener_accept(listener);
                    cio_register(ctx, cio_stream_get_fd(new_stream), TOKEN_STREAM,
                                 CIOF_READABLE | CIOF_WRITABLE, new_stream);
                }
                break;
            }
            case TOKEN_STREAM: {
                struct cio_stream *stream = cioe_get_wrapper(ev);
                if (cioe_is_readable(ev)) {
                    char buf[256] = {0};
                    int nr = cio_stream_recv(stream, buf, sizeof(buf));
                    if (nr == 0 || nr == -1) {
                        cio_unregister(ctx, cio_stream_get_fd(stream));
                        cio_stream_drop(stream);
                    } else if (cioe_is_writable(ev)) {
                        cio_stream_send(stream, buf, nr);
                    }
                }
                break;
            }
        }
    }
}

cio_unregister(ctx, cio_listener_get_fd(listener));
cio_listener_drop(listener);
cio_drop(ctx);
```

To write a echo server in Rust.
```rust
// socket init
let unix_server = cio::CioListener::unix_bind("/tmp/cio").expect("listen unix failed");
let tcp_server = cio::CioListener::tcp_bind("127.0.0.1:6000").expect("listen tcp failed");

// cio init
let ctx = cio::Cio::new().unwrap();
ctx.register(&unix_server, unix_server.fd, cio::CioFlag::READABLE);
ctx.register(&tcp_server, tcp_server.fd, cio::CioFlag::READABLE);

// cio accepted streams init
let mut streams: HashMap<i32, cio::CioStream> = HashMap::new();

loop {
    assert!(ctx.poll(100 * 1000) != -1);
    while let Some(ev) = ctx.cio_iter() {
        match ev.get_token() {
            x if x == unix_server.fd => {
                if ev.is_readable() {
                    let new_stream = unix_server.accept().expect("accept failed");
                    ctx.register(&new_stream, new_stream.fd,
                                 cio::CioFlag::READABLE | cio::CioFlag::WRITABLE);
                    streams.insert(new_stream.fd, new_stream);
                }
            },
            x if x == tcp_server.fd => {
                if ev.is_readable()  {
                    let new_stream = tcp_server.accept().expect("accept failed");
                    ctx.register(&new_stream, new_stream.fd,
                                 cio::CioFlag::READABLE | cio::CioFlag::WRITABLE);
                    streams.insert(new_stream.fd, new_stream);
                }
            },
            _ => {
                if ev.is_readable() {
                    if let Some(stream) = streams.get(&ev.get_token()) {
                        let mut buf: [u8;256] = [0;256];
                        let nr = stream.recv(&mut buf);
                        if nr == 0 || nr == -1 {
                            ctx.unregister(&streams.remove(&ev.get_token()).unwrap());
                        } else if ev.is_writable() {
                            stream.send(&buf);
                        }
                    }
                }
            },
        }
    }
}
```

[![Build status](https://ci.appveyor.com/api/projects/status/vdnrs758uowwi243?svg=true)](https://ci.appveyor.com/project/yonzkon/cio)

Lightweight stream based io framework.

## Supported platforms

- Linux
- MacOS
- MinGW

## Build
```
mkdir build && cd build
cmake ..
make && make install
```
