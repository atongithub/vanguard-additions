import cv2
import sys
import os

def main():
    if len(sys.argv) < 3:
        print("Usage: python encode_video.py <input_video.mp4> <output_file.h>")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    # Ensure output directory exists
    os.makedirs(os.path.dirname(output_file), exist_ok=True)

    cap = cv2.VideoCapture(input_file)
    if not cap.isOpened():
        print(f"Error: Could not open {input_file}")
        sys.exit(1)

    print(f"Encoding {input_file} to 160x120 MJPEG C array...")
    
    out_f = open(output_file, 'w')
    out_f.write("#pragma once\n\n")
    out_f.write("// Auto-generated MJPEG video array (160x120)\n")
    out_f.write("const unsigned char video_mjpeg[] = {\n")
    
    frame_count = 0
    total_bytes = 0
    
    # Target 15 FPS: We'll just assume the video is 15fps or we just process all frames.
    # To keep size down, we can skip frames if the source is 30/60 FPS.
    fps = cap.get(cv2.CAP_PROP_FPS)
    skip_frames = 1
    if fps > 25:
        skip_frames = int(fps / 15)  # Drop to ~15 fps
    
    frame_idx = 0
    
    while True:
        ret, frame = cap.read()
        if not ret:
            break
            
        if frame_idx % skip_frames != 0:
            frame_idx += 1
            continue
            
        frame_idx += 1
        
        # Resize to 160x120
        frame_resized = cv2.resize(frame, (160, 120), interpolation=cv2.INTER_AREA)
        
        # Encode to JPEG (Quality 60 is a good balance for 160x120)
        encode_param = [int(cv2.IMWRITE_JPEG_QUALITY), 60]
        result, encimg = cv2.imencode('.jpg', frame_resized, encode_param)
        
        if not result:
            print("Failed to encode frame!")
            continue
            
        # Write bytes to C array
        bytes_data = encimg.tobytes()
        total_bytes += len(bytes_data)
        
        # Format as hex
        hex_str = ", ".join(f"0x{b:02x}" for b in bytes_data)
        out_f.write(f"    {hex_str},\n")
        
        frame_count += 1

    out_f.write("};\n\n")
    out_f.write(f"const unsigned int video_mjpeg_len = {total_bytes};\n")
    out_f.write(f"const unsigned int video_mjpeg_frames = {frame_count};\n")
    out_f.close()
    
    cap.release()
    print(f"Done! Wrote {frame_count} frames ({total_bytes / 1024.0:.1f} KB) to {output_file}")

if __name__ == "__main__":
    main()
