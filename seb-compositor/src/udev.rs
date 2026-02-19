//! Udev/DRM backend: session, GPU manager, device add/change, render.

use std::collections::HashMap;
use std::path::Path;
use std::sync::Arc;
use std::time::Duration;

use calloop::generic::Generic;
use calloop::{EventLoop, Interest, Mode};
use smithay::backend::allocator::gbm::{GbmAllocator, GbmBufferFlags, GbmDevice};
use smithay::backend::allocator::Fourcc;
use smithay::backend::drm::{
    compositor::FrameFlags,
    output::{DrmOutput, DrmOutputManager, DrmOutputRenderElements},
    CreateDrmNodeError, DrmDevice, DrmDeviceFd, DrmError, DrmEvent, DrmNode, NodeType,
};
use smithay::backend::drm::exporter::gbm::GbmFramebufferExporter;
use smithay::backend::egl::{EGLDevice, EGLDisplay};
use smithay::backend::libinput::LibinputInputBackend;
use smithay::backend::renderer::multigpu::{gbm::GbmGlesBackend, GpuManager};
use smithay::backend::session::libseat::LibSeatSession;
use smithay::backend::session::Event as SessionEvent;
use smithay::backend::session::Session;
use smithay::backend::renderer::ImportDma;
use smithay::backend::udev::{primary_gpu, UdevBackend, UdevEvent};
use smithay::backend::renderer::element::surface::WaylandSurfaceRenderElement;
use smithay::desktop::space::SpaceRenderElements;
use smithay::output::{Mode as WlMode, Output, PhysicalProperties};
use smithay::reexports::calloop::RegistrationToken;
use smithay::reexports::drm::control::{connector, crtc, ModeTypeFlags};
use smithay::utils::{DeviceFd, Transform};
use smithay::wayland::socket::ListeningSocketSource;
use smithay_drm_extras::drm_scanner::{DrmScanEvent, DrmScanner};
use tracing::info;

use crate::input;
use crate::state::{ClientState, KioskState};
use crate::RUNNING;

const SUPPORTED_FORMATS: &[Fourcc] = &[
    Fourcc::Abgr2101010,
    Fourcc::Argb2101010,
    Fourcc::Abgr8888,
    Fourcc::Argb8888,
];

pub struct UdevData {
    pub session: LibSeatSession,
    pub gpus: GpuManager<GbmGlesBackend<smithay::backend::renderer::gles::GlesRenderer, DrmDeviceFd>>,
    pub backends: HashMap<DrmNode, BackendData>,
    pub primary_gpu: DrmNode,
}

pub struct BackendData {
    pub drm_output_manager: DrmOutputManager<
        GbmAllocator<DrmDeviceFd>,
        GbmFramebufferExporter<DrmDeviceFd>,
        (),
        DrmDeviceFd,
    >,
    pub drm_scanner: DrmScanner,
    pub surfaces: HashMap<crtc::Handle, SurfaceData>,
    pub render_node: Option<DrmNode>,
    pub registration_token: RegistrationToken,
}

pub struct SurfaceData {
    pub output: Output,
    pub global: smithay::reexports::wayland_server::backend::GlobalId,
    pub drm_output: DrmOutput<
        GbmAllocator<DrmDeviceFd>,
        GbmFramebufferExporter<DrmDeviceFd>,
        (),
        DrmDeviceFd,
    >,
}

