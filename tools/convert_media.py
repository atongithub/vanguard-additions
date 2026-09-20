import cv2
import numpy as np
import librosa
import os

# --- 1. Video Conversion ---
video_path = '../car.mp4'
if not os.path.exists(video_path):
    print("Video file not found!")
else:
    cap = cv2.VideoCapture(video_path)
    frames = []
    while True:
        ret, frame = cap.read()
        if not ret:
            break
        # Resize to 160x120
        frame = cv2.resize(frame, (160, 120))
        # Encode to JPEG
        # Quality can be tweaked to save space
        ret, buf = cv2.imencode('.jpg', frame, [int(cv2.IMWRITE_JPEG_QUALITY), 60])
        if ret:
            frames.append(buf.tobytes())

    cap.release()

    if frames:
        mjpeg_data = b''.join(frames)
        header_path = '../firmware_vanguard_backup/main/boot/boot_video_data.h'
        with open(header_path, 'w') as f:
            f.write('#pragma once\n\n')
            f.write(f'const unsigned int video_mjpeg_len = {len(mjpeg_data)};\n')
            f.write('const unsigned char video_mjpeg[] = {\n')
            
            # Write hex array
            for i in range(0, len(mjpeg_data), 12):
                chunk = mjpeg_data[i:i+12]
                hex_chunk = ', '.join([f'0x{b:02x}' for b in chunk])
                if i + 12 < len(mjpeg_data):
                    f.write(f'    {hex_chunk},\n')
                else:
                    f.write(f'    {hex_chunk}\n')
            
            f.write('};\n')
        print(f"Video converted. {len(frames)} frames. {len(mjpeg_data)} bytes.")

# --- 2. Audio Conversion ---
audio_path = '../bootsound.m4a'
if not os.path.exists(audio_path):
    print("Audio file not found!")
else:
    y, sr = librosa.load(audio_path, sr=None)

    # 2. Extract fundamental frequencies (pitches)
    # hop_length determines time resolution (e.g., 512 samples ~23ms frames @ 22kHz)
    # Using 1024 to reduce array size slightly
    hop_length = 512
    frequencies = []
    durations = []
    
    # Use pyin for much better fundamental frequency estimation
    f0, voiced_flag, voiced_probs = librosa.pyin(y, fmin=librosa.note_to_hz('C2'), fmax=librosa.note_to_hz('C7'), sr=sr, frame_length=2048, hop_length=hop_length)
    
    frame_duration_ms = int((hop_length / sr) * 1000)
    current_freq = 0
    current_duration = 0

    for pitch in f0:
        freq = int(pitch) if (pitch > 0 and not np.isnan(pitch)) else 0
        
        # Group identical consecutive pitches to save memory
        if freq == current_freq:
            current_duration += frame_duration_ms
        else:
            if current_duration > 0:
                frequencies.append(current_freq)
                durations.append(current_duration)
            current_freq = freq
            current_duration = frame_duration_ms

    # Append the final note
    if current_duration > 0:
        frequencies.append(current_freq)
        durations.append(current_duration)

    print(f"Audio converted: {len(frequencies)} notes.")
    
    # We will just write a new header with the melody so boot.cpp can include it,
    # or just print it and replace it in boot.cpp.
    # Let's generate a header file.
    audio_header_path = '../firmware_vanguard_backup/main/boot/boot_audio_data.h'
    with open(audio_header_path, 'w') as f:
        f.write('#pragma once\n\n')
        f.write('#include "../audio/audio_manager.h"\n\n')
        f.write(f'static const audio::Note kBootMelody[] = {{\n')
        for i in range(len(frequencies)):
            f.write(f'    {{ {frequencies[i]}, {durations[i]} }},\n')
        f.write('};\n')
        f.write(f'constexpr uint16_t kBootMelodyCount = {len(frequencies)};\n')
    print("Audio header generated.")
