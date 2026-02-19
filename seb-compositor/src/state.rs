//! Kiosk compositor state: Wayland protocol state, seat, space, and app lifecycle.

use std::cell::RefCell;
use std::os::unix::net::UnixStream;
use std::rc::Rc;

use calloop::LoopSignal;
use smithay::desktop::{Space, Window};
use smithay::input::keyboard::KeyboardHandle;
use smithay::input::pointer::CursorImageStatus;
use smithay::reexports::wayland_server::backend::ClientData;
use smithay::reexports::wayland_server::protocol::wl_surface::WlSurface;
use smithay::reexports::wayland_server::DisplayHandle;
use smithay::wayland::compositor::{CompositorClientState, CompositorState};
use smithay::wayland::output::OutputManagerState;
use smithay::wayland::shell::xdg::XdgShellState;
use smithay::wayland::shm::ShmState;
use smithay::{
    delegate_compositor, delegate_output, delegate_seat, delegate_shm, delegate_xdg_shell,
    input::SeatState,
    input::{Seat, SeatHandler},
    wayland::shell::xdg::ToplevelSurface,
};
use xkbcommon::xkb::Keysym;

use crate::udev::UdevData;

/// Per-client data attached to each Wayland client.
#[derive(Default)]
pub struct ClientState {
    pub compositor_state: CompositorClientState,
}

impl ClientData for ClientState {}

/// Main kiosk compositor state.
pub struct KioskState {
    pub backend_data: UdevData,
    pub display_handle: DisplayHandle,
    pub loop_handle: calloop::LoopHandle<'static, KioskState>,
    pub loop_signal: LoopSignal,
    pub compositor_state: CompositorState,
    pub xdg_shell_state: XdgShellState,
    pub shm_state: ShmState,
    pub output_manager_state: OutputManagerState,
    pub seat_state: SeatState<KioskState>,
    pub seat: Seat<KioskState>,
    pub keyboard: Option<KeyboardHandle<KioskState>>,
    pub suppressed_keys: Rc<RefCell<Vec<Keysym>>>,
    pub single_surface: Option<ToplevelSurface>,
    pub space: Space<Window>,
    pub child_process: Option<nix::unistd::Pid>,
    pub app_cmd: Vec<String>,
    /// New client streams; processed by display dispatch callback in udev.
    pub pending_client_streams: Vec<UnixStream>,
}

impl KioskState {
    pub fn new(
        display_handle: DisplayHandle,
        loop_handle: calloop::LoopHandle<'static, KioskState>,
        loop_signal: LoopSignal,
        backend_data: UdevData,
        app_cmd: Vec<String>,
    ) -> Self {
        let compositor_state = CompositorState::new::<KioskState>(&display_handle);
        let xdg_shell_state =
            XdgShellState::new::<KioskState>(&display_handle);
        let shm_state = ShmState::new::<KioskState>(&display_handle, Vec::new());
        let output_manager_state = OutputManagerState::new_with_xdg_output::<KioskState>(&display_handle);

        let mut seat_state = SeatState::new();
        let mut seat = seat_state.new_wl_seat(&display_handle, "seat0");
        let keyboard = seat
            .add_keyboard(smithay::input::keyboard::XkbConfig::default(), 200, 25)
            .ok();

        KioskState {
            backend_data,
            display_handle,
            loop_handle,
            loop_signal,
            compositor_state,
            xdg_shell_state,
            shm_state,
            output_manager_state,
            seat_state,
            seat,
            keyboard,
            suppressed_keys: Rc::new(RefCell::new(Vec::new())),
            single_surface: None,
            space: Space::default(),
            child_process: None,
            app_cmd,
            pending_client_streams: Vec::new(),
        }
    }

    /// Launch the kiosk app after first output is ready.
    pub fn launch_app(&mut self) {
        if self.child_process.is_some() {
            return;
        }
        if self.backend_data.backends.is_empty() {
            return;
        }
        let first_backend = self.backend_data.backends.values().next().unwrap();
        if first_backend.surfaces.is_empty() {
            return;
        }
        if self.app_cmd.is_empty() {
            return;
        }
        match std::process::Command::new(&self.app_cmd[0])
            .args(self.app_cmd.iter().skip(1))
            .env("WAYLAND_DISPLAY", std::env::var("WAYLAND_DISPLAY").unwrap_or_default())
            .spawn()
        {
            Ok(child) => {
                self.child_process = Some(nix::unistd::Pid::from_raw(child.id() as i32));
                tracing::info!("Launched app: {:?}", self.app_cmd);
            }
            Err(e) => {
                tracing::error!("Failed to launch app: {:?}: {}", self.app_cmd, e);
            }
        }
    }

    /// Shutdown child: SIGTERM, wait up to 5s, then SIGKILL.
    pub fn shutdown_child(&mut self) {
        if let Some(pid) = self.child_process.take() {
            let _ = nix::sys::signal::kill(pid, nix::sys::signal::SIGTERM);
            for _ in 0..50 {
                match nix::sys::wait::waitpid(pid, Some(nix::sys::wait::WaitPidFlag::WNOHANG)) {
                    Ok(nix::sys::wait::WaitStatus::StillAlive) => {}
                    Ok(_) => break,
                    Err(_) => break,
                }
                std::thread::sleep(std::time::Duration::from_millis(100));
            }
            let _ = nix::sys::signal::kill(pid, nix::sys::signal::SIGKILL);
        }
    }

    /// Primary output size (first output mode) or (1920, 1080).
    pub fn primary_output_size(&self) -> (i32, i32) {
        for backend in self.backend_data.backends.values() {
            for surface in backend.surfaces.values() {
                if let Some(mode) = surface.output.current_mode() {
                    let size = mode.size;
                    return (size.w, size.h);
                }
            }
        }
        (1920, 1080)
    }

    /// Schedule render for all (node, crtc) surfaces via insert_idle.
    pub fn schedule_render(&mut self) {
        for (node, backend) in &self.backend_data.backends {
            for (crtc, _) in &backend.surfaces {
                let node = *node;
                let crtc = *crtc;
                self.loop_handle.insert_idle(move |state: &mut KioskState| {
                    state.render_surface(node, crtc);
                });
            }
        }
    }

    /// If the single window is gone, relaunch the app (no-op if child still running).
    pub fn check_and_relaunch(&mut self) {
        if self.single_surface.is_none() && !self.app_cmd.is_empty() {
            self.launch_app();
        }
    }
}

impl SeatHandler for KioskState {
    type KeyboardFocus = WlSurface;
    type PointerFocus = WlSurface;
    type TouchFocus = WlSurface;

    fn seat_state(&mut self) -> &mut SeatState<KioskState> {
        &mut self.seat_state
    }

    fn focus_changed(&mut self, _seat: &Seat<KioskState>, _focused: Option<&WlSurface>) {}

    fn cursor_image(&mut self, _seat: &Seat<KioskState>, _image: CursorImageStatus) {
        // No-op: no cursor drawing in minimal kiosk
    }
}

impl smithay::wayland::shm::ShmHandler for KioskState {
    fn shm_state(&self) -> &ShmState {
        &self.shm_state
    }
}

impl smithay::wayland::buffer::BufferHandler for KioskState {
    fn buffer_destroyed(
        &mut self,
        _buffer: &smithay::reexports::wayland_server::protocol::wl_buffer::WlBuffer,
    ) {
    }
}

impl smithay::wayland::output::OutputHandler for KioskState {}

delegate_compositor!(KioskState);
delegate_xdg_shell!(KioskState);
delegate_seat!(KioskState);
delegate_output!(KioskState);
delegate_shm!(KioskState);
