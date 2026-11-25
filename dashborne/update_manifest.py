"""Describe files owned by a build for verified on-device updates."""
import hashlib
import json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]

def write(folder):
    folder = Path(folder)
    files = {str(p.relative_to(folder)): hashlib.sha256(p.read_bytes()).hexdigest()
             for p in sorted(folder.rglob('*')) if p.is_file() and p.name != 'update-manifest.json'}
    manifest = dict(format=1, version=(ROOT/'VERSION').read_text().strip(),
                    stock_os=(ROOT/'STOCK_OS_VERSION').read_text().strip(), files=files)
    (folder/'update-manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')

if __name__ == '__main__':
    write(ROOT/'dashborne/build/card/Dashborne')