#[derive(Debug, thiserror::Error)]
#[allow(clippy::enum_variant_names)]
enum DeviceAddError {
    #[error("Failed to open device: {0}")]
    DeviceOpen(#[from] smithay::backend::session::libseat::Error),
    #[error("DRM device: {0}")]
    DrmDevice(#[from] DrmError),
    #[error("GBM device: {0}")]
    GbmDevice(#[from] std::io::Error),
    #[error("DRM node: {0}")]
    DrmNode(#[from] CreateDrmNodeError),
    #[error("Add node: {0}")]
    AddNode(#[from] smithay::backend::egl::Error),
    #[error("No render node")]
    NoRenderNode,
    #[error("Primary GPU missing")]
    PrimaryGpuMissing,
}

pub fn run_compositor(app_cmd: Vec<String>) -> anyhow::Result<()> {
    let mut event_loop: EventLoop<KioskState> = EventLoop::try_new()?;
    let display = wayland_server::Display::new()?;
    let display_handle = display.handle().clone();

    let (session, notifier) = LibSeatSession::new()?;
    let primary_gpu = if let Ok(path) = std::env::var("SEB_DRM_DEVICE") {
        DrmNode::from_path(&path).map_err(DeviceAddError::DrmNode)?
    } else {
        primary_gpu(session.seat())
            .map_err(|_| DeviceAddError::PrimaryGpuMissing)?
            .and_then(|p| DrmNode::from_path(&p).ok())
            .and_then(|n| n.node_with_type(NodeType::Render).and_then(Result::ok))
            .unwrap_or_else(|| {
                smithay::backend::udev::all_gpus(session.seat())
                    .unwrap()
                    .into_iter()
                    .find_map(|p| DrmNode::from_path(&p).ok())
                    .expect("No GPU")
            })
    };
    info!("Primary GPU: {:?}", primary_gpu);

    let gpus = GpuManager::new(GbmGlesBackend::<smithay::backend::renderer::gles::GlesRenderer, DrmDeviceFd>::default())?;
    let backend_data = UdevData {
        session,
        gpus,
        backends: HashMap::new(),
        primary_gpu,
    };

    let loop_handle = event_loop.handle();
    let loop_signal = event_loop.get_signal();
    let mut state = KioskState::new(
        display_handle.clone(),
        loop_handle.clone(),
        loop_signal,
        backend_data,
        app_cmd,
    );

    let listening_socket = ListeningSocketSource::new_auto()?;
    let socket_name = listening_socket.socket_name().to_string_lossy().into_owned();
    std::env::set_var("WAYLAND_DISPLAY", &socket_name);
    loop_handle
        .insert_source(listening_socket, |stream, _, state| {
            state.pending_client_streams.push(stream);
        })
        .map_err(|e| anyhow::anyhow!("Wayland socket: {}", e))?;

    let display_source = Generic::new(display, Interest::READ, Mode::Level);
        loop_handle
        .insert_source(display_source, |_, display, state| {
            let display = unsafe { display.get_mut() };
            let mut handle = display.handle();
            for stream in state.pending_client_streams.drain(..) {
                let _ = handle.insert_client(stream, Arc::new(ClientState::default()));
            }
            let _ = display.dispatch_clients(state);
            Ok(calloop::PostAction::Continue)
        })
        .map_err(|e| anyhow::anyhow!("Display source: {}", e))?;

    loop_handle
        .insert_source(notifier, |event, &mut (), state| match event {
            SessionEvent::PauseSession => {
                for backend in state.backend_data.backends.values_mut() {
                    backend.drm_output_manager.pause();
                }
            }
            SessionEvent::ActivateSession => {
                let pairs: Vec<_> = state
                    .backend_data
                    .backends
                    .iter_mut()
                    .flat_map(|(node, backend)| {
                        backend.drm_output_manager.activate(false).expect("activate");
                        backend.surfaces.keys().map(move |&crtc| (*node, crtc))
                    })
                    .collect();
                for (node, crtc) in pairs {
                    state.loop_handle.insert_idle(move |s| {
                        s.render_surface(node, crtc);
                    });
                }
            }
        })
        .map_err(|e| anyhow::anyhow!("Session: {}", e))?;

    let libinput_interface = smithay::backend::libinput::LibinputSessionInterface::from(state.backend_data.session.clone());
    let mut libinput = smithay::reexports::input::Libinput::new_with_udev(libinput_interface);
    libinput.udev_assign_seat(&state.backend_data.session.seat()).unwrap();
    let libinput_backend = LibinputInputBackend::new(libinput);
    loop_handle
        .insert_source(libinput_backend, |event, _, state| {
            input::process_input_event(state, event);
        })
        .map_err(|e| anyhow::anyhow!("Libinput: {}", e))?;

    let udev_backend = UdevBackend::new(&state.backend_data.session.seat())?;
    for (device_id, path) in udev_backend.device_list() {
        if let Ok(node) = DrmNode::from_dev_id(device_id) {
            let _ = state.device_added(node, Path::new(&path));
        }
    }
    loop_handle
        .insert_source(udev_backend, |event, _, state| match event {
            UdevEvent::Added { device_id, path } => {
                if let Ok(node) = DrmNode::from_dev_id(device_id) {
                    let _ = state.device_added(node, Path::new(&path));
                }
            }
            UdevEvent::Changed { device_id } => {
                if let Ok(node) = DrmNode::from_dev_id(device_id) {
                    state.device_changed(node);
                }
            }
            UdevEvent::Removed { device_id } => {
                if let Ok(node) = DrmNode::from_dev_id(device_id) {
                    state.device_removed(node);
                }
            }
        })
        .map_err(|e| anyhow::anyhow!("Udev: {}", e))?;

    while RUNNING.load(std::sync::atomic::Ordering::SeqCst) {
        let _ = event_loop.dispatch(Some(Duration::from_millis(16)), &mut state);
        state.space.refresh();
        state.display_handle.flush_clients().unwrap();
        if let Some(pid) = state.child_process {
            if let Ok(status) =
                nix::sys::wait::waitpid(pid, Some(nix::sys::wait::WaitPidFlag::WNOHANG))
            {
                if !matches!(status, nix::sys::wait::WaitStatus::StillAlive) {
                    state.child_process = None;
                }
            }
        }
        state.check_and_relaunch();
    }

    state.shutdown_child();
    Ok(())
}

impl KioskState {
    fn device_added(&mut self, node: DrmNode, path: &Path) -> Result<(), DeviceAddError> {
        use smithay::reexports::rustix::fs::OFlags;

        let fd = self.backend_data.session.open(
            path,
            OFlags::RDWR | OFlags::CLOEXEC | OFlags::NONBLOCK,
        )?;
        let fd = DrmDeviceFd::new(DeviceFd::from(fd));
        let (drm, notifier) = DrmDevice::new(fd.clone(), true)?;
        let gbm = GbmDevice::new(fd)?;

        let node = node;
        let registration_token = self.loop_handle.insert_source(notifier, move |event, _, data| {
            if let DrmEvent::VBlank(crtc) = event {
                data.frame_submitted(node, crtc);
            }
        }).unwrap();

        let mut try_gpu = || {
            let display = unsafe { EGLDisplay::new(gbm.clone())? };
            let egl_device = EGLDevice::device_for_display(&display)?;
            if egl_device.is_software() {
                return Err(DeviceAddError::NoRenderNode);
            }
            let render_node = egl_device
                .try_get_render_node()
                .ok()
                .flatten()
                .unwrap_or(node);
            self.backend_data.gpus.as_mut().add_node(render_node, gbm.clone())?;
            Ok::<_, DeviceAddError>(render_node)
        };
        let render_node = try_gpu().ok();

        let allocator = GbmAllocator::new(
            gbm.clone(),
            GbmBufferFlags::RENDERING | GbmBufferFlags::SCANOUT,
        );
        let framebuffer_exporter = GbmFramebufferExporter::new(gbm.clone(), render_node);

        let render_node_or_primary = render_node.unwrap_or(self.backend_data.primary_gpu);
        let renderer = self
            .backend_data
            .gpus
            .single_renderer(&render_node_or_primary)
            .map_err(|_| DeviceAddError::PrimaryGpuMissing)?;
        let render_formats = renderer.dmabuf_formats();
        let drm_output_manager = DrmOutputManager::new(
            drm,
            allocator,
            framebuffer_exporter,
            Some(gbm),
            SUPPORTED_FORMATS.iter().copied(),
            render_formats.iter().cloned(),
        );

        self.backend_data.backends.insert(
            node,
            BackendData {
                drm_output_manager,
                drm_scanner: DrmScanner::new(),
                surfaces: HashMap::new(),
                render_node,
                registration_token,
            },
        );
        self.device_changed(node);
        Ok(())
    }

    fn device_changed(&mut self, node: DrmNode) {
        let backend = match self.backend_data.backends.get_mut(&node) {
            Some(b) => b,
            None => return,
        };
        let device = backend.drm_output_manager.device();
        if let Ok(scan_result) = backend.drm_scanner.scan_connectors(device) {
        for event in scan_result {
            match event {
                DrmScanEvent::Connected { connector, crtc } => {
                    if let Some(crtc) = crtc {
                        self.connector_connected(node, connector, crtc);
                    }
                }
                DrmScanEvent::Disconnected { connector, crtc } => {
                    if let Some(crtc) = crtc {
                        self.connector_disconnected(node, connector, crtc);
                    }
                }
            }
        }
        }
    }

    fn connector_connected(
        &mut self,
        node: DrmNode,
        connector: connector::Info,
        crtc: crtc::Handle,
    ) {
        let backend = match self.backend_data.backends.get_mut(&node) {
            Some(b) => b,
            None => return,
        };
        let render_node = backend.render_node.unwrap_or(self.backend_data.primary_gpu);
        let mut renderer = self.backend_data.gpus.single_renderer(&render_node).unwrap();

        let mode_id = connector
            .modes()
            .iter()
            .position(|m| m.mode_type().contains(ModeTypeFlags::PREFERRED))
            .unwrap_or(0);
        let drm_mode = connector.modes()[mode_id];
        let wl_mode = WlMode::from(drm_mode);

        let (phys_w, phys_h) = connector.size().unwrap_or((0, 0));
        let name = format!("{}-{}", connector.interface().as_str(), connector.interface_id());
        let output = Output::new(
            name,
            PhysicalProperties {
                size: (phys_w as i32, phys_h as i32).into(),
                subpixel: connector.subpixel().into(),
                make: "SEB".into(),
                model: "Kiosk".into(),
            },
        );
        let global = output.create_global::<KioskState>(&self.display_handle);
        output.set_preferred(wl_mode);
        output.change_current_state(Some(wl_mode), Some(Transform::Normal), None, Some((0, 0).into()));
        self.space.map_output(&output, (0, 0));

        let empty_elements: DrmOutputRenderElements<
            _,
            SpaceRenderElements<_, WaylandSurfaceRenderElement<_>>,
        > = DrmOutputRenderElements::default();
        let drm_output = backend
            .drm_output_manager
            .initialize_output(
                crtc,
                drm_mode,
                &[connector.handle()],
                &output,
                None,
                &mut renderer,
                &empty_elements,
            )
            .expect("initialize_output");

        backend.surfaces.insert(
            crtc,
            SurfaceData {
                output,
                global,
                drm_output,
            },
        );
        self.launch_app();
        let node = node;
        self.loop_handle.insert_idle(move |s| {
            s.render_surface(node, crtc);
        });
    }

    fn connector_disconnected(
        &mut self,
        node: DrmNode,
        _connector: connector::Info,
        crtc: crtc::Handle,
    ) {
        if let Some(backend) = self.backend_data.backends.get_mut(&node) {
            if let Some(surf) = backend.surfaces.remove(&crtc) {
                self.display_handle.remove_global::<KioskState>(surf.global.clone());
                self.space.unmap_output(&surf.output);
            }
        }
    }

    fn frame_submitted(&mut self, node: DrmNode, crtc: crtc::Handle) {
        self.loop_handle.insert_idle(move |s| {
            s.render_surface(node, crtc);
        });
    }

    fn device_removed(&mut self, _node: DrmNode) {}

    pub fn render_surface(&mut self, node: DrmNode, crtc: crtc::Handle) {
        let render_node = match self.backend_data.backends.get(&node) {
            Some(b) => b.render_node.unwrap_or(self.backend_data.primary_gpu),
            None => return,
        };
        let mut surface_data = match self.backend_data.backends.get_mut(&node) {
            Some(b) => match b.surfaces.remove(&crtc) {
                Some(s) => s,
                None => return,
            },
            None => return,
        };
        let output = &surface_data.output;
        let mut renderer = match self.backend_data.gpus.single_renderer(&render_node) {
            Ok(r) => r,
            Err(_) => {
                self.backend_data.backends.get_mut(&node).unwrap().surfaces.insert(crtc, surface_data);
                return;
            }
        };
        let elements = self
            .space
            .render_elements_for_output(&mut renderer, output, 1.0)
            .unwrap();
        let result = surface_data.drm_output.render_frame(
            &mut renderer,
            &elements,
            [0.0, 0.0, 0.0, 1.0],
            FrameFlags::empty(),
        );
        if let Ok(ref res) = result {
            if !res.is_empty {
                let _ = surface_data.drm_output.queue_frame(());
            }
        }
        let now = std::time::Duration::from_millis(
            std::time::Instant::now().elapsed().as_millis() as u64,
        );
        if let Some(ref toplevel) = self.single_surface {
            let window = smithay::desktop::Window::new_wayland_window(toplevel.clone());
            window.send_frame(output, now, None, |_, _| None);
        }
        self.backend_data.backends.get_mut(&node).unwrap().surfaces.insert(crtc, surface_data);
    }
}

impl Drop for SurfaceData {
    fn drop(&mut self) {}
}
