# Cio

To write a echo server in C.
```c
const int TOKEN_LISTENER = 1;
const int TOKEN_STREAM = 2;

struct cio_listener *listener = tcp_listener_bind("127.0.0.1:60000");
assert_true(listener);

struct cio *ctx = cio_new();
cio_register(ctx, cio_listener_get_fd(listener),
             TOKEN_LISTENER, CIOF_READABLE, listener);

for (;;) {
    assert_true(cio_poll(ctx, 100 * 1000) == 0);
    for (;;) {
        struct cio_event *ev = cio_iter(ctx);
        if (!ev) break;
        printf("[server:iter]: fetch a event on server: token:%d, read:%d, write:%d\n",
                cioe_get_token(ev), cioe_is_readable(ev), cioe_is_writable(ev));
        switch (cioe_get_token(ev)) {
            case TOKEN_LISTENER: {
                struct cio_listener *listener = cioe_get_wrapper(ev);
                if (cioe_is_readable(ev)) {
                    struct cio_stream *new_stream = cio_listener_accept(listener);
                    cio_register(ctx, cio_stream_get_fd(new_stream),
                                 TOKEN_STREAM, CIOF_READABLE | CIOF_WRITABLE, new_stream);
                }
                break;
            }
            case TOKEN_STREAM: {
                struct cio_stream *stream = cioe_get_wrapper(ev);
                if (cioe_is_readable(ev)) {
                    char buf[256] = {0};
                    int nr = cio_stream_recv(stream, buf, sizeof(buf));
                    if (nr == 0 || nr == -1) {
                        printf("[server:recv]: nr:%d, client fin\n", nr);
                        cio_unregister(ctx, cio_stream_get_fd(stream));
                        cio_stream_drop(stream);
                    } else {
                        printf("[server:recv]: nr:%d, buf:%s\n", nr, buf);
                        if (cioe_is_writable(ev)) {
                            cio_stream_send(stream, buf, nr);
                            printf("[server:send]: nr:%d, buf:%s\n", nr, buf);
                        }
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
// unix_server init
let unix_server = cio::CioListener::unix_bind("/tmp/cio")
    .expect("listen unix socket failed");

// tcp_server init
let tcp_server = cio::CioListener::tcp_bind("127.0.0.1:60000")
    .expect("listen tcp socket failed");

// cio init
let ctx = cio::Cio::new().unwrap();
ctx.register(&unix_server, unix_server.fd, cio::CioFlag::READABLE);
ctx.register(&tcp_server, tcp_server.fd, cio::CioFlag::READABLE);

// cio accepted streams init
let mut streams: HashMap<i32, cio::CioStream> = HashMap::new();

// main loop
loop {
    assert!(ctx.poll(100 * 1000) != -1);

    loop {
        if let Some(ev) = ctx.cio_iter() {
            info!("token:{}, readable:{}, writable:{}",
                  ev.get_token(), ev.is_readable(), ev.is_writable());
            match ev.get_token() {
                x if x == unix_server.fd => {
                    if ev.is_readable() {
                        let new_stream = unix_server.accept().expect("accept failed");
                        ctx.register(&new_stream, new_stream.fd,
                                     cio::CioFlag::READABLE |
                                     cio::CioFlag::WRITABLE);
                        streams.insert(new_stream.fd, new_stream);
                    }
                },
                x if x == tcp_server.fd => {
                    if ev.is_readable()  {
                        let new_stream = tcp_server.accept().expect("accept failed");
                        ctx.register(&new_stream, new_stream.fd,
                                     cio::CioFlag::READABLE |
                                     cio::CioFlag::WRITABLE);
                        streams.insert(new_stream.fd, new_stream);
                    }
                },
                _ => {
                    if ev.is_readable() {
                        if let Some(stream) = streams.get(&ev.get_token()) {
                            let mut buf: [u8;256] = [0;256];
                            let nr = stream.recv(&mut buf);
                            if nr == 0 || nr == -1 {
                                let stream = streams.remove(&ev.get_token())
                                    .expect("stream not in hashmap");
                                ctx.unregister(&stream);
                            } else {
                                if ev.is_writable() {
                                    info!("nr:{}, buf:{}",
                                          nr, std::str::from_utf8(&buf).unwrap());
                                    stream.send(&buf);
                                }
                            }
                        } else {
                            error!("unkown token:{}", ev.get_token());
                        }
                    }
                },
            }
        } else {
            break;
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
