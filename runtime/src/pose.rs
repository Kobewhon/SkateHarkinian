use super::*;
pub const MAX_JOINTS: usize = 256;
#[repr(C)]
#[derive(Clone, Copy)]
pub struct PoseJoint {
    pub id: u32,
    pub parent: i32,
    pub name: [u8; 64],
    pub local: [f32; 16],
    pub rest_local: [f32; 16],
    pub global: [f32; 16],
    pub rest_global: [f32; 16],
    pub world: [f32; 16],
    pub valid: u32,
}
impl Default for PoseJoint {
    fn default() -> Self {
        Self {
            id: 0,
            parent: -1,
            name: [0; 64],
            local: [0.; 16],
            rest_local: [0.; 16],
            global: [0.; 16],
            rest_global: [0.; 16],
            world: [0.; 16],
            valid: 0,
        }
    }
}
#[repr(C)]
pub struct SkaterPose {
    pub version: u32,
    pub count: u32,
    pub tick: u64,
    pub root: [f32; 16],
    pub joints: [PoseJoint; MAX_JOINTS],
}
pub(super) fn read_pose(session: &Session) -> Result<Box<SkaterPose>, String> {
    let source = session.pose();
    let n = source.bones.len();
    if n == 0 || n > MAX_JOINTS || source.parents.len() != n || source.names.len() != n || source.rest_bones.len() != n
    {
        return Err(format!("Invalid final pose hierarchy count={n}"));
    }
    let mut out = Box::new(SkaterPose {
        version: 1,
        count: n as u32,
        tick: source.tick,
        root: source.root.to_cols_array(),
        joints: [PoseJoint::default(); MAX_JOINTS],
    });
    for i in 0..n {
        let parent = source.parents[i];
        if parent >= n as i32 || parent == i as i32 {
            return Err(format!("Invalid pose parent bone={i} parent={parent}"));
        }
        let global = source.bones[i];
        let rest = source.rest_bones[i];
        let (local, rest_local) = if parent >= 0 {
            (
                source.bones[parent as usize].inverse() * global,
                source.rest_bones[parent as usize].inverse() * rest,
            )
        } else {
            (global, rest)
        };
        let joint = &mut out.joints[i];
        joint.id = i as u32;
        joint.parent = parent;
        let name = source.names[i].as_bytes();
        let len = name.len().min(63);
        joint.name[..len].copy_from_slice(&name[..len]);
        joint.local = local.to_cols_array();
        joint.rest_local = rest_local.to_cols_array();
        joint.global = global.to_cols_array();
        joint.rest_global = rest.to_cols_array();
        joint.world = (source.root * global).to_cols_array();
        joint.valid = u32::from(
            joint
                .local
                .iter()
                .chain(joint.rest_local.iter())
                .chain(joint.global.iter())
                .chain(joint.rest_global.iter())
                .chain(joint.world.iter())
                .all(|v| v.is_finite())
                && local.determinant().abs() > 1e-6
                && rest_local.determinant().abs() > 1e-6,
        );
    }
    Ok(out)
}
// No advance, animation evaluation, collision update or mutation occurs here.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn sh_skate_read_pose(worker: *mut Worker, out: *mut SkaterPose, bytes: u32) -> i32 {
    if worker.is_null() || out.is_null() || bytes as usize != std::mem::size_of::<SkaterPose>() {
        error("Invalid read-only pose ABI buffer".into());
        return 0;
    }
    let run = catch_unwind(AssertUnwindSafe(|| -> Result<Box<SkaterPose>, String> {
        let (tx, rx) = mpsc::sync_channel(1);
        unsafe { &*worker }
            .commands
            .send(Command::Pose(tx))
            .map_err(|_| "Pose worker stopped".to_string())?;
        rx.recv().map_err(|_| "Pose worker disconnected".to_string())?
    }));
    match run {
        Ok(Ok(p)) => {
            unsafe {
                std::ptr::copy_nonoverlapping(&*p, out, 1);
            }
            1
        }
        Ok(Err(e)) => {
            error(e);
            0
        }
        Err(_) => {
            error("Read-only pose ABI panic".into());
            0
        }
    }
}
