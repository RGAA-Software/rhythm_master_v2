"""Write an original 16-second PCM synth loop for audio/visual acceptance (no samples)."""

import argparse
import math
from pathlib import Path
import struct
import wave


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--tones', action='store_true')
    parser.add_argument('--quality', action='store_true',
                        help='Add mid-band and quiet/loud PCM variants for content review')
    parser.add_argument('--low-hz', type=float, default=80,
                        help='Low-tone fixture frequency (20..16000 Hz)')
    parser.add_argument('--high-hz', type=float, default=3500,
                        help='High-tone fixture frequency (20..16000 Hz)')
    parser.add_argument('--repeat', type=int, choices=range(1, 17), default=1,
                        help='Repeat the generated PCM in bounded chunks for large-song tests')
    args = parser.parse_args()
    if args.quality and not args.tones:
        parser.error('--quality requires --tones')
    if not all(math.isfinite(value) and 20 <= value <= 16000 for value in (args.low_hz, args.high_hz)):
        parser.error('Tone frequencies must be finite and within 20..16000 Hz')
    args.output.mkdir(parents=True, exist_ok=True)
    rate = 48000
    variants = ('resonance_demo', 'low', 'high', 'silence') if args.tones else ('resonance_demo',)
    if args.quality:
        variants += ('mid', 'quiet', 'loud')
    for name in variants:
        pcm = bytearray()
        is_music = name in ('resonance_demo', 'quiet', 'loud')
        duration = 16 if is_music else 4
        for frame in range(rate * duration):
            t = frame / rate
            beat = t % 0.5
            step = t % 0.125
            if is_music:
                kick = 0.42 * math.exp(-beat * 17) * math.sin(math.tau * (52 * beat + 5 * (1 - math.exp(-beat * 26))))
                notes = (55, 65.4064, 82.4069, 73.4162)
                bass = 0.20 * math.exp(-beat * 5) * math.sin(math.tau * notes[int(t / 2) % 4] * t)
                arp_note = (220, 329.6276, 440, 523.2511, 659.2551, 440, 329.6276, 293.6648)[int(t / 0.25) % 8]
                arp = 0.10 * math.exp(-(t % 0.25) * 12) * math.sin(math.tau * arp_note * t)
                hat = 0.07 * math.exp(-step * 90) * (math.sin(math.tau * 7200 * t) + math.sin(math.tau * 9317 * t))
                value = kick + bass + arp + hat
                value *= 0.25 if name == 'quiet' else 1.25 if name == 'loud' else 1
            else:
                frequency = {'low': args.low_hz, 'mid': 700, 'high': args.high_hz}.get(name, 0)
                value = 0.4 * math.sin(math.tau * frequency * t)
            fade = min(1, t * 100, duration * 100 - t * 100)
            sample = round(max(-1, min(1, value * fade)) * 32767)
            pcm.extend(struct.pack('<hh', sample, sample))
        destination = args.output / (name + '.wav')
        with wave.open(str(destination), 'wb') as output:
            output.setnchannels(2)
            output.setsampwidth(2)
            output.setframerate(rate)
            for _ in range(args.repeat):
                output.writeframesraw(pcm)
    print(args.output)


if __name__ == '__main__':
    main()
