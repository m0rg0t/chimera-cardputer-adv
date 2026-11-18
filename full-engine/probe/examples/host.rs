use chimera::control::score::{Gesture, GestureScore};
use chimera_cardputer_probe::{BLOCK_FRAMES, Probe, SCORE_CAPACITY};
use std::alloc::{GlobalAlloc, Layout, System};
use std::sync::atomic::{AtomicUsize, Ordering::Relaxed};
use std::time::Instant;

struct MeasuredAllocator;
static LIVE: AtomicUsize = AtomicUsize::new(0);
static PEAK: AtomicUsize = AtomicUsize::new(0);
static ALLOCS: AtomicUsize = AtomicUsize::new(0);

unsafe impl GlobalAlloc for MeasuredAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        let memory = unsafe { System.alloc(layout) };
        if !memory.is_null() {
            let live = LIVE.fetch_add(layout.size(), Relaxed) + layout.size();
            PEAK.fetch_max(live, Relaxed);
            ALLOCS.fetch_add(1, Relaxed);
        }
        memory
    }

    unsafe fn dealloc(&self, memory: *mut u8, layout: Layout) {
        LIVE.fetch_sub(layout.size(), Relaxed);
        unsafe { System.dealloc(memory, layout) };
    }
}

#[global_allocator]
static ALLOCATOR: MeasuredAllocator = MeasuredAllocator;

fn main() {
    println!("Gesture={} bytes; stock score reserve={} bytes",
        size_of::<Gesture>(), GestureScore::CAPACITY * size_of::<Gesture>());
    let mut audition = Probe::new(24_000, 1, SCORE_CAPACITY, false);
    audition.render(&mut [0.0; BLOCK_FRAMES]); // Warm the process-wide phoneme tables.
    drop(audition);

    for (rate, oversample) in [(24_000, 1), (24_000, 2), (48_000, 2)] {
        let baseline = LIVE.load(Relaxed);
        PEAK.store(baseline, Relaxed);
        let mut compact = Probe::new(rate, oversample, SCORE_CAPACITY, false);
        let init_live = LIVE.load(Relaxed) - baseline;
        let init_peak = PEAK.load(Relaxed) - baseline;
        let mut reference = Probe::new(rate, oversample, GestureScore::CAPACITY, true);
        let mut actual = [0.0; BLOCK_FRAMES];
        let mut expected = [0.0; BLOCK_FRAMES];
        let blocks = compact.audition_frames.div_ceil(BLOCK_FRAMES);
        let mut render_time = std::time::Duration::ZERO;
        let mut render_allocs = 0;
        let mut audio_peak = 0.0f32;
        for block in 0..blocks {
            let allocations = ALLOCS.load(Relaxed);
            let t0 = Instant::now();
            compact.render(std::hint::black_box(&mut actual));
            render_time += t0.elapsed();
            if block > 0 {
                render_allocs += ALLOCS.load(Relaxed) - allocations;
            }
            reference.render(&mut expected);
            assert!(actual.iter().zip(expected).all(|(a, b)| a.is_finite() && a.to_bits() == b.to_bits()));
            audio_peak = audio_peak.max(actual.iter().fold(0.0f32, |p, x| p.max(x.abs())));
        }
        assert!(audio_peak > 0.003);
        assert_eq!(render_allocs, 0);
        println!("{rate} Hz {oversample}x: {} gestures, {init_live} B live and {init_peak} B peak heap above warm tables, {} bit-identical sample pairs, {:.1}% desktop core",
                 compact.gestures, blocks * BLOCK_FRAMES,
                 100.0 * render_time.as_secs_f64() * f64::from(rate) / (blocks * BLOCK_FRAMES) as f64);
    }
}
