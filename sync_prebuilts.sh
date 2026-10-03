#!/bin/bash
# Fetch the toolchains listed in prebuilts.json into kernel_platform/prebuilts,
# each at its pinned revision. Safe to repeat.
set -euo pipefail
cd "$(dirname "$0")"
python3 - <<'PY'
import json, os, subprocess

def git(*args):
    subprocess.run(['git', '-c', 'http.version=HTTP/1.1', *args], check=True)

for p in json.load(open('prebuilts.json'))['prebuilts']:
    path, revision = p['path'], p['revision']
    if os.path.isdir(os.path.join(path, '.git')):
        head = subprocess.run(['git', '-C', path, 'rev-parse', 'HEAD'],
                              capture_output=True, text=True).stdout.strip()
        if head == revision:
            continue
    os.makedirs(path, exist_ok=True)
    git('-C', path, 'init', '-q')
    for attempt in range(4):
        try:
            git('-C', path, 'fetch', '-q', '--depth=1', p['url'], revision)
            break
        except subprocess.CalledProcessError:
            if attempt == 3:
                raise
    git('-C', path, 'checkout', '-q', '--detach', 'FETCH_HEAD')
    print('fetched', path)
PY
