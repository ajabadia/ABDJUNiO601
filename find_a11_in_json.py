
import json

path = 'factory_presets.json'
with open(path, 'r') as f:
    data = json.load(f)

presets = data.get('presets', [])
if len(presets) > 0:
    print("First preset state:")
    print(presets[0]['state'][:200])
