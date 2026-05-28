import json
import os

bundle_path = 'project_bundle_202604011156.jsonl.txt'
target_file = 'Source/ABD-SynthEngine/Protocol/Presets/PresetManagerBase.h'

def restore_file(target_rel_path):
    target_abs = os.path.join(os.getcwd(), target_rel_path.replace('/', '\\'))
    with open(bundle_path, 'r', encoding='utf-8') as f:
        for line in f:
            data = json.loads(line)
            if data['path'].replace('\\', '/') == target_rel_path.replace('\\', '/'):
                print(f"Restoring {target_rel_path}...")
                os.makedirs(os.path.dirname(target_abs), exist_ok=True)
                with open(target_abs, 'w', encoding='utf-8-sig') as out:
                    out.write(data['content'])
                return True
    return False

# Restore critical files
files_to_restore = [
    'Source/ABD-SynthEngine/Protocol/Presets/PresetManagerBase.h',
    'Source/Core/PerformanceState.cpp',
    'Source/Core/PluginEditor.cpp',
    'Source/Core/JunoVoiceManager.h',
    'Source/Core/JunoVoiceManager.cpp'
]

for f in files_to_restore:
    if restore_file(f):
        print(f"Successfully restored {f}")
    else:
        print(f"FAILED to find {f} in bundle")
