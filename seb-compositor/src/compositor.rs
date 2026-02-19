//! Compositor and XDG shell handlers: single fullscreen window, deny extra toplevels.

use smithay::desktop::Window;
use smithay::reexports::wayland_server::protocol::wl_surface::WlSurface;
use smithay::utils::Serial;
use smithay::wayland::shell::xdg::{PopupSurface, PositionerState, ToplevelSurface};
use smithay::wayland::shell::xdg::ToplevelState;
use wayland_protocols::xdg::shell::server::xdg_toplevel::State as XdgToplevelState;

use crate::state::KioskState;

impl smithay::wayland::compositor::CompositorHandler for KioskState {
    fn compositor_state(&mut self) -> &mut smithay::wayland::compositor::CompositorState {
        &mut self.compositor_state
    }

    fn client_compositor_state<'a>(
        &self,
        client: &'a smithay::reexports::wayland_server::Client,
    ) -> &'a smithay::wayland::compositor::CompositorClientState {
        &client.get_data::<crate::state::ClientState>().unwrap().compositor_state
    }

    fn commit(&mut self, _surface: &WlSurface) {
        if let Some(ref toplevel) = self.single_surface {
            Window::new_wayland_window(toplevel.clone()).on_commit();
        }
        self.schedule_render();
    }
}

impl smithay::wayland::shell::xdg::XdgShellHandler for KioskState {
    fn xdg_shell_state(&mut self) -> &mut smithay::wayland::shell::xdg::XdgShellState {
        &mut self.xdg_shell_state
    }

    fn new_toplevel(&mut self, surface: ToplevelSurface) {
        if self.single_surface.is_some() {
            surface.send_close();
            return;
        }
        let (w, h) = self.primary_output_size();
        let size = (w, h).into();
        surface.with_pending_state(|state: &mut ToplevelState| {
            state.states.set(XdgToplevelState::Fullscreen);
            state.size = Some(size);
        });
        surface.send_configure();
        let window = Window::new_wayland_window(surface.clone());
        self.space
            .map_element(window, (0, 0), true);
        self.single_surface = Some(surface);
    }

    fn toplevel_destroyed(&mut self, surface: ToplevelSurface) {
        if self.single_surface.as_ref().map(|s| s == &surface).unwrap_or(false) {
            let window = Window::new_wayland_window(surface);
            self.space.unmap_elem(&window);
            self.single_surface = None;
            self.check_and_relaunch();
        }
    }

    fn unfullscreen_request(&mut self, surface: ToplevelSurface) {
        let (w, h) = self.primary_output_size();
        surface.with_pending_state(|state: &mut ToplevelState| {
            state.states.set(XdgToplevelState::Fullscreen);
            state.size = Some((w, h).into());
        });
        surface.send_configure();
    }

    fn maximize_request(&mut self, surface: ToplevelSurface) {
        let (w, h) = self.primary_output_size();
        surface.with_pending_state(|state: &mut ToplevelState| {
            state.states.set(XdgToplevelState::Fullscreen);
            state.size = Some((w, h).into());
        });
        surface.send_configure();
    }

    fn minimize_request(&mut self, _surface: ToplevelSurface) {}
    fn move_request(&mut self, _surface: ToplevelSurface, _seat: smithay::reexports::wayland_server::protocol::wl_seat::WlSeat, _serial: Serial) {}
    fn resize_request(
        &mut self,
        _surface: ToplevelSurface,
        _seat: smithay::reexports::wayland_server::protocol::wl_seat::WlSeat,
        _serial: Serial,
        _edges: wayland_protocols::xdg::shell::server::xdg_toplevel::ResizeEdge,
    ) {
    }

    fn new_popup(
        &mut self,
        _surface: PopupSurface,
        _positioner: PositionerState,
    ) {
    }

    fn grab(
        &mut self,
        _surface: PopupSurface,
        _seat: smithay::reexports::wayland_server::protocol::wl_seat::WlSeat,
        _serial: Serial,
    ) {
    }

    fn reposition_request(
        &mut self,
        _surface: PopupSurface,
        _positioner: PositionerState,
        _token: u32,
    ) {
    }
}
