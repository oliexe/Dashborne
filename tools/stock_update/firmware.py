#!/usr/bin/env python3
"""Download the update offered to a real DASH cloud ID; never flash or execute it."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import sys
from urllib.parse import quote, urlsplit, urlunsplit
from urllib.request import Request, build_opener, HTTPSHandler, HTTPRedirectHandler
from urllib.error import HTTPError, URLError
from .container import decode

MANIFEST_LIMIT = 1024 * 1024
IMAGE_LIMIT = 1024 * 1024 * 1024


def https_url(url):
    if not isinstance(url, str):
        raise ValueError('Download URL must be a string')
    parts = urlsplit(url)
    if parts.scheme != 'https' or not parts.hostname or parts.username or parts.password or parts.fragment:
        raise ValueError('Expected an HTTPS URL without credentials or fragment')
    return url


class HTTPSRedirects(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        https_url(newurl)
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def request(url, headers=None):
    https_url(url)
    opener = build_opener(HTTPSHandler(), HTTPSRedirects())
    return opener.open(Request(url, headers=headers or {}), timeout=45)


def without_query(url):
    p = urlsplit(https_url(url))
    return urlunsplit((p.scheme, p.netloc, p.path, '', ''))


def choose_file(manifest, file_type):
    if not isinstance(manifest, dict):
        raise ValueError('Manifest must be an object')
    version = manifest.get('version')
    if not isinstance(version, str) or not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise ValueError('Unrecognized manifest version format; inspect schema privately')
    files = manifest.get('files')
    if not isinstance(files, list) or not all(isinstance(f, dict) for f in files):
        raise ValueError('Unrecognized manifest files schema')
    selected = [f for f in files if type(f.get('type')) is int and f['type'] == file_type]
    if len(selected) != 1:
        raise ValueError('Expected exactly one file of the selected type')
    return version, https_url(selected[0].get('url'))


def fetch(device_id, output, file_type=0, download_only=False):
    if not isinstance(device_id, str) or not re.fullmatch(r'[A-Za-z0-9_.-]{1,128}', device_id):
        raise ValueError('Invalid cloud ID; use the exact ID from the user or device')
    if file_type != 0 and not download_only:
        raise ValueError('Non-display artifacts require --download-only and separate format review')
    output = Path(output).expanduser().resolve()
    for parent in (output, *output.parents):
        if (parent / '.git').exists():
            raise ValueError('Choose a private output directory outside a Git repository')
    output.mkdir(parents=True, mode=0o700, exist_ok=False)
    endpoint = 'https://api.cloud.enhance.bg/api/v1/devices/' + quote(device_id, safe='') + '/firmware'
    with request(endpoint, {'Content-Type': 'application/json', 'Accept': 'application/json',
                            'User-Agent': 'Mozilla/5.0'}) as response:
        raw = response.read(MANIFEST_LIMIT + 1)
    if len(raw) > MANIFEST_LIMIT:
        raise ValueError('Manifest exceeds size limit')
    manifest = json.loads(raw)
    version, url = choose_file(manifest, file_type)
    artifact = output / ('update.img' if file_type == 0 else f'artifact-type-{file_type}.bin')
    pending = output / 'download.part'
    digest = hashlib.sha256()
    size = 0
    try:
        with request(url) as response, pending.open('xb') as stream:
            declared = response.headers.get('Content-Length')
            expected = int(declared) if declared is not None else None
            if expected is not None and not 0 < expected <= IMAGE_LIMIT:
                raise ValueError('Invalid or oversized download length')
            while True:
                chunk = response.read(1024 * 1024)
                if not chunk:
                    break
                size += len(chunk)
                if size > IMAGE_LIMIT:
                    raise ValueError('Download exceeds size limit')
                stream.write(chunk)
                digest.update(chunk)
            if not size or (expected is not None and size != expected):
                raise ValueError('Incomplete download')
            final_source = without_query(response.geturl())
        pending.rename(artifact)
    except Exception:
        pending.unlink(missing_ok=True)
        raise
    metadata = {
        'retrieved_at': datetime.now(timezone.utc).isoformat(),
        'offered_version': version, 'isLatest': manifest.get('isLatest'),
        'selected_file_type': file_type, 'bytes': size, 'sha256': digest.hexdigest(),
        'source_without_query': final_source,
        'available_files': [{'type': f.get('type'), 'source_without_query': without_query(f['url'])}
                            for f in manifest['files']],
        'verification': 'downloaded; container verification pending',
    }
    metadata_path = output / 'provenance.json'
    metadata_path.write_text(json.dumps(metadata, indent=2) + '\n')
    if not download_only:
        payload, container = decode(artifact.read_bytes())
        if not container['ext_superblock_magic_valid']:
            raise ValueError('Unexpected payload filesystem; retained for private inspection')
        with (output / 'partition.img').open('xb') as stream:
            stream.write(payload)
        metadata['container'] = container
        metadata['verification'] = 'header CRC32 and payload MD5 verified; not a signature check'
    else:
        metadata['verification'] = 'download only; format and integrity not yet verified'
    metadata_path.write_text(json.dumps(metadata, indent=2) + '\n')
    return metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    identity = parser.add_mutually_exclusive_group(required=True)
    identity.add_argument('--identity', type=Path, help='Private JSON containing dash_device_id')
    identity.add_argument('--device-id', help='Actual user/device cloud ID')
    parser.add_argument('--output', required=True, type=Path, help='New private directory outside Git')
    parser.add_argument('--file-type', type=int, default=0)
    parser.add_argument('--download-only', action='store_true', help='Retain another artifact for separate format analysis')
    args = parser.parse_args()
    os.umask(0o077)
    try:
        device_id = json.loads(args.identity.read_text())['dash_device_id'] if args.identity else args.device_id
        result = fetch(device_id, args.output, args.file_type, args.download_only)
    except HTTPError as exc:
        print(f'Firmware request failed: HTTP {exc.code}; no update performed.', file=sys.stderr)
        return 1
    except (OSError, ValueError, KeyError, URLError) as exc:
        # Network exception strings may contain signed URLs or device identifiers.
        print(f'Firmware retrieval stopped ({type(exc).__name__}); inspect private output and schema.', file=sys.stderr)
        return 1
    print(f"Offered version: {result['offered_version']}; bytes: {result['bytes']}; SHA-256: {result['sha256']}")
    print(f"Saved in {args.output}; {result['verification']}")
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
