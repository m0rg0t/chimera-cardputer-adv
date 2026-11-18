use chimera_cardputer_probe::{BLOCK_FRAMES, Probe, SCORE_CAPACITY};
use std::io::{self, BufWriter, Write};

fn main() -> io::Result<()> {
    let mut args = std::env::args().skip(1);
    let rate: u32 = args.next().expect("sample rate").parse().expect("integer sample rate");
    let oversample: u32 = args.next().expect("oversampling").parse().expect("integer oversampling");
    let blocks: usize = args.next().expect("block count").parse().expect("integer block count");
    let mut voice = Probe::new(rate, oversample, SCORE_CAPACITY, false);
    let mut output = BufWriter::new(io::stdout().lock());
    let mut pcm = [0.0; BLOCK_FRAMES];
    for _ in 0..blocks {
        voice.render(&mut pcm);
        for sample in pcm {
            assert!(sample.is_finite(), "nonfinite PCM");
            output.write_all(&sample.to_le_bytes())?;
        }
    }
    output.flush()
}
