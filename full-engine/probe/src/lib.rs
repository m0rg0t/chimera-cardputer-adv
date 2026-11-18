//! Exercise CHIMERA's original human airway, vocal folds, and gesture planner.
use chimera::anatomy::human::{BODY_MALE, BodyMix};
use chimera::phonetics::Segment;
use chimera::phonetics::planner::{PlanOptions, plan_utterance};
use chimera::voice::human_voice::HumanVoice;

pub const SCORE_CAPACITY: usize = 64;
pub const BLOCK_FRAMES: usize = 256;
// The fixed benchmark starts from phonemes so it needs no Unicode normalizer on the device.
fn sample_segments() -> Vec<Segment> {
    [
        ("h", 0, false), ("ə", 0, false), ("l", 0, false), ("oʊ", 1, true),
        ("ð", 0, false), ("ə", 0, true),
        ("s", 0, false), ("ʌ", 1, false), ("n", 0, true),
        ("ɪ", 0, false), ("z", 0, true),
        ("ʃ", 0, false), ("aɪ", 1, false), ("n", 0, false),
        ("ɪ", 0, false), ("ŋ", 0, true),
        ("t", 0, false), ("ə", 0, false), ("d", 0, false), ("eɪ", 1, false),
    ].into_iter().map(|(ipa, stress, word_end)| Segment {
        ipa: ipa.into(), stress: Some(stress), word_end: word_end.then_some(true), ..Default::default()
    }).collect()
}

pub struct Probe {
    voice: HumanVoice,
    pub gestures: usize,
    pub audition_frames: usize,
}

impl Probe {
    pub fn new(rate: u32, oversample: u32, capacity: usize, capture_scope: bool) -> Self {
        let plan = plan_utterance(&sample_segments(), &PlanOptions {
            body: BodyMix::MALE, t0: 0.06, ..Default::default()
        });
        let audition_frames = ((plan.end + 0.35) * f64::from(rate)) as usize;
        let gestures = plan.gestures.len();
        let score = plan.gestures.into_boxed_slice();
        assert!(gestures <= capacity);
        let mut voice = HumanVoice::with_options(
            f64::from(rate), BODY_MALE, oversample as usize, capacity, capture_scope,
        );
        voice.schedule(&score);
        voice.set_gate(1.0);
        Self { voice, gestures, audition_frames }
    }

    pub fn render(&mut self, pcm: &mut [f32]) {
        self.voice.render(pcm);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn chimera_probe_new(rate: u32, oversample: u32, gestures: &mut u32) -> Box<Probe> {
    let probe = Box::new(Probe::new(rate, oversample, SCORE_CAPACITY, false));
    *gestures = probe.gestures as u32;
    probe
}

#[unsafe(no_mangle)]
pub extern "C" fn chimera_probe_render(probe: &mut Probe, pcm: &mut [f32; BLOCK_FRAMES]) {
    probe.render(pcm);
}

#[unsafe(no_mangle)]
pub extern "C" fn chimera_probe_free(_probe: Box<Probe>) {}

#[unsafe(no_mangle)]
pub extern "C" fn chimera_probe_scalar_bytes() -> u32 {
    std::mem::size_of::<f64>() as u32
}
