// Project-original constrained transition presentation. No game animation tracks.
use super::*;
pub struct External {
    source: Snapshot,
    source_pose: Box<SkaterPose>,
    pub current: Snapshot,
    delta: [f32; 16],
    hand_ticks: u32,
}
fn mul(a: [f32; 16], b: [f32; 16]) -> [f32; 16] {
    std::array::from_fn(|i| {
        let row = i % 4;
        let col = i / 4;
        (0..4).map(|k| a[k * 4 + row] * b[col * 4 + k]).sum()
    })
}
fn inverse(a: [f32; 16]) -> [f32; 16] {
    let mut m = [0.; 16];
    for r in 0..3 {
        for c in 0..3 {
            m[c * 4 + r] = a[r * 4 + c];
        }
        m[12 + r] = -(0..3).map(|k| m[k * 4 + r] * a[12 + k]).sum::<f32>();
    }
    m[15] = 1.;
    m
}
fn point(m: [f32; 16], p: [f32; 3]) -> [f32; 3] {
    std::array::from_fn(|r| m[12 + r] + (0..3).map(|k| m[k * 4 + r] * p[k]).sum::<f32>())
}
impl External {
    pub fn new(s: &Session) -> Result<Self, String> {
        let source = snapshot(s);
        let source_pose = read_pose(s)?;
        let mut delta = [0.; 16];
        delta[0] = 1.;
        delta[5] = 1.;
        delta[10] = 1.;
        delta[15] = 1.;
        Ok(Self {
            source,
            source_pose,
            current: source,
            delta,
            hand_ticks: 0,
        })
    }
    pub fn step(
        &mut self,
        mut frame: [f32; 16],
        velocity: [f32; 3],
        anchor: [f32; 3],
        mode: u32,
        weight: f32,
        tick: u64,
    ) -> Result<Snapshot, String> {
        if frame[15] != 1. || frame[..12].iter().any(|v| v.abs() > 1.01) {
            return Err("Invalid vert frame basis".into());
        }
        for c in 0..3 {
            let n = (0..3).map(|r| frame[c * 4 + r] * frame[c * 4 + r]).sum::<f32>();
            if (n - 1.).abs() > 0.01 {
                return Err("Non-unit vert contact basis".into());
            }
        }
        self.delta = mul(frame, inverse(self.source.board));
        if mode == 2 {
            self.hand_ticks += 1;
            if let Some(j) = self.source_pose.joints[..self.source_pose.count as usize]
                .iter()
                .find(|j| j.name.starts_with(b"RIGHTHAND\0"))
            {
                let hand = point(self.delta, [j.world[12], j.world[13], j.world[14]]);
                let w = (self.hand_ticks as f32 / 11.).min(1.) * weight;
                for r in 0..3 {
                    let shift = (anchor[r] - hand[r]) * w;
                    frame[12 + r] += shift;
                    self.delta[12 + r] += shift;
                }
            }
        } else {
            self.hand_ticks = 0;
        }
        let mut out = self.source;
        out.root = mul(self.delta, self.source.root);
        out.board = frame;
        out.tick = tick;
        out.state_id = if mode == 2 { 1001 } else { 1000 };
        out.state = [0; 64];
        let name = if mode == 2 {
            b"Handplant".as_slice()
        } else {
            b"VertTransition".as_slice()
        };
        out.state[..name.len()].copy_from_slice(name);
        out.trajectory = point(self.delta, self.source.trajectory);
        out.velocity = velocity;
        out.trajectory_velocity = velocity;
        out.ground_normal = [frame[4], frame[5], frame[6]];
        out.heading = self.source.heading;
        out.body_pitch = frame[9].clamp(-1., 1.).asin();
        self.current = out;
        Ok(out)
    }
    pub fn pose(&self) -> Result<Box<SkaterPose>, String> {
        let mut p = Box::new(SkaterPose {
            version: 1,
            count: self.source_pose.count,
            tick: self.current.tick,
            root: mul(self.delta, self.source_pose.root),
            joints: self.source_pose.joints,
        });
        for j in &mut p.joints[..p.count as usize] {
            j.world = mul(self.delta, j.world);
        }
        Ok(p)
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn rigid_roundtrip() {
        let mut f = [0.; 16];
        f[0] = 0.;
        f[2] = -1.;
        f[5] = 1.;
        f[8] = 1.;
        f[15] = 1.;
        f[12] = 10.;
        f[13] = 3.;
        let id = mul(f, inverse(f));
        for (i, v) in id.iter().enumerate() {
            assert!((*v - if i % 5 == 0 { 1. } else { 0. }).abs() < 1e-5);
        }
    }
}
