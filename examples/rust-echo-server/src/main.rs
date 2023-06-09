use std::sync::Mutex;
use std::collections::HashMap;
use log::{debug, info, error};
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

    // signal
    ctrlc::set_handler(move || {
        *EXIT_FLANG.lock().unwrap() = 1;
    }).expect("Error setting Ctrl-C handler");

    // stream init
    let tcp_server = cio::CioListener::bind("tcp://127.0.0.1:6000").expect("listen tcp failed");
    let unix_server = cio::CioListener::bind("unix:///tmp/cio").expect("listen unix failed");
    let com_conn = cio::CioStream::connect(
        "com:///dev/ttyUSB0?baud=115200&data_bit=8&stop_bit=1&parity=n")
        .expect("connect com failed");

    // cio init
    let ctx = cio::Cio::new().unwrap();
    ctx.register(&tcp_server, tcp_server.getfd(), cio::CioFlag::READABLE);
    ctx.register(&unix_server, unix_server.getfd(), cio::CioFlag::READABLE);
    ctx.register(&com_conn, com_conn.getfd(), cio::CioFlag::READABLE | cio::CioFlag::WRITABLE);

    // accepted streams init
    let mut streams: HashMap<i32, cio::CioStream> = HashMap::new();
    streams.insert(com_conn.getfd(), com_conn);

    loop {
        if *EXIT_FLANG.lock().unwrap() == 1 {
            break;
        }

        assert!(ctx.poll(100 * 1000) != -1);
        while let Some(ev) = ctx.cio_iter() {
            debug!("token:{}, readable:{}, writable:{}",
                  ev.get_token(), ev.is_readable(), ev.is_writable());
            match ev.get_token() {
                x if x == tcp_server.getfd() => {
                    if ev.is_readable()  {
                        let new_stream = tcp_server.accept().expect("accept failed");
                        ctx.register(&new_stream, new_stream.getfd(),
                                     cio::CioFlag::READABLE | cio::CioFlag::WRITABLE);
                        streams.insert(new_stream.getfd(), new_stream);
                    }
                },
                x if x == unix_server.getfd() => {
                    if ev.is_readable() {
                        let new_stream = unix_server.accept().expect("accept failed");
                        ctx.register(&new_stream, new_stream.getfd(),
                                     cio::CioFlag::READABLE | cio::CioFlag::WRITABLE);
                        streams.insert(new_stream.getfd(), new_stream);
                    }
                },
                _ => {
                    if ev.is_readable() {
                        if let Some(stream) = streams.get(&ev.get_token()) {
                            let mut buf: [u8;256] = [0;256];
                            let nr = stream.recv(&mut buf);
                            info!("nr:{}, recv:{}", nr, std::str::from_utf8(&buf).unwrap());
                            if nr == 0 || nr == -1 {
                                ctx.unregister(&streams.remove(&ev.get_token()).unwrap());
                            } else if ev.is_writable() {
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
