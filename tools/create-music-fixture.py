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
    parser.add_argument('--repeat', type=int, choices=range(1, 17), default=1,
                        help='Repeat the generated PCM in bounded chunks for large-song tests')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    rate = 48000
    variants = ('resonance_demo', 'low', 'high', 'silence') if args.tones else ('resonance_demo',)
    for name in variants:
        pcm = bytearray()
        for frame in range(rate * (16 if name == 'resonance_demo' else 4)):
            t = frame / rate
            beat = t % 0.5
            step = t % 0.125
            if name == 'resonance_demo':
                kick = 0.42 * math.exp(-beat * 17) * math.sin(math.tau * (52 * beat + 5 * (1 - math.exp(-beat * 26))))
                notes = (55, 65.4064, 82.4069, 73.4162)
                bass = 0.20 * math.exp(-beat * 5) * math.sin(math.tau * notes[int(t / 2) % 4] * t)
                arp_note = (220, 329.6276, 440, 523.2511, 659.2551, 440, 329.6276, 293.6648)[int(t / 0.25) % 8]
                arp = 0.10 * math.exp(-(t % 0.25) * 12) * math.sin(math.tau * arp_note * t)
                hat = 0.07 * math.exp(-step * 90) * (math.sin(math.tau * 7200 * t) + math.sin(math.tau * 9317 * t))
                value = kick + bass + arp + hat
            else:
                value = 0 if name == 'silence' else 0.4 * math.sin(math.tau * (80 if name == 'low' else 3500) * t)
            fade = min(1, t * 100, (16 if name == 'resonance_demo' else 4) * 100 - t * 100)
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
