mod compositor;
mod input;
mod state;
mod udev;

use std::sync::atomic::{AtomicBool, Ordering};
use tracing_subscriber::EnvFilter;

/// Global running flag — set to false by SIGTERM/SIGINT to exit the event loop.
pub static RUNNING: AtomicBool = AtomicBool::new(true);

fn main() -> anyhow::Result<()> {
    // Initialize structured logging
    tracing_subscriber::fmt()
        .with_env_filter(
            EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| EnvFilter::new("seb_compositor=debug,smithay=warn")),
        )
        .init();

    // Parse CLI: seb-compositor --launch <command> [args...]
    let args: Vec<String> = std::env::args().collect();
    let launch_idx = args
        .iter()
        .position(|a| a == "--launch")
        .expect("Usage: seb-compositor --launch <command> [args...]");
    let app_cmd = args[launch_idx + 1..].to_vec();
    if app_cmd.is_empty() {
        anyhow::bail!("--launch requires a command argument");
    }

    // Install signal handlers — only write to AtomicBool (async-signal-safe)
    install_signal_handlers();

    tracing::info!("Starting SEB kiosk compositor, launching: {:?}", app_cmd);
    udev::run_compositor(app_cmd)
}

fn install_signal_handlers() {
    extern "C" fn handle_signal(_: libc::c_int) {
        RUNNING.store(false, Ordering::SeqCst);
    }
    unsafe {
        libc::signal(libc::SIGTERM, handle_signal as *const () as libc::sighandler_t);
        libc::signal(libc::SIGINT, handle_signal as *const () as libc::sighandler_t);
    }
}
