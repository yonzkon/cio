use std::sync::Mutex;
use std::collections::HashMap;
use log::{info, error};
use clap::Parser;

static EXIT_FLANG: Mutex<i32> = Mutex::new(0);

#[derive(Parser)]
#[command(author, version, about, long_about = None)]
struct Args {
    #[arg(short, long, action = clap::ArgAction::Count)]
    debug: u8,
}

fn main() {
    // parse args
    let args = Args::parse();
    match args.debug {
        0 => {
            std::env::set_var("RUST_LOG", "info");
            info!("Debug mode is off");
        }
        1 => {
            std::env::set_var("RUST_LOG", "debug");
            info!("Debug mode is on");
        }
        2 => {
            std::env::set_var("RUST_LOG", "trace");
            info!("Trace mode is on");
        }
        _ => info!("Don't be crazy"),
    }

    // logger init
    simple_logger::SimpleLogger::new().env().init().unwrap();

    // unix_server init
    let unix_server = cio::CioListener::bind("unix://tmp/cio-echo-server")
        .expect("listen unix failed");

    // tcp_server init
    let tcp_server = cio::CioListener::bind("tcp://127.0.0.1:6000")
        .expect("listen tcp failed");

    // cio init
    let ctx = cio::Cio::new().unwrap();
    ctx.register(&unix_server, unix_server.fd, cio::CioFlag::READABLE);
    ctx.register(&tcp_server, tcp_server.fd, cio::CioFlag::READABLE);

    // cio accepted streams init
    let mut streams: HashMap<i32, cio::CioStream> = HashMap::new();

    // signal
    ctrlc::set_handler(move || {
        *EXIT_FLANG.lock().unwrap() = 1;
    }).expect("Error setting Ctrl-C handler");

    loop {
        if *EXIT_FLANG.lock().unwrap() == 1 {
            break;
        }

        assert!(ctx.poll(100 * 1000) != -1);

        while let Some(ev) = ctx.cio_iter() {
            info!("token:{}, readable:{}, writable:{}",
                  ev.get_token(), ev.is_readable(), ev.is_writable());
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
                                info!("nr:{}, buf:{}", nr, std::str::from_utf8(&buf).unwrap());
                                stream.send(&buf);
                            }
                        } else {
                            error!("unkown token:{}", ev.get_token());
                        }
                    }
                },
            }
        }
    }
}
