import re

header_path = '../firmware_vanguard_backup/main/boot/boot_audio_data.h'
ino_path = 'simulator_audio.ino'

with open(header_path, 'r') as f:
    content = f.read()

# Match the data inside kBootMelody array
match = re.search(r'kBootMelody\[\] = \{\s*(.*?)\s*\};', content, re.DOTALL)
if match:
    notes_str = match.group(1)
    
    # Extract all {freq, duration} pairs
    notes = re.findall(r'\{\s*(\d+),\s*(\d+)\s*\}', notes_str)
    
    with open(ino_path, 'w') as f:
        f.write('const int buzzer = 6; // register buzzer to arduino pin 6\n\n')
        f.write('void setup() {\n')
        f.write('  pinMode(buzzer, OUTPUT); // Set buzzer - pin 6 as an output\n')
        f.write('}\n\n')
        f.write('void loop() {\n')
        
        for freq, duration in notes:
            freq = int(freq)
            duration = int(duration)
            if freq == 0:
                f.write(f'  noTone(buzzer);\n')
                f.write(f'  delay({duration});\n\n')
            else:
                f.write(f'  tone(buzzer, {freq});\n')
                f.write(f'  delay({duration});\n\n')
                
        # Optional: Add a long delay or stop at the end so it doesn't loop instantly
        f.write('  noTone(buzzer);\n')
        f.write('  delay(5000);\n')
        f.write('}\n')
        
    print(f'Generated {ino_path} with {len(notes)} notes.')
else:
    print('Failed to parse boot_audio_data.h')
