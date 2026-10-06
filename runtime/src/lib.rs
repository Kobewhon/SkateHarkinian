// Original host ABI. The pinned MW2 Session remains the gameplay authority.
// Its large native simulation records require an explicit worker stack; they
// must never be constructed or stepped on Shipwright's 1 MB main thread.
use skate_host::bridge::{Controls, Session};
use std::{
    cell::RefCell,
    ffi::{CStr, CString, c_char},
    panic::{AssertUnwindSafe, catch_unwind},
    path::Path,
    sync::mpsc,
    thread,
};
thread_local! {static ERROR:RefCell<CString>=RefCell::new(CString::new("").unwrap());}
fn error(message: String) {
    ERROR.with(|e| *e.borrow_mut() = CString::new(message.replace('\0', " ")).unwrap());
}
#[repr(C)]
#[derive(Default, Clone, Copy)]
pub struct Packet {
    pub buttons: u16,
    pub triggers: [u8; 2],
    pub left: [i16; 2],
    pub right: [i16; 2],
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Snapshot {
    pub root: [f32; 16],
    pub board: [f32; 16],
    pub state_id: u32,
    pub velocity: [f32; 3],
    pub tick: u64,
    pub state: [u8; 64],
    pub core_intents: [f32; 7],
    pub body_pitch: f32,
    pub body_yaw: f32,
    pub trajectory: [f32; 3],
    pub trajectory_velocity: [f32; 3],
    pub ground_normal: [f32; 3],
    pub heading: f32,
}
impl Default for Snapshot {
    fn default() -> Self {
        Self {
            root: [0.; 16],
            board: [0.; 16],
            state_id: 0,
            velocity: [0.; 3],
            tick: 0,
            state: [0; 64],
            core_intents: [0.; 7],
            body_pitch: 0.,
            body_yaw: 0.,
            trajectory: [0.; 3],
            trajectory_velocity: [0.; 3],
            ground_normal: [0.; 3],
            heading: 0.,
        }
    }
}
fn snapshot(session: &Session) -> Snapshot {
    let (root, velocity, tick, state) = session.host_snapshot();
    let mut out = Snapshot {
        root: root.to_cols_array(),
        board: session.host_board_matrix(),
        state_id: session.host_state_id(),
        velocity: velocity.to_array(),
        tick: tick,
        ..Default::default()
    };
    for (i, name) in [
        "Ollie",
        "Nollie",
        "Kickflip",
        "Heelflip",
        "PopShuvit",
        "FSPopShuvit",
        "360Flip",
    ]
    .iter()
    .enumerate()
    {
        out.core_intents[i] = session.host_intent(name).unwrap_or(0.);
    }
    let (position, velocity, normal, heading) = session.host_trajectory();
    out.trajectory = position;
    out.trajectory_velocity = velocity;
    out.ground_normal = normal;
    out.heading = heading;
    out.body_pitch = session.host_body_pitch();
    out.body_yaw = session.host_body_yaw();
    let bytes = state.as_bytes();
    let count = bytes.len().min(63);
    out.state[..count].copy_from_slice(&bytes[..count]);
    out
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Marker {
    pub version: u32,
    pub onboard: u32,
    pub foot_forward: u32,
    pub reserved: u32,
    pub frame: [f32; 16],
}
mod vert;
type Reply = mpsc::SyncSender<Result<Snapshot, String>>;
// Original worker floating-point environment adaptation. The local Recomp
// SDK's FPSCR host uses FTZ/DAZ; do not modify Shipwright's rendering thread.
#[cfg(target_arch = "x86_64")]
struct HostFloatMode(u32);
#[cfg(target_arch = "x86_64")]
impl HostFloatMode {
    fn enter() -> Self {
        let mut prior = 0u32;
        unsafe {
            std::arch::asm!("stmxcsr [{0}]",in(reg)&mut prior,options(nostack,preserves_flags));
        }
        let mode = (prior | 0x8040) & !0x6000; // FTZ, DAZ, round-to-nearest
        unsafe {
            std::arch::asm!("ldmxcsr [{0}]",in(reg)&mode,options(nostack,preserves_flags));
        }
        Self(prior)
    }
}
#[cfg(target_arch = "x86_64")]
impl Drop for HostFloatMode {
    fn drop(&mut self) {
        unsafe {
            std::arch::asm!("ldmxcsr [{0}]",in(reg)&self.0,options(nostack,preserves_flags));
        }
    }
}
enum Command {
    LegacyPop(Reply),
    Modifiers([f32; 4], bool, Reply),
    Vert([f32; 16], [f32; 3], [f32; 3], u32, f32, Reply),
    ActorContacts(mpsc::SyncSender<Vec<[f32; 5]>>),
    ActorBail(Reply),
    Stats(mpsc::SyncSender<[u64; 16]>),
    Contacts(mpsc::SyncSender<Vec<[f32; 7]>>),
    Events(usize, mpsc::SyncSender<Vec<skate_host::bridge::NativeEvent>>),
    ScoreReset(Reply),
    DynamicGrinds(u32, Option<Vec<Vec<[f32; 3]>>>, [f32; 16], bool, Reply),
    Grinds(Vec<Vec<[f32; 3]>>, Reply),
    GrindStatus(mpsc::SyncSender<[f32; 8]>),
    Probe([f32; 3], [f32; 3], mpsc::SyncSender<Result<[f32; 8], String>>),
    Dynamic(u32, Option<Vec<[[f32; 3]; 3]>>, [f32; 16], bool, bool, Reply),
    MarkerCapture(mpsc::SyncSender<Result<Marker, String>>),
    MarkerLoad(Marker, Reply),
    WaterReturn(Marker, Reply),
    BodyHeight(f32, Reply),
    Pose(mpsc::SyncSender<Result<Box<SkaterPose>, String>>),
    Step(Vec<Packet>, Reply),
    StepCamera(Vec<Packet>, [f32; 3], Reply),
    World(Vec<[[f32; 3]; 3]>, Vec<Vec<[f32; 3]>>, Reply),
    Handoff([f32; 3], f32, [f32; 3], Reply),
    Activate([f32; 3], f32, Reply),
    AirHandoff([f32; 3], f32, [f32; 3], Reply),
    Stop,
}
pub struct Worker {
    heartbeat: std::sync::Arc<[std::sync::atomic::AtomicU64; 6]>,
    commands: mpsc::Sender<Command>,
    join: Option<thread::JoinHandle<()>>,
    diagnostics: std::sync::Arc<std::sync::Mutex<[f32; 24]>>,
    biped: std::sync::Arc<std::sync::Mutex<[f32; 24]>>,
    rider: std::sync::Arc<std::sync::Mutex<[f32; 256]>>,
}
impl Worker {
    fn retire(&mut self) -> bool {
        let _ = self.commands.send(Command::Stop);
        if let Some(join) = self.join.take() {
            let start = std::time::Instant::now();
            while !join.is_finished() && start.elapsed() < std::time::Duration::from_millis(100) {
                thread::sleep(std::time::Duration::from_millis(1));
            }
            if join.is_finished() {
                let _ = join.join();
                return true;
            }
            return false;
        }
        true
    }
}
impl Drop for Worker {
    fn drop(&mut self) {
        self.retire();
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_destroy_checked(worker: *mut Worker) -> i32 {
    if worker.is_null() {
        return 1;
    }
    let mut w = unsafe { Box::from_raw(worker) };
    i32::from(w.retire())
}

#[unsafe(no_mangle)]
pub extern "C" fn sh_skate_abi() -> u32 {
    4
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_push_diagnostics(worker: *mut Worker, out: *mut f32, count: u32) -> i32 {
    if worker.is_null() || out.is_null() || count != 24 {
        return 0;
    }
    let data = unsafe { &*worker }
        .diagnostics
        .lock()
        .unwrap_or_else(|e| e.into_inner());
    unsafe {
        std::ptr::copy_nonoverlapping(data.as_ptr(), out, 24);
    }
    1
}
#[unsafe(no_mangle)]
pub extern "C" fn sh_skate_error() -> *const c_char {
    ERROR.with(|e| e.borrow().as_ptr())
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_create(
    root: *const c_char,
    points: *const f32,
    triangle_count: u32,
    spawn: *const f32,
    heading: f32,
) -> *mut Worker {
    let run = catch_unwind(AssertUnwindSafe(|| -> Result<*mut Worker, String> {
        if root.is_null() || spawn.is_null() || (triangle_count > 0 && points.is_null()) || triangle_count > 1_000_000 {
            return Err("Null or invalid runtime creation argument".into());
        }
        let path = unsafe { CStr::from_ptr(root) }
            .to_str()
            .map_err(|e| e.to_string())?
            .to_owned();
        let spawn = unsafe { std::slice::from_raw_parts(spawn, 3) };
        let origin = [spawn[0], spawn[1], spawn[2]];
        if origin.iter().any(|v| !v.is_finite()) || !heading.is_finite() {
            return Err("Invalid runtime spawn".into());
        }
        let flat = if triangle_count == 0 {
            &[][..]
        } else {
            unsafe { std::slice::from_raw_parts(points, triangle_count as usize * 9) }
        };
        if flat.iter().any(|v| !v.is_finite()) {
            return Err("Nonfinite collision".into());
        }
        let triangles = flat
            .chunks_exact(9)
            .map(|p| [[p[0], p[1], p[2]], [p[3], p[4], p[5]], [p[6], p[7], p[8]]])
            .collect();
        let (commands, receiver) = mpsc::channel();
        let (ready, loaded) = mpsc::sync_channel(1);
        let diagnostics = std::sync::Arc::new(std::sync::Mutex::new([0.; 24]));
        let worker_diagnostics = diagnostics.clone();
        let biped = std::sync::Arc::new(std::sync::Mutex::new([0.; 24]));
        let worker_biped = biped.clone();
        let rider = std::sync::Arc::new(std::sync::Mutex::new([0.; 256]));
        let worker_rider = rider.clone();
        let heartbeat = std::sync::Arc::new(std::array::from_fn(|_| std::sync::atomic::AtomicU64::new(0)));
        let worker_heartbeat = heartbeat.clone();
        let thread_begin = std::time::Instant::now();
        let join = thread::Builder::new()
            .name("SkateHarkinian physics".into())
            .stack_size(16 * 1024 * 1024)
            .spawn(move || {
                #[cfg(target_arch = "x86_64")]
                let _float_mode = HostFloatMode::enter();

                let construction = catch_unwind(AssertUnwindSafe(|| -> Result<Session, String> {
                    let mut session = Session::new(Path::new(&path), triangles, vec![], origin, heading)?;
                    session.activate(origin, heading)?;
                    Ok(session)
                }));
                let mut session = match construction {
                    Ok(Ok(s)) => {
                        let _ = ready.send(Ok(()));
                        s
                    }
                    Ok(Err(e)) => {
                        let _ = ready.send(Err(e));
                        return;
                    }
                    Err(_) => {
                        let _ = ready.send(Err("Runtime initialization panic contained on worker".into()));
                        return;
                    }
                };
                let mut external: Option<vert::External> = None;
                while let Ok(command) = receiver.recv() {
                    use std::sync::atomic::Ordering::Relaxed;
                    let sequence = worker_heartbeat[0].fetch_add(1, Relaxed) + 1;
                    let kind = match &command {
                        Command::Step(..) | Command::StepCamera(..) => 1,
                        Command::Vert(..) => 2,
                        Command::World(..) | Command::Dynamic(..) => 3,
                        Command::Activate(..) | Command::MarkerLoad(..) | Command::WaterReturn(..) => 4,
                        _ => 5,
                    };
                    worker_heartbeat[2].store(kind, Relaxed);
                    worker_heartbeat[5].store(thread_begin.elapsed().as_millis() as u64, Relaxed);
                    match command {
                        Command::Stop => break,
                        Command::Stats(reply) => {
                            let _ = reply.send(session.host_lifecycle_stats());
                        }
                        Command::Contacts(reply) => {
                            let _ = reply.send(session.host_contacts());
                        }
                        Command::Events(capacity, reply) => {
                            let _ = reply.send(session.host_read_events(capacity));
                        }
                        Command::GrindStatus(reply) => {
                            let _ = reply.send(session.host_grind_status());
                        }
                        Command::Probe(a, b, reply) => {
                            let _ = reply.send(session.host_probe(a, b));
                        }
                        Command::MarkerCapture(reply) => {
                            let result = catch_unwind(AssertUnwindSafe(|| {
                                session
                                    .host_capture_marker()
                                    .map(|(frame, onboard, foot_forward)| Marker {
                                        version: 1,
                                        onboard: u32::from(onboard),
                                        foot_forward: u32::from(foot_forward),
                                        reserved: 0,
                                        frame,
                                    })
                            }))
                            .unwrap_or_else(|_| Err("Marker capture panic".into()));
                            let _ = reply.send(result);
                        }
                        Command::Pose(reply) => {
                            let result = catch_unwind(AssertUnwindSafe(|| {
                                if let Some(e) = &external {
                                    e.pose()
                                } else {
                                    read_pose(&session)
                                }
                            }))
                            .unwrap_or_else(|_| Err("Read-only pose export panic".into()));
                            let _ = reply.send(result);
                        }
                        command => {
                            let (reply, run) = match command {
                                Command::Vert(frame, velocity, anchor, mode, weight, reply) => (
                                    reply,
                                    catch_unwind(AssertUnwindSafe(|| -> Result<Snapshot, String> {
                                        if mode == 0 {
                                            if let Some(mut e) = external.take() {
                                                let current =
                                                    e.step(frame, velocity, anchor, 1, 0., session.pose().tick)?;
                                                session.host_release_vert(current.board, velocity)?;
                                                session.host_score_reset();
                                            }
                                            return Ok(snapshot(&session));
                                        }
                                        if external.is_none() {
                                            external = Some(vert::External::new(&session)?);
                                        }
                                        let tick = session.host_external_contact_tick();
                                        external
                                            .as_mut()
                                            .unwrap()
                                            .step(frame, velocity, anchor, mode, weight, tick)
                                    })),
                                ),
                                Command::LegacyPop(reply) => (
                                    reply,
                                    catch_unwind(AssertUnwindSafe(|| -> Result<Snapshot, String> {
                                        session.host_legacy_pop()?;
                                        *worker_rider.lock().unwrap_or_else(|e| e.into_inner()) =
                                            session.host_rider_diagnostics();
                                        Ok(snapshot(&session))
                                    })),
                                ),
                                Command::Modifiers(values, no_bail, reply) => (
                                    reply,
                                    catch_unwind(AssertUnwindSafe(|| -> Result<Snapshot, String> {
                                        session.host_skate_modifiers(values, no_bail)?;
                                        Ok(snapshot(&session))
                                    })),
                                ),
                                Command::ScoreReset(reply) => (
                                    reply,
                                    catch_unwind(AssertUnwindSafe(|| -> Result<Snapshot, String> {
                                        session.host_score_reset();
                                        Ok(snapshot(&session))
                                    })),
                                ),
                                Command::DynamicGrinds(id, local, frame, active, reply) => (
                                    reply,
                                    catch_unwind(AssertUnwindSafe(|| -> Result<Snapshot, String> {
                                        session.host_dynamic_grinds(id, local, frame, active)?;
                                        Ok(snapshot(&session))
                                    })),
                                ),
                                Command::Grinds(rails, reply) => (
                                    reply,
                                    catch_unwind(AssertUnwindSafe(|| -> Result<Snapshot, String> {
                                        session.host_grinds(rails)?;
                                        Ok(snapshot(&session))
                                    })),
                                ),
                                Command::Dynamic(id, local, frame, active, riding_surface, reply) => (
                                    reply,
                                    catch_unwind(AssertUnwindSafe(|| -> Result<Snapshot, String> {
                                        session.host_dynamic(id, local, frame, active, riding_surface)?;
                                        Ok(snapshot(&session))
                                    })),
                                ),
                                Command::ActorContacts(reply) => {
                                    let _ = reply.send(session.host_actor_contacts());
                                    worker_heartbeat[1].store(sequence, Relaxed);
                                    continue;
                                }
                                Command::ActorBail(reply) => (
                                    reply,
                                    catch_unwind(AssertUnwindSafe(|| -> Result<Snapshot, String> {
                                        session.host_actor_bail()?;
                                        *worker_rider.lock().unwrap_or_else(|e| e.into_inner()) =
                                            session.host_rider_diagnostics();
                                        Ok(snapshot(&session))
                                    })),
                                ),
                                Command::BodyHeight(height, reply) => (
                                    reply,
                                    catch_unwind(AssertUnwindSafe(|| -> Result<Snapshot, String> {
                                        session.host_link_clearance_height(height)?;
                                        Ok(snapshot(&session))
                                    })),
                                ),
                                Command::Step(inputs, reply) => (
                                    reply,
                                    catch_unwind(AssertUnwindSafe(|| -> Result<Snapshot, String> {
                                        let samples: Vec<Controls> = inputs
                                            .into_iter()
                                            .map(|input| Controls {
                                                buttons: input.buttons,
                                                triggers: input.triggers,
                                                left: input.left,
                                                right: input.right,
                                            })
                                            .collect();
                                        let result = session.host_tick_samples(&samples);
                                        *worker_diagnostics.lock().unwrap_or_else(|e| e.into_inner()) =
                                            session.host_push_diagnostics();
                                        result?;
                                        *worker_rider.lock().unwrap_or_else(|e| e.into_inner()) =
                                            session.host_rider_diagnostics();
                                        Ok(snapshot(&session))
                                    })),
                                ),
                                Command::StepCamera(inputs, forward, reply) => (
                                    reply,
                                    catch_unwind(AssertUnwindSafe(|| -> Result<Snapshot, String> {
                                        session.host_camera_forward(forward)?;
                                        let samples: Vec<Controls> = inputs
                                            .into_iter()
                                            .map(|input| Controls {
                                                buttons: input.buttons,
                                                triggers: input.triggers,
                                                left: input.left,
                                                right: input.right,
                                            })
                                            .collect();
                                        let result = session.host_tick_samples(&samples);
                                        *worker_diagnostics.lock().unwrap_or_else(|e| e.into_inner()) =
                                            session.host_push_diagnostics();
                                        let mut b = session.host_biped_diagnostics();
                                        if let Some(input) = samples.last() {
                                            b[14] = input.left[0] as f32 / 32767.;
                                            b[15] = input.left[1] as f32 / 32767.;
                                        }
                                        *worker_biped.lock().unwrap_or_else(|e| e.into_inner()) = b;
                                        result?;
                                        *worker_rider.lock().unwrap_or_else(|e| e.into_inner()) =
                                            session.host_rider_diagnostics();
                                        Ok(snapshot(&session))
                                    })),
                                ),
                                Command::World(triangles, rails, reply) => (
                                    reply,
                                    catch_unwind(AssertUnwindSafe(|| -> Result<Snapshot, String> {
                                        external = None;
                                        let collision = session.prepare_cached(triangles, rails)?;
                                        session.install_collision(collision)?;
                                        Ok(snapshot(&session))
                                    })),
                                ),
                                Command::Handoff(spawn, heading, velocity, reply) => (
                                    reply,
                                    catch_unwind(AssertUnwindSafe(|| -> Result<Snapshot, String> {
                                        external = None;
                                        session.host_activate_motion(spawn, heading, velocity)?;
                                        Ok(snapshot(&session))
                                    })),
                                ),
                                Command::AirHandoff(spawn, heading, velocity, reply) => (
                                    reply,
                                    catch_unwind(AssertUnwindSafe(|| -> Result<Snapshot, String> {
                                        external = None;
                                        session.host_activate_air_motion(spawn, heading, velocity)?;
                                        Ok(snapshot(&session))
                                    })),
                                ),
                                Command::MarkerLoad(marker, reply) => (
                                    reply,
                                    catch_unwind(AssertUnwindSafe(|| -> Result<Snapshot, String> {
                                        external = None;
                                        session.host_load_marker(
                                            marker.frame,
                                            marker.onboard != 0,
                                            marker.foot_forward != 0,
                                        )?;
                                        *worker_rider.lock().unwrap_or_else(|e| e.into_inner()) =
                                            session.host_rider_diagnostics();
                                        *worker_biped.lock().unwrap_or_else(|e| e.into_inner()) =
                                            session.host_biped_diagnostics();
                                        *worker_diagnostics.lock().unwrap_or_else(|e| e.into_inner()) =
                                            session.host_push_diagnostics();
                                        Ok(snapshot(&session))
                                    })),
                                ),
                                Command::WaterReturn(marker, reply) => (
                                    reply,
                                    catch_unwind(AssertUnwindSafe(|| -> Result<Snapshot, String> {
                                        external = None;
                                        session.host_water_return(
                                            marker.frame,
                                            marker.onboard != 0,
                                            marker.foot_forward != 0,
                                        )?;
                                        *worker_rider.lock().unwrap_or_else(|e| e.into_inner()) =
                                            session.host_rider_diagnostics();
                                        *worker_biped.lock().unwrap_or_else(|e| e.into_inner()) =
                                            session.host_biped_diagnostics();
                                        *worker_diagnostics.lock().unwrap_or_else(|e| e.into_inner()) =
                                            session.host_push_diagnostics();
                                        Ok(snapshot(&session))
                                    })),
                                ),
                                Command::Activate(spawn, heading, reply) => (
                                    reply,
                                    catch_unwind(AssertUnwindSafe(|| -> Result<Snapshot, String> {
                                        external = None;
                                        session.activate(spawn, heading)?;
                                        Ok(snapshot(&session))
                                    })),
                                ),
                                Command::Stats(_)
                                | Command::Contacts(_)
                                | Command::Stop
                                | Command::Pose(_)
                                | Command::MarkerCapture(_)
                                | Command::Probe(..)
                                | Command::GrindStatus(_)
                                | Command::Events(..) => unreachable!(),
                            };
                            let fatal = run.is_err() || matches!(&run, Ok(Err(_)));
                            let result =
                                run.unwrap_or_else(|_| Err("Runtime command panic contained on worker".into()));
                            let _ = reply.send(result);
                            if fatal {
                                break;
                            }
                        }
                    }
                    worker_heartbeat[3].store(session.pose().tick, Relaxed);
                    worker_heartbeat[1].store(sequence, Relaxed);
                    worker_heartbeat[5].store(thread_begin.elapsed().as_millis() as u64, Relaxed);
                }
            })
            .map_err(|e| e.to_string())?;
        match loaded
            .recv_timeout(std::time::Duration::from_secs(120))
            .map_err(|_| "Runtime loader timed out or disconnected; initialization worker quarantined".to_string())?
        {
            Ok(()) => Ok(Box::into_raw(Box::new(Worker {
                heartbeat,
                commands,
                join: Some(join),
                diagnostics,
                biped,
                rider,
            }))),
            Err(e) => {
                let _ = join.join();
                Err(e)
            }
        }
    }));
    match run {
        Ok(Ok(p)) => p,
        Ok(Err(e)) => {
            error(e);
            std::ptr::null_mut()
        }
        Err(_) => {
            error("Runtime ABI initialization panic contained".into());
            std::ptr::null_mut()
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_step(worker: *mut Worker, input: Packet, out: *mut Snapshot) -> i32 {
    if worker.is_null() || out.is_null() {
        error("Null runtime step argument".into());
        return 0;
    }
    let run = catch_unwind(AssertUnwindSafe(|| -> Result<Snapshot, String> {
        let worker = unsafe { &*worker };
        let (tx, rx) = mpsc::sync_channel(1);
        worker
            .commands
            .send(Command::Step(vec![input], tx))
            .map_err(|_| "Runtime worker stopped".to_string())?;
        rx.recv_timeout(std::time::Duration::from_secs(2))
            .map_err(|_| "Runtime worker disconnected".to_string())?
    }));
    match run {
        Ok(Ok(s)) => {
            unsafe { *out = s };
            1
        }
        Ok(Err(e)) => {
            error(e);
            0
        }
        Err(_) => {
            error("Runtime ABI tick panic contained".into());
            0
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_destroy(worker: *mut Worker) {
    if !worker.is_null() {
        drop(unsafe { Box::from_raw(worker) });
    }
}

// All buffers are copied before crossing the worker boundary. The caller never
// lends actor/collision memory to the runtime, including during scene changes.
fn request(worker: &Worker, command: impl FnOnce(Reply) -> Command) -> Result<Snapshot, String> {
    let (tx, rx) = mpsc::sync_channel(1);
    let cmd = command(tx);
    let timeout = if matches!(&cmd, Command::World(..)) { 120 } else { 2 };
    worker
        .commands
        .send(cmd)
        .map_err(|_| "Runtime worker stopped".to_string())?;
    rx.recv_timeout(std::time::Duration::from_secs(timeout))
        .map_err(|_| "Runtime worker disconnected or request timed out".to_string())?
}
fn finish(run: Result<Result<Snapshot, String>, Box<dyn std::any::Any + Send>>, out: *mut Snapshot) -> i32 {
    match run {
        Ok(Ok(s)) => {
            unsafe { *out = s };
            1
        }
        Ok(Err(e)) => {
            error(e);
            0
        }
        Err(_) => {
            error("Runtime ABI command panic contained".into());
            0
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_step_samples(
    worker: *mut Worker,
    inputs: *const Packet,
    count: u32,
    out: *mut Snapshot,
) -> i32 {
    if worker.is_null() || out.is_null() || inputs.is_null() || count == 0 || count > 512 {
        error("Invalid runtime sample batch".into());
        return 0;
    }
    finish(
        catch_unwind(AssertUnwindSafe(|| {
            let samples = unsafe { std::slice::from_raw_parts(inputs, count as usize) }.to_vec();
            request(unsafe { &*worker }, |reply| Command::Step(samples, reply))
        })),
        out,
    )
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_world(
    worker: *mut Worker,
    points: *const f32,
    triangle_count: u32,
    rail_points: *const f32,
    rail_offsets: *const u32,
    rail_count: u32,
    rail_point_count: u32,
    out: *mut Snapshot,
) -> i32 {
    if worker.is_null()
        || out.is_null()
        || triangle_count > 1_000_000
        || rail_count > 100_000
        || rail_point_count > 1_000_000
        || (triangle_count > 0 && points.is_null())
        || (rail_count > 0 && (rail_points.is_null() || rail_offsets.is_null()))
    {
        error("Invalid runtime world buffer".into());
        return 0;
    }
    finish(
        catch_unwind(AssertUnwindSafe(|| -> Result<Snapshot, String> {
            let flat = if triangle_count == 0 {
                &[][..]
            } else {
                unsafe { std::slice::from_raw_parts(points, triangle_count as usize * 9) }
            };
            if flat.iter().any(|v| !v.is_finite()) {
                return Err("Nonfinite collision".into());
            }
            let triangles = flat
                .chunks_exact(9)
                .map(|p| [[p[0], p[1], p[2]], [p[3], p[4], p[5]], [p[6], p[7], p[8]]])
                .collect();
            let mut rails = Vec::new();
            if rail_count > 0 {
                let offsets = unsafe { std::slice::from_raw_parts(rail_offsets, rail_count as usize + 1) };
                if offsets[0] != 0
                    || offsets[rail_count as usize] != rail_point_count
                    || offsets.iter().any(|v| *v > rail_point_count)
                    || offsets.windows(2).any(|v| v[1].saturating_sub(v[0]) < 2)
                {
                    return Err("Invalid rail offsets".into());
                }
                let points = unsafe { std::slice::from_raw_parts(rail_points, rail_point_count as usize * 3) };
                if points.iter().any(|v| !v.is_finite()) {
                    return Err("Nonfinite rail point".into());
                }
                for range in offsets.windows(2) {
                    rails.push(
                        points[range[0] as usize * 3..range[1] as usize * 3]
                            .chunks_exact(3)
                            .map(|p| [p[0], p[1], p[2]])
                            .collect(),
                    );
                }
            }
            request(unsafe { &*worker }, |reply| Command::World(triangles, rails, reply))
        })),
        out,
    )
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_handoff(
    worker: *mut Worker,
    spawn: *const f32,
    heading: f32,
    velocity: *const f32,
    out: *mut Snapshot,
) -> i32 {
    if worker.is_null() || out.is_null() || spawn.is_null() || velocity.is_null() {
        error("Invalid runtime handoff buffer".into());
        return 0;
    }
    finish(
        catch_unwind(AssertUnwindSafe(|| -> Result<Snapshot, String> {
            let p = unsafe { std::slice::from_raw_parts(spawn, 3) };
            let v = unsafe { std::slice::from_raw_parts(velocity, 3) };
            if p.iter().chain(v.iter()).any(|v| !v.is_finite()) || !heading.is_finite() {
                return Err("Nonfinite runtime handoff".into());
            }
            request(unsafe { &*worker }, |reply| {
                Command::Handoff([p[0], p[1], p[2]], heading, [v[0], v[1], v[2]], reply)
            })
        })),
        out,
    )
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_activate(
    worker: *mut Worker,
    spawn: *const f32,
    heading: f32,
    out: *mut Snapshot,
) -> i32 {
    if worker.is_null() || out.is_null() || spawn.is_null() {
        error("Invalid runtime activation buffer".into());
        return 0;
    }
    finish(
        catch_unwind(AssertUnwindSafe(|| -> Result<Snapshot, String> {
            let p = unsafe { std::slice::from_raw_parts(spawn, 3) };
            if p.iter().any(|v| !v.is_finite()) || !heading.is_finite() {
                return Err("Nonfinite runtime activation".into());
            }
            request(unsafe { &*worker }, |reply| {
                Command::Activate([p[0], p[1], p[2]], heading, reply)
            })
        })),
        out,
    )
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_air_handoff(
    worker: *mut Worker,
    spawn: *const f32,
    heading: f32,
    velocity: *const f32,
    out: *mut Snapshot,
) -> i32 {
    if worker.is_null() || out.is_null() || spawn.is_null() || velocity.is_null() {
        error("Invalid runtime handoff buffer".into());
        return 0;
    }
    finish(
        catch_unwind(AssertUnwindSafe(|| -> Result<Snapshot, String> {
            let p = unsafe { std::slice::from_raw_parts(spawn, 3) };
            let v = unsafe { std::slice::from_raw_parts(velocity, 3) };
            if p.iter().chain(v.iter()).any(|v| !v.is_finite()) || !heading.is_finite() {
                return Err("Nonfinite runtime handoff".into());
            }
            request(unsafe { &*worker }, |reply| {
                Command::AirHandoff([p[0], p[1], p[2]], heading, [v[0], v[1], v[2]], reply)
            })
        })),
        out,
    )
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_biped_diagnostics(worker: *mut Worker, out: *mut f32, count: u32) -> i32 {
    if worker.is_null() || out.is_null() || count != 24 {
        return 0;
    }
    let data = unsafe { &*worker }.biped.lock().unwrap_or_else(|e| e.into_inner());
    unsafe {
        std::ptr::copy_nonoverlapping(data.as_ptr(), out, 24);
    }
    1
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_step_samples_camera(
    worker: *mut Worker,
    inputs: *const Packet,
    count: u32,
    forward: *const f32,
    out: *mut Snapshot,
) -> i32 {
    if worker.is_null() || inputs.is_null() || forward.is_null() || out.is_null() || count == 0 || count > 512 {
        error("Invalid camera sample batch".into());
        return 0;
    }
    let samples = unsafe { std::slice::from_raw_parts(inputs, count as usize) }.to_vec();
    let f = unsafe { std::slice::from_raw_parts(forward, 3) };
    match request(unsafe { &*worker }, |reply| {
        Command::StepCamera(samples, [f[0], f[1], f[2]], reply)
    }) {
        Ok(s) => {
            unsafe {
                *out = s;
            }
            1
        }
        Err(e) => {
            error(e);
            0
        }
    }
}

mod pose;
use pose::{SkaterPose, read_pose};

#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_rider_diagnostics(worker: *mut Worker, out: *mut f32, count: u32) -> i32 {
    if worker.is_null() || out.is_null() || count != 256 {
        return 0;
    }
    let d = unsafe { &*worker }.rider.lock().unwrap_or_else(|e| e.into_inner());
    unsafe {
        std::ptr::copy_nonoverlapping(d.as_ptr(), out, 256);
    }
    1
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_rider_height(worker: *mut Worker, height: f32) -> i32 {
    if worker.is_null() {
        return 0;
    }
    let result = catch_unwind(AssertUnwindSafe(|| {
        request(unsafe { &*worker }, |reply| Command::BodyHeight(height, reply))
    }));
    match result {
        Ok(Ok(_)) => 1,
        Ok(Err(e)) => {
            error(e);
            0
        }
        Err(_) => {
            error("Body profile panic".into());
            0
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_marker_capture(worker: *mut Worker, out: *mut Marker) -> i32 {
    if worker.is_null() || out.is_null() {
        error("Invalid marker capture buffer".into());
        return 0;
    }
    let run = catch_unwind(AssertUnwindSafe(|| -> Result<Marker, String> {
        let (tx, rx) = mpsc::sync_channel(1);
        unsafe { &*worker }
            .commands
            .send(Command::MarkerCapture(tx))
            .map_err(|_| "Marker worker stopped".to_string())?;
        rx.recv_timeout(std::time::Duration::from_secs(2))
            .map_err(|_| "Marker worker disconnected".to_string())?
    }));
    match run {
        Ok(Ok(marker)) => {
            unsafe {
                *out = marker;
            }
            1
        }
        Ok(Err(e)) => {
            error(e);
            0
        }
        Err(_) => {
            error("Marker capture panic contained".into());
            0
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_marker_load(worker: *mut Worker, marker: *const Marker, out: *mut Snapshot) -> i32 {
    if worker.is_null() || marker.is_null() || out.is_null() {
        error("Invalid marker load buffer".into());
        return 0;
    }
    let m = unsafe { *marker };
    if m.version != 1 || m.onboard > 1 || m.foot_forward > 1 || m.frame.iter().any(|x| !x.is_finite()) {
        error("Invalid marker ABI".into());
        return 0;
    }
    finish(
        catch_unwind(AssertUnwindSafe(|| {
            request(unsafe { &*worker }, |reply| Command::MarkerLoad(m, reply))
        })),
        out,
    )
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_water_return(worker: *mut Worker, marker: *const Marker, out: *mut Snapshot) -> i32 {
    if worker.is_null() || marker.is_null() || out.is_null() {
        error("Invalid marker load buffer".into());
        return 0;
    }
    let m = unsafe { *marker };
    if m.version != 1 || m.onboard > 1 || m.foot_forward > 1 || m.frame.iter().any(|x| !x.is_finite()) {
        error("Invalid marker ABI".into());
        return 0;
    }
    finish(
        catch_unwind(AssertUnwindSafe(|| {
            request(unsafe { &*worker }, |reply| Command::WaterReturn(m, reply))
        })),
        out,
    )
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_dynamic(
    worker: *mut Worker,
    id: u32,
    points: *const f32,
    count: u32,
    frame: *const f32,
    active: u32,
) -> i32 {
    if worker.is_null() || frame.is_null() || count > 100000 || id == 0 || id > 32767 {
        return 0;
    }
    let f = unsafe { std::slice::from_raw_parts(frame, 16) };
    if f.iter().any(|v| !v.is_finite()) {
        return 0;
    }
    let mut matrix = [0.; 16];
    matrix.copy_from_slice(f);
    let local = if points.is_null() {
        if count != 0 {
            return 0;
        }
        None
    } else {
        let p = unsafe { std::slice::from_raw_parts(points, count as usize * 9) };
        if p.iter().any(|v| !v.is_finite()) {
            return 0;
        }
        Some(
            p.chunks_exact(9)
                .map(|p| [[p[0], p[1], p[2]], [p[3], p[4], p[5]], [p[6], p[7], p[8]]])
                .collect(),
        )
    };
    match catch_unwind(AssertUnwindSafe(|| {
        request(unsafe { &*worker }, |reply| {
            Command::Dynamic(id, local, matrix, active & 1 != 0, active & 2 != 0, reply)
        })
    })) {
        Ok(Ok(_)) => 1,
        Ok(Err(e)) => {
            error(e);
            0
        }
        Err(_) => {
            error("Dynamic collision panic".into());
            0
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_contacts(worker: *mut Worker, out: *mut f32, capacity: u32) -> i32 {
    if worker.is_null() || out.is_null() || capacity > 4096 {
        return -1;
    }
    let (tx, rx) = mpsc::sync_channel(1);
    if unsafe { &*worker }.commands.send(Command::Contacts(tx)).is_err() {
        return -1;
    }
    match rx.recv_timeout(std::time::Duration::from_secs(2)) {
        Ok(v) => {
            let n = v.len().min(capacity as usize);
            unsafe {
                std::ptr::copy_nonoverlapping(v.as_ptr() as *const f32, out, n * 7);
            }
            n as i32
        }
        _ => -1,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_world_probe(
    worker: *mut Worker,
    start: *const f32,
    end: *const f32,
    out: *mut f32,
) -> i32 {
    if worker.is_null() || start.is_null() || end.is_null() || out.is_null() {
        return 0;
    }
    let a = unsafe { std::slice::from_raw_parts(start, 3) };
    let b = unsafe { std::slice::from_raw_parts(end, 3) };
    if a.iter().chain(b).any(|v| !v.is_finite()) {
        return 0;
    }
    let (tx, rx) = mpsc::sync_channel(1);
    if unsafe { &*worker }
        .commands
        .send(Command::Probe([a[0], a[1], a[2]], [b[0], b[1], b[2]], tx))
        .is_err()
    {
        return 0;
    }
    match rx.recv_timeout(std::time::Duration::from_secs(2)) {
        Ok(Ok(v)) => {
            unsafe {
                std::ptr::copy_nonoverlapping(v.as_ptr(), out, 8);
            }
            1
        }
        _ => 0,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_grinds(
    worker: *mut Worker,
    points: *const f32,
    offsets: *const u32,
    count: u32,
    point_count: u32,
) -> i32 {
    if worker.is_null()
        || offsets.is_null()
        || count > 10000
        || point_count > 100000
        || (point_count > 0 && points.is_null())
    {
        return 0;
    }
    let ranges = unsafe { std::slice::from_raw_parts(offsets, count as usize + 1) };
    if ranges[0] != 0 || ranges[count as usize] != point_count || ranges.windows(2).any(|p| p[1] < p[0] + 2) {
        return 0;
    }
    let p = if point_count == 0 {
        &[][..]
    } else {
        unsafe { std::slice::from_raw_parts(points, point_count as usize * 3) }
    };
    if p.iter().any(|v| !v.is_finite() || v.abs() > 100000.) {
        return 0;
    }
    let rails = ranges
        .windows(2)
        .map(|r| {
            p[r[0] as usize * 3..r[1] as usize * 3]
                .chunks_exact(3)
                .map(|p| [p[0], p[1], p[2]])
                .collect()
        })
        .collect();
    match catch_unwind(AssertUnwindSafe(|| {
        request(unsafe { &*worker }, |reply| Command::Grinds(rails, reply))
    })) {
        Ok(Ok(_)) => 1,
        Ok(Err(e)) => {
            error(e);
            0
        }
        Err(_) => {
            error("Native grind upload panic".into());
            0
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_grind_status(worker: *mut Worker, out: *mut f32) -> i32 {
    if worker.is_null() || out.is_null() {
        return 0;
    }
    let (tx, rx) = mpsc::sync_channel(1);
    if unsafe { &*worker }.commands.send(Command::GrindStatus(tx)).is_err() {
        return 0;
    }
    match rx.recv_timeout(std::time::Duration::from_secs(2)) {
        Ok(v) => {
            unsafe {
                std::ptr::copy_nonoverlapping(v.as_ptr(), out, 8);
            }
            1
        }
        _ => 0,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_dynamic_grinds(
    worker: *mut Worker,
    id: u32,
    points: *const f32,
    offsets: *const u32,
    count: u32,
    point_count: u32,
    frame: *const f32,
    active: u32,
) -> i32 {
    if worker.is_null() || id == 0 || id > 32767 || frame.is_null() || count > 10000 || point_count > 100000 {
        return 0;
    }
    let f = unsafe { std::slice::from_raw_parts(frame, 16) };
    if f.iter().any(|v| !v.is_finite()) {
        return 0;
    }
    let mut matrix = [0.; 16];
    matrix.copy_from_slice(f);
    let local = if offsets.is_null() {
        if count != 0 || point_count != 0 {
            return 0;
        }
        None
    } else {
        if point_count > 0 && points.is_null() {
            return 0;
        }
        let r = unsafe { std::slice::from_raw_parts(offsets, count as usize + 1) };
        if r[0] != 0 || r[count as usize] != point_count || r.windows(2).any(|p| p[1] < p[0] + 2) {
            return 0;
        }
        let p = if point_count == 0 {
            &[][..]
        } else {
            unsafe { std::slice::from_raw_parts(points, point_count as usize * 3) }
        };
        if p.iter().any(|v| !v.is_finite()) {
            return 0;
        }
        Some(
            r.windows(2)
                .map(|r| {
                    p[r[0] as usize * 3..r[1] as usize * 3]
                        .chunks_exact(3)
                        .map(|p| [p[0], p[1], p[2]])
                        .collect()
                })
                .collect(),
        )
    };
    match catch_unwind(AssertUnwindSafe(|| {
        request(unsafe { &*worker }, |reply| {
            Command::DynamicGrinds(id, local, matrix, active != 0, reply)
        })
    })) {
        Ok(Ok(_)) => 1,
        Ok(Err(e)) => {
            error(e);
            0
        }
        Err(_) => {
            error("Dynamic grind transform panic".into());
            0
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_events(
    worker: *mut Worker,
    out: *mut skate_host::bridge::NativeEvent,
    capacity: u32,
    bytes: u32,
) -> i32 {
    if worker.is_null()
        || out.is_null()
        || capacity == 0
        || capacity > 4096
        || bytes as usize != std::mem::size_of::<skate_host::bridge::NativeEvent>()
    {
        return -1;
    }
    let (tx, rx) = mpsc::sync_channel(1);
    if unsafe { &*worker }
        .commands
        .send(Command::Events(capacity as usize, tx))
        .is_err()
    {
        return -1;
    }
    match rx.recv_timeout(std::time::Duration::from_secs(2)) {
        Ok(v) => {
            unsafe {
                std::ptr::copy_nonoverlapping(v.as_ptr(), out, v.len());
            }
            v.len() as i32
        }
        _ => -1,
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_score_reset(worker: *mut Worker) -> i32 {
    if worker.is_null() {
        return 0;
    }
    match request(unsafe { &*worker }, Command::ScoreReset) {
        Ok(_) => 1,
        Err(e) => {
            error(e);
            0
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_lifecycle_stats(worker: *mut Worker, out: *mut u64, count: u32) -> i32 {
    if worker.is_null() || out.is_null() || (count != 12 && count != 16) {
        return 0;
    }
    let (tx, rx) = mpsc::sync_channel(1);
    if unsafe { &*worker }.commands.send(Command::Stats(tx)).is_err() {
        return 0;
    }
    match rx.recv_timeout(std::time::Duration::from_secs(2)) {
        Ok(v) => {
            unsafe {
                std::ptr::copy_nonoverlapping(v.as_ptr(), out, count as usize);
            }
            1
        }
        _ => 0,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_actor_bail(worker: *mut Worker, out: *mut Snapshot) -> i32 {
    if worker.is_null() || out.is_null() {
        return 0;
    }
    let (tx, rx) = mpsc::sync_channel(1);
    if unsafe { &*worker }.commands.send(Command::ActorBail(tx)).is_err() {
        return 0;
    }
    match rx.recv_timeout(std::time::Duration::from_secs(2)) {
        Ok(Ok(value)) => {
            unsafe { *out = value };
            1
        }
        Ok(Err(e)) => {
            error(e);
            0
        }
        Err(e) => {
            error(e.to_string());
            0
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_actor_contacts(worker: *mut Worker, out: *mut f32, capacity: u32) -> i32 {
    if worker.is_null() || out.is_null() || capacity > 256 {
        return -1;
    }
    let (tx, rx) = mpsc::sync_channel(1);
    if unsafe { &*worker }.commands.send(Command::ActorContacts(tx)).is_err() {
        return -1;
    }
    match rx.recv_timeout(std::time::Duration::from_secs(2)) {
        Ok(v) => {
            let n = v.len().min(capacity as usize);
            unsafe { std::ptr::copy_nonoverlapping(v.as_ptr() as *const f32, out, n * 5) };
            n as i32
        }
        Err(_) => -1,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_vert_step(
    worker: *mut Worker,
    frame: *const f32,
    velocity: *const f32,
    anchor: *const f32,
    mode: u32,
    weight: f32,
    out: *mut Snapshot,
) -> i32 {
    if worker.is_null()
        || frame.is_null()
        || velocity.is_null()
        || anchor.is_null()
        || out.is_null()
        || mode > 2
        || !weight.is_finite()
        || !(0.0..=1.0).contains(&weight)
    {
        return 0;
    }
    let f: [f32; 16] = unsafe { std::slice::from_raw_parts(frame, 16) }.try_into().unwrap();
    let v: [f32; 3] = unsafe { std::slice::from_raw_parts(velocity, 3) }.try_into().unwrap();
    let a: [f32; 3] = unsafe { std::slice::from_raw_parts(anchor, 3) }.try_into().unwrap();
    if f.iter().chain(v.iter()).chain(a.iter()).any(|v| !v.is_finite()) {
        return 0;
    }
    finish(
        catch_unwind(AssertUnwindSafe(|| {
            request(unsafe { &*worker }, |reply| Command::Vert(f, v, a, mode, weight, reply))
        })),
        out,
    )
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_modifiers(worker: *mut Worker, values: *const f32, no_bail: u32) -> i32 {
    if worker.is_null() || values.is_null() || no_bail > 1 {
        return 0;
    }
    let mut v = [0.; 4];
    v.copy_from_slice(unsafe { std::slice::from_raw_parts(values, 4) });
    if v.iter()
        .enumerate()
        .any(|(i, x)| !x.is_finite() || *x < if i == 3 { 0. } else { 0.5 } || *x > if i == 3 { 2. } else { 3. })
    {
        error("Invalid skate modifiers".into());
        return 0;
    }
    match request(unsafe { &*worker }, |reply| Command::Modifiers(v, no_bail != 0, reply)) {
        Ok(_) => 1,
        Err(e) => {
            error(e);
            0
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_legacy_pop(worker: *mut Worker, out: *mut Snapshot) -> i32 {
    if worker.is_null() || out.is_null() {
        return 0;
    }
    match request(unsafe { &*worker }, Command::LegacyPop) {
        Ok(s) => {
            unsafe { *out = s };
            1
        }
        Err(e) => {
            error(e);
            0
        }
    }
}

// Nonblocking worker evidence: submitted/completed commands, final command
// kind, native tick, timeout/reserved, milliseconds since worker construction.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_worker_progress(worker: *mut Worker, out: *mut u64) -> i32 {
    if worker.is_null() || out.is_null() {
        return 0;
    }
    let w = unsafe { &*worker };
    for i in 0..6 {
        unsafe {
            *out.add(i) = w.heartbeat[i].load(std::sync::atomic::Ordering::Relaxed);
        }
    }
    1
}

#[cfg(test)]
mod worker_safety_tests {
    use super::*;
    use std::sync::{Arc, Mutex, atomic::AtomicU64};
    #[test]
    fn unanswered_request_and_destroy_are_bounded() {
        let (commands, rx) = mpsc::channel();
        let join = thread::spawn(move || {
            let _cmd = rx.recv().unwrap();
            thread::sleep(std::time::Duration::from_millis(2600));
        });
        let mut w = Worker {
            heartbeat: Arc::new(std::array::from_fn(|_| AtomicU64::new(0))),
            commands,
            join: Some(join),
            diagnostics: Arc::new(Mutex::new([0.; 24])),
            biped: Arc::new(Mutex::new([0.; 24])),
            rider: Arc::new(Mutex::new([0.; 256])),
        };
        let start = std::time::Instant::now();
        assert!(request(&w, Command::ScoreReset).is_err());
        assert!(start.elapsed() < std::time::Duration::from_millis(2300));
        let start = std::time::Instant::now();
        assert!(!w.retire());
        assert!(start.elapsed() < std::time::Duration::from_millis(200));
    }
}
