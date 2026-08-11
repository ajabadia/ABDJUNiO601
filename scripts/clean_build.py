import os
import shutil
import subprocess

build_dir = 'build'
if os.path.exists(build_dir):
    print(f"Removing {build_dir}...")
    shutil.rmtree(build_dir)

os.makedirs(build_dir)
print("Starting build_auto.bat...")
subprocess.run(['cmd.exe', '/c', 'build_auto.bat'], check=True)
