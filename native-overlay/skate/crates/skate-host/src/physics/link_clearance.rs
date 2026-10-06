//! Body-clearance-only host profile. Board contacts and ground response stay stock.
use super::SkaterRuntime;
use skate_core::{
    math::Vector3,
    physics::{board_world::BoardWorldVolume, world_contact::ContactPrimitive},
};
pub(super) fn active(skater: &SkaterRuntime) -> bool {
    use skate_core::player::state::PhysicalStateId;
    skater.host_clearance_scale < 0.999
        && !skater.skeleton_collision.is_ragdoll
        && matches!(
            skater.player_state.current(),
            PhysicalStateId::PhysicsGround | PhysicalStateId::BipedGround | PhysicalStateId::GroundAnimation
        )
}
pub(super) fn volumes(s: &SkaterRuntime, input: &[BoardWorldVolume]) -> Vec<BoardWorldVolume> {
    let scale = s.host_clearance_scale;
    let com = s.skeleton.record.centre_of_mass;
    let anchor = Vector3::new(
        com[0],
        s.skeleton.record.pose[15][3][1].min(s.skeleton.record.pose[19][3][1]),
        com[2],
    );
    let point = |v: Vector3| {
        Vector3::new(
            anchor.x + (v.x - anchor.x) * scale,
            anchor.y + (v.y - anchor.y) * scale,
            anchor.z + (v.z - anchor.z) * scale,
        )
    };
    input
        .iter()
        .cloned()
        .map(|mut v| {
            v.primitive = match v.primitive {
                ContactPrimitive::Sphere(mut p) => {
                    p.center = point(p.center);
                    p.radius *= scale;
                    ContactPrimitive::Sphere(p)
                }
                ContactPrimitive::Capsule {
                    center,
                    axis,
                    half_length,
                    radius,
                } => ContactPrimitive::Capsule {
                    center: point(center),
                    axis,
                    half_length: half_length * scale,
                    radius: radius * scale,
                },
                ContactPrimitive::RoundedBox {
                    center,
                    basis,
                    half_extents,
                    radius,
                } => ContactPrimitive::RoundedBox {
                    center: point(center),
                    basis,
                    half_extents: Vector3::new(half_extents.x * scale, half_extents.y * scale, half_extents.z * scale),
                    radius: radius * scale,
                },
                other => other,
            };
            v
        })
        .collect()
}
