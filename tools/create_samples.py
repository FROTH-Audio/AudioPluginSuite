#!/usr/bin/env python3
"""Generate sample audio files for Sgraffito plugin."""

import numpy as np
import struct
import wave
import subprocess as sp
import os

def create_sample(name, description, f0=440, duration=0.5, 
                  envelope_type="exp", decay=0.3):
    """Create a sine-based sample with the given envelope."""
    
    sr = 48000
    t = np.linspace(0, duration, int(sr * duration), endpoint=False)
    
    # Generate base signal
    if envelope_type == "exp":
        envelope = np.exp(-t / (duration * decay))
    elif envelope_type == "linear":
        envelope = 1.0 - t / duration
    else:
        envelope = np.ones_like(t)
    
    signal = envelope * np.sin(2 * np.pi * f0 * t)
    
    # Normalize
    max_val = np.max(np.abs(signal))
    if max_val > 0:
        signal = signal / max_val
    
    # Write as WAV first, then convert to FLAC
    wav_path = os.path.join("Resources", "samples", name.replace('.flac', '.wav'))
    flac_path = wav_path.replace('.wav', '.flac')
    print(f"Creating {name}...")
    
    with wave.open(wav_path, 'w') as wav_file:
        n_channels = 1
        sampwidth = 2
        numframes = len(signal)
        
        wav_file.setnchannels(n_channels)
        wav_file.setsampwidth(sampwidth)
        wav_file.setframerate(sr)
        
        for i in range(numframes):
            sample = int(32767 * signal[i])
            packed = struct.pack('<h', sample)
            wav_file.writeframes(packed)
    
    # Convert to FLAC using ffmpeg
    result = sp.run([
        'ffmpeg', '-y', '-i', wav_path, '-ar', '48000', '-ac', '1', '-c:a', 'flac', flac_path
    ], capture_output=True, text=True)
    
    # Clean up WAV (we only need FLAC)
    os.remove(wav_path)
    
    size = os.path.getsize(flac_path)
    print(f"  Created {name}: {size / 1024:.1f} KB")
    
    return flac_path

def main():
    # Create grain sample - short percussive burst
    create_sample("grain_4k_flac.flac", "Grain Sample (4 kHz)", 
                  f0=440, duration=0.3, envelope_type="exp", decay=0.2)
    
    # Create cloud sample - longer, more complex
    create_sample("cloud_4k_flac.flac", "Cloud Sample (4 kHz)", 
                  f0=200, duration=1.5, envelope_type="exp", decay=0.6)
    
    print("\nAll samples created!")

if __name__ == "__main__":
    main()
