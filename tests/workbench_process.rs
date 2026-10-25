//! Runs the `gsmtap-workbench` binary to check how it stops and reports errors.
#![cfg(unix)]

use std::io::{Read, Write};
use std::net::{TcpListener, TcpStream};
use std::process::{Child, Command, Stdio};
use std::time::{Duration, Instant};

const CONFIG_VARS: &[&str] = &[
    "GSMTAP_MODE",
    "GSMTAP_LISTEN",
    "GSMTAP_FORWARD",
    "HTTP_LISTEN",
    "PACKET_HISTORY_CAPACITY",
    "PACKET_INGRESS_CAPACITY",
    "GSMTAP_REFERENCE_VECTORS",
];

fn workbench(http_listen: &str, gsmtap_listen: &str) -> Command {
    let mut command = Command::new(env!("CARGO_BIN_EXE_gsmtap-workbench"));
    for name in CONFIG_VARS {
        command.env_remove(name);
    }
    command
        .env("HTTP_LISTEN", http_listen)
        .env("GSMTAP_LISTEN", gsmtap_listen)
        .stdout(Stdio::null())
        .stderr(Stdio::piped());
    command
}

fn free_port() -> u16 {
    TcpListener::bind("127.0.0.1:0")
        .unwrap()
        .local_addr()
        .unwrap()
        .port()
}

/// Waits until the server answers a request, so its signal handlers are set.
fn wait_until_serving(port: u16, child: &mut Child) {
    let deadline = Instant::now() + Duration::from_secs(10);
    while Instant::now() < deadline {
        assert!(
            child.try_wait().unwrap().is_none(),
            "workbench exited early"
        );
        if let Ok(mut stream) = TcpStream::connect(("127.0.0.1", port)) {
            stream
                .write_all(
                    b"GET /api/status HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n",
                )
                .unwrap();
            let mut response = String::new();
            if stream.read_to_string(&mut response).is_ok() && response.starts_with("HTTP/1.1 200")
            {
                return;
            }
        }
        std::thread::sleep(Duration::from_millis(50));
    }
    panic!("workbench did not start serving");
}

fn wait_with_timeout(child: &mut Child, timeout: Duration) -> std::process::ExitStatus {
    let deadline = Instant::now() + timeout;
    loop {
        if let Some(status) = child.try_wait().unwrap() {
            return status;
        }
        if Instant::now() >= deadline {
            child.kill().unwrap();
            panic!("workbench did not exit within {timeout:?}");
        }
        std::thread::sleep(Duration::from_millis(20));
    }
}

#[test]
fn sigterm_stops_the_workbench_cleanly() {
    let port = free_port();
    let mut child = workbench(&format!("127.0.0.1:{port}"), "127.0.0.1:0")
        .spawn()
        .unwrap();
    wait_until_serving(port, &mut child);
    // An open event stream must not keep the process alive.
    let mut events = TcpStream::connect(("127.0.0.1", port)).unwrap();
    events
        .write_all(b"GET /api/events HTTP/1.1\r\nHost: localhost\r\n\r\n")
        .unwrap();

    let killed = Command::new("kill")
        .args(["-TERM", &child.id().to_string()])
        .status()
        .unwrap();
    assert!(killed.success());
    let status = wait_with_timeout(&mut child, Duration::from_secs(5));
    assert!(status.success(), "{status:?}");
}

/// Runs a workbench that must fail at startup and returns its stderr.
fn startup_error(command: &mut Command) -> String {
    let output = command.output().unwrap();
    assert_eq!(output.status.code(), Some(1));
    String::from_utf8(output.stderr).unwrap()
}

#[test]
fn startup_errors_are_one_readable_line() {
    let missing = std::env::temp_dir().join("gsmtap-rs-no-such-vectors");
    let stderr = startup_error(
        workbench("127.0.0.1:0", "127.0.0.1:0")
            .args(["--reference-vectors".as_ref(), missing.as_os_str()]),
    );
    assert!(
        stderr.starts_with(&format!(
            "gsmtap-workbench: cannot read {}:",
            missing.display()
        )),
        "{stderr}"
    );
    assert_eq!(stderr.lines().count(), 1, "{stderr}");
    assert!(!stderr.contains("Custom"), "{stderr}");

    // 192.0.2.0/24 is reserved for documentation, so no host interface has it.
    let stderr = startup_error(&mut workbench("127.0.0.1:0", "192.0.2.1:4729"));
    assert!(
        stderr.starts_with("gsmtap-workbench: cannot listen for GSMTAP on 192.0.2.1:4729:"),
        "{stderr}"
    );
    assert_eq!(stderr.lines().count(), 1, "{stderr}");

    let busy = TcpListener::bind("127.0.0.1:0").unwrap();
    let address = busy.local_addr().unwrap();
    let stderr = startup_error(&mut workbench(&address.to_string(), "127.0.0.1:0"));
    assert!(
        stderr.starts_with(&format!(
            "gsmtap-workbench: cannot serve HTTP on {address}:"
        )),
        "{stderr}"
    );
}
