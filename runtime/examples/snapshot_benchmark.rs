use skate_host::bridge::Session;
use std::{
    alloc::{GlobalAlloc, Layout, System},
    hint::black_box,
    path::Path,
    sync::atomic::{AtomicU64, Ordering},
    time::Instant,
};
struct Counting;
static ALLOCS: AtomicU64 = AtomicU64::new(0);
unsafe impl GlobalAlloc for Counting {
    unsafe fn alloc(&self, l: Layout) -> *mut u8 {
        ALLOCS.fetch_add(1, Ordering::Relaxed);
        unsafe { System.alloc(l) }
    }
    unsafe fn dealloc(&self, p: *mut u8, l: Layout) {
        unsafe { System.dealloc(p, l) }
    }
    unsafe fn realloc(&self, p: *mut u8, l: Layout, n: usize) -> *mut u8 {
        ALLOCS.fetch_add(1, Ordering::Relaxed);
        unsafe { System.realloc(p, l, n) }
    }
}
#[global_allocator]
static A: Counting = Counting;
fn run() {
    #[cfg(target_arch = "x86_64")]
    unsafe {
        let mut prior = 0u32;
        std::arch::asm!("stmxcsr [{0}]",in(reg)&mut prior,options(nostack,preserves_flags));
        let mode = (prior | 0x8040) & !0x6000;
        std::arch::asm!("ldmxcsr [{0}]",in(reg)&mode,options(nostack,preserves_flags));
    }
    let root = std::env::args()
        .nth(1)
        .expect("usage: snapshot_benchmark <prepared assets root>");
    let triangles = vec![
        [[-100., 0., -100.], [100., 0., 100.], [100., 0., -100.]],
        [[-100., 0., -100.], [-100., 0., 100.], [100., 0., 100.]],
    ];
    let mut s = Session::new(Path::new(&root), triangles, vec![], [0., 0., 0.], 0.).unwrap();
    s.activate([0., 0., 0.], 0.).unwrap();
    for _ in 0..100 {
        let old = s.pose();
        let new = s.host_snapshot();
        assert_eq!(old.root, new.0);
        assert_eq!(old.velocity, new.1);
        assert_eq!(old.tick, new.2);
        assert_eq!(old.state, new.3);
    }
    let n = 20000;
    for round in 0..5 {
        for lightweight in [false, true] {
            ALLOCS.store(0, Ordering::Relaxed);
            let start = Instant::now();
            for _ in 0..n {
                if lightweight {
                    black_box(s.host_snapshot());
                } else {
                    let p = s.pose();
                    black_box((p.root, p.velocity, p.tick, p.state));
                }
            }
            let elapsed = start.elapsed();
            let a = ALLOCS.load(Ordering::Relaxed);
            println!(
                "round={round} lightweight={lightweight} operations={n} elapsed_ns={} allocations={a}",
                elapsed.as_nanos()
            );
        }
    }
    println!("PASS status fields identical; no simulation advanced during measurement");
}

fn main() {
    std::thread::Builder::new()
        .name("read-only snapshot benchmark".into())
        .stack_size(16 * 1024 * 1024)
        .spawn(run)
        .unwrap()
        .join()
        .unwrap();
}
