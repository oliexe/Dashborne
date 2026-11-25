"""Bounded, read-only traversal of update bundles, partitions and Linux filesystems.

7-Zip handles disk tables, filesystems and archives; vendor files are never executed.
All extracted data goes to generated flat paths, not paths supplied by the archive.
"""
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import resource
import shutil
import struct
import subprocess
import zlib

from .container import decode

MAX_MEMBER = 2 * 1024**3
MAX_EXPANDED = 4 * 1024**3
MAX_METADATA = 128 * 1024**2
MAX_NODES = 30000
MAX_DEPTH = 8
MAX_LISTING = 16 * 1024**2
SYSTEM_AREAS = ('kernel', 'bootloader', 'rootfs', 'device-tree', 'drivers')
PATH_REFERENCES = ('/mnt/extsd', 'startupLibPath', 'EasyUI.cfg', '/bin/zkgui',
                   '/lib/libeasyui.so', '/res/lib/libzkgui.so', '/dev/fb', '/dev/input',
                   'wlan0', 'hostapd', 'wpa_supplicant', 'adbd', 'udhcpc',
                   'mount', 'insmod', 'modprobe', 'mmcblk', 'mtdparts')
CONFIG_KEYS = re.compile(rb'(?:CONFIG_(?:ARM|AEABI|VFP|MODULES|MODVERSIONS|IKCONFIG|BLK_DEV_INITRD|'
                         rb'DEVTMPFS|TMPFS|OVERLAY_FS|EXT[234]_FS|VFAT_FS|FAT_FS|SQUASHFS|MMC|'
                         rb'INPUT_EVDEV|FB|DRM|CFG80211|MAC80211|WIRELESS|WLAN|USB)[A-Z0-9_]*'
                         rb'=(?:y|m|n)|# CONFIG_[A-Z0-9_]+ is not set)')


def fingerprint_file(path):
    digest = hashlib.sha256(); crc = 0; size = 0
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024**2), b''):
            digest.update(chunk); crc = zlib.crc32(chunk, crc); size += len(chunk)
    return {'bytes': size, 'sha256': digest.hexdigest(), 'crc32': f'{crc:08x}'}


def run_to_file(args, output, limit, timeout=60):
    """Limit output even if a corrupt archive lies about its expanded size."""
    def limits():
        resource.setrlimit(resource.RLIMIT_FSIZE, (limit, limit))
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    with output.open('xb') as stream:
        result = subprocess.run(args, stdin=subprocess.DEVNULL, stdout=stream,
                                stderr=subprocess.DEVNULL, timeout=timeout, preexec_fn=limits)
    if output.stat().st_size > limit:
        raise ValueError('Tool output limit exceeded')
    return result.returncode


def archive_members(listing):
    if '\n----------\n' not in listing:
        raise ValueError('Unrecognized archive listing')
    result = {}; seen = set()
    for block in listing.split('\n----------\n', 1)[1].strip().split('\n\n'):
        fields = dict(line.split(' = ', 1) for line in block.splitlines() if ' = ' in line)
        name = fields.get('Path', '')
        # Reject ambiguous selectors; 7-Zip also receives -spd to disable wildcard matching.
        if (not name or PurePosixPath(name).is_absolute() or '..' in PurePosixPath(name).parts or
                not re.fullmatch(r'[A-Za-z0-9_./+ @,=:~\[\]-]+', name) or name.startswith('-')):
            raise ValueError('Unsupported archive member path')
        normalized = str(PurePosixPath(name))
        if normalized in seen:
            raise ValueError('Duplicate archive member')
        seen.add(normalized)
        mode = fields.get('Mode', '')
        if fields.get('Folder') == '+' or mode.startswith('d'):
            continue
        item = {'mode': mode, 'partition_metadata': {k:fields[k] for k in ('Offset','Type','Primary','File System') if k in fields}}
        link = fields.get('Symbolic Link') or fields.get('Hard Link')
        if link or mode.startswith('l'):
            item.update(kind='link', target=link or 'unknown')
        elif mode.startswith(('b', 'c', 'p', 's')):
            item['kind'] = 'special'
        else:
            size = int(fields.get('Size', '-1'))
            if not 0 <= size <= MAX_MEMBER:
                raise ValueError('Invalid archive file size')
            item.update(kind='file', bytes=size)
        result[name] = item
    if len(result) > MAX_NODES or sum(x.get('bytes', 0) for x in result.values()) > MAX_EXPANDED:
        raise ValueError('Archive inventory exceeds budget')
    return result


def root_prefix(names):
    """Recognize a root tree by contents, never confuse app lib/ with rootfs /lib/."""
    names = {str(PurePosixPath(n)) for n in names}
    for name in sorted(names):
        for anchor in ('etc/os-release', 'etc/inittab', 'sbin/init', 'bin/busybox'):
            if name == anchor or name.endswith('/'+anchor):
                prefix = name[:-len(anchor)]
                if any(n.startswith(prefix+'lib/') or n.startswith(prefix+'usr/lib/') for n in names):
                    return prefix
    return None


def file_roles(name, data, root=False):
    base = PurePosixPath(name).name.lower(); roles = []
    if b'Linux version ' in data or re.fullmatch(r'(?:[uzv]?image|vmlinuz|vmlinux|kernel)(?:[._-].*)?', base):
        roles.append('kernel')
    if b'U-Boot ' in data or re.search(r'(?:u-boot|uboot|bootloader|^spl(?:\.|$))', base):
        roles.append('bootloader')
    if data[:4] == b'\xd0\x0d\xfe\xed' or base.endswith(('.dtb', '.dtbo')):
        roles.append('device-tree')
    if b'vermagic=' in data or re.search(r'\.ko(?:\.(?:xz|gz|zst))?$', base) or (root and '/firmware/' in '/'+name):
        roles.append('drivers')
    if root:
        roles.append('rootfs')
    return roles


def dtb_metadata(data):
    """Read bounded FDT properties, including embedded FIT payloads, without dtc execution."""
    if len(data) < 40:
        raise ValueError('Truncated device tree')
    magic, total, off_struct, off_strings, _, version, _, _, size_strings, size_struct = struct.unpack_from('>10I', data)
    if magic != 0xd00dfeed or version < 17 or not 40 <= total <= len(data):
        raise ValueError('Unsupported device tree header')
    if off_struct+size_struct > total or off_strings+size_strings > total:
        raise ValueError('Device tree bounds')
    strings = data[off_strings:off_strings+size_strings]
    cursor = off_struct; end = off_struct+size_struct; stack = []; props = []; images = {}
    def cstring(blob, offset, stop):
        last = blob.find(b'\0', offset, stop)
        if last < 0:raise ValueError('Device tree string bounds')
        return blob[offset:last].decode('ascii', errors='replace'), last+1
    while cursor+4 <= end:
        token = struct.unpack_from('>I', data, cursor)[0]; cursor += 4
        if token == 1:
            name, cursor = cstring(data, cursor, end); cursor = (cursor+3)&~3; stack.append(name)
        elif token == 2:
            if not stack:raise ValueError('Device tree node underflow')
            stack.pop()
        elif token == 3:
            if cursor+8 > end:raise ValueError('Device tree property bounds')
            length, nameoff = struct.unpack_from('>II', data, cursor); cursor += 8
            if cursor+length > end or nameoff >= len(strings):raise ValueError('Device tree value bounds')
            name, _ = cstring(strings, nameoff, len(strings)); value = data[cursor:cursor+length]
            node = '/'.join(stack); cursor = (cursor+length+3)&~3
            if node.startswith('/images/') and node.count('/') == 2:
                images.setdefault(node, {})[name] = value
            if name in ('compatible', 'model', 'status', 'device_type', 'bootargs'):
                # Bootargs may contain private values: expose only relevant token presence.
                if name == 'bootargs':value = ','.join(x for x in PATH_REFERENCES if x.encode() in value).encode()
                props.append({'node': node, 'property': name, 'value': value[:512].replace(b'\0', b',').decode('ascii', errors='replace')})
                if len(props) > 2000:raise ValueError('Device tree metadata limit')
        elif token == 4:
            continue
        elif token == 9:
            if stack:raise ValueError('Unclosed device tree node')
            return {'version': version, 'properties': props, 'fit': bool(images)}, images, total
        else:
            raise ValueError('Unknown device tree token')
    raise ValueError('Missing device tree end')


def metadata(name, data, roles):
    result = {'roles': roles}
    for label, pattern in (('kernel_versions', rb'Linux version ([A-Za-z0-9.+_~-]{1,100})'),
                           ('bootloader_versions', rb'U-Boot ([A-Za-z0-9.+_~-]{1,100})'),
                           ('module_vermagic', rb'vermagic=([^\0\n]{1,200})'),
                           ('module_dependencies', rb'depends=([^\0\n]{0,200})')):
        matches = re.findall(pattern, data)
        if matches:result[label] = sorted({x.decode('ascii', errors='replace') for x in matches})[:100]
    if 'kernel' in roles or PurePosixPath(name).name.startswith(('config-', '.config')):
        config = data
        offset = data.find(b'IKCFG_ST\x1f\x8b')
        if offset >= 0:
            decoder = zlib.decompressobj(31)
            config = decoder.decompress(data[offset+8:], 1024**2+1)
            if len(config) > 1024**2 or not decoder.eof:raise ValueError('Kernel config decompression incomplete')
        flags = sorted({x.decode('ascii') for x in CONFIG_KEYS.findall(config)})
        if flags:
            result['kernel_config'] = flags[:1000]
            if any(re.match(r'CONFIG_(?:MMC|INPUT|FB|DRM|WLAN|WIRELESS|USB|CFG80211|MAC80211)', f) for f in flags):
                result['builtin_driver_config'] = True
                if 'drivers' not in roles:roles.append('drivers')
    refs = [x for x in PATH_REFERENCES if x.encode() in data]
    if refs:result['integration_references'] = refs
    return result


def container_hint(name, header):
    base = PurePosixPath(name).name.lower()
    return (base.endswith(('.img','.bin','.fw','.goc','.zip','.tar','.gz','.xz','.bz2','.lzma',
                           '.lz4','.zst','.7z','.cpio','.squashfs','.ext4','.itb','.fit','.dtb','.dtbo')) or
            header.startswith((b'ZKSWE',b'PK\x03\x04',b'\x1f\x8b',b'\xfd7zXZ',b'hsqs',b'070701',
                               b'070702',b'\x27\x05\x19\x56',b'\xd0\x0d\xfe\xed')) or
            header[257:262] == b'ustar' or header[1080:1082] == b'\x53\xef' or
            header[512:520] == b'EFI PART' or header[510:512] == b'\x55\xaa')


def archive_switch(header, size):
    # Auto-detection can jump directly into the FIRST filesystem in a raw disk.
    # Force disk-table handling so every partition gets traversed.
    if header[512:520] == b'EFI PART':return ['-tGPT']
    if header[510:512] == b'\x55\xaa':
        for i in range(4):
            offset=446+i*16
            start,count=struct.unpack_from('<II',header,offset+8)
            if header[offset+4] and start and count and (start+count)*512<=size:return ['-tMBR']
    return []


class Inspector:
    def __init__(self, work):
        self.work = work; work.mkdir()
        self.seven = shutil.which('7zz') or shutil.which('7z')
        if not self.seven:raise ValueError('7-Zip is required')
        self.records = {}; self.paths = {}; self.gaps = []; self.expanded = 0; self.counter = 0
        self.containers = {}; self.inventory = {}; self.covered = {k: [] for k in SYSTEM_AREAS}

    def allocate(self):
        self.counter += 1
        if self.counter > MAX_NODES:raise ValueError('Inspection file-count limit')
        return self.work/f'member-{self.counter:05d}'

    def gap(self, label, reason):
        self.gaps.append({'path': label, 'reason': reason})

    def child(self, data, label, depth, root=False):
        if len(data)+self.expanded > MAX_EXPANDED:raise ValueError('Expansion budget exceeded')
        path = self.allocate(); path.write_bytes(data); self.expanded += len(data)
        self.visit(path, label, depth+1, root)

    def visit(self, path, label, depth=0, root=False):
        try:
            self._visit(path, label, depth, root)
        except (ValueError, OSError, subprocess.TimeoutExpired, zlib.error) as exc:
            # Keep other artifacts inspectable; never publish parser output containing vendor data.
            self.gap(label, type(exc).__name__+' during static inspection')

    def _visit(self, path, label, depth, root):
        if depth > MAX_DEPTH:raise ValueError('Nesting limit')
        if len(self.records) >= MAX_NODES:raise ValueError('Inspection node limit')
        facts = fingerprint_file(path); self.records[label] = facts; self.paths[label] = str(path.resolve())
        with path.open('rb') as stream:data = stream.read(min(facts['bytes'], MAX_METADATA))
        roles = file_roles(label.rsplit('!', 1)[-1], data, root)
        facts.update(metadata(label.rsplit('!', 1)[-1], data, roles))
        if facts['bytes'] > MAX_METADATA and roles:self.gap(label, 'Metadata scan limited to first 128 MiB')
        if data.startswith(b'ZKSWE'):
            if facts['bytes'] > MAX_METADATA:raise ValueError('ZKSWE decode budget exceeded')
            payload, checks = decode(data)
            self.containers[label] = {'format': 'ZKSWE', 'checks': checks}
            self.child(payload, label+'!partition-'+str(checks['partition_index']), depth)
            return
        if data.startswith(b'\x27\x05\x19\x56'):
            if facts['bytes'] > MAX_METADATA or len(data) < 64:raise ValueError('uImage bounds')
            _, hcrc, _, size, load, entry, dcrc, os_id, arch, kind, compression = struct.unpack_from('>7I4B', data)
            header = data[:4]+b'\0'*4+data[8:64]; payload = data[64:64+size]
            if len(payload) != size or zlib.crc32(header) != hcrc or zlib.crc32(payload) != dcrc:
                raise ValueError('uImage CRC or size mismatch')
            self.containers[label] = {'format': 'uImage', 'os': os_id, 'arch': arch, 'type': kind,
                                      'compression': compression, 'load': hex(load), 'entry': hex(entry)}
            if kind in (4, 6):
                lengths = []; cursor = 0
                while cursor+4 <= len(payload):
                    length = struct.unpack_from('>I', payload, cursor)[0]; cursor += 4
                    if not length:break
                    lengths.append(length)
                    if len(lengths) > 32:raise ValueError('uImage component limit')
                else:raise ValueError('uImage missing length terminator')
                for i, length in enumerate(lengths):
                    if cursor+length > len(payload):raise ValueError('uImage component bounds')
                    self.child(payload[cursor:cursor+length], label+f'!component-{i}.bin', depth)
                    cursor = (cursor+length+3)&~3
            else:
                name = {2: 'kernel', 3: 'initramfs', 5: 'firmware'}.get(kind, 'payload')
                self.child(payload, label+'!'+name+'.bin', depth)
            if 64+size < len(data):self.gap(label, 'Trailing uImage data not decoded')
            return
        if data.startswith(b'\xd0\x0d\xfe\xed'):
            info, images, total = dtb_metadata(data); facts['device_tree'] = info
            if not images:self.covered['device-tree'].append(label)
            else:
                self.containers[label] = {'format': 'FIT', 'image_count': len(images), 'signature_verification': 'not performed'}
                for i, fields in enumerate(images.values()):
                    blob = fields.get('data')
                    if blob is None and 'data-size' in fields:
                        def number(key):
                            value = fields[key]
                            if len(value) not in (4,8):raise ValueError('FIT integer size')
                            return int.from_bytes(value, 'big')
                        start = number('data-position') if 'data-position' in fields else ((total+3)&~3)+number('data-offset')
                        size = number('data-size')
                        if start < total or start+size > len(data):raise ValueError('FIT external data bounds')
                        blob = data[start:start+size]
                    if blob is None:self.gap(label, 'FIT payload unavailable');continue
                    self.child(blob, label+f'!fit-image-{i}.bin', depth)
            return
        name = label.rsplit('!', 1)[-1]
        if depth and not container_hint(name, data[:2048]):
            self.mark_leaf(label, facts, data)
            return
        if data.startswith(b'\x7fELF'):
            self.mark_leaf(label, facts, data)
            return
        listing = self.allocate()
        switches=archive_switch(data[:2048],facts['bytes'])
        code = run_to_file([self.seven, 'l', '-slt', '-p-', *switches, str(path)], listing, MAX_LISTING)
        text = listing.read_text(errors='replace'); listing.unlink()
        if code:
            self.mark_leaf(label, facts, data)
            if not facts.get('kernel_versions') and not facts.get('bootloader_versions'):
                self.gap(label, 'Unrecognized or encrypted image/container')
            return
        header_text=text.split('\n----------\n',1)[0]
        offset=re.search(r'^Offset = (\d+)$',header_text,re.M)
        if offset and int(offset.group(1)):
            self.gap(label, 'Archive begins after a prefix; prefix not decoded')
        members = archive_members(text)
        prefix = root_prefix(members)
        archive_type = re.search(r'^Type = ([A-Za-z0-9_-]+)$', text, re.M)
        self.containers[label] = {'format': archive_type.group(1) if archive_type else '7-Zip recognized',
                                  'members': len(members), 'rootfs_prefix': prefix}
        self.inventory[label] = members
        if prefix is not None:self.covered['rootfs'].append(label)
        for member, entry in members.items():
            if entry['kind'] != 'file':continue
            size = entry['bytes']
            if self.expanded+size > MAX_EXPANDED:
                self.gap(label, 'Expansion budget exceeded; remaining members not inspected');break
            child = self.allocate()
            try:
                code = run_to_file([self.seven, 'x', '-so', '-spd', '-p-', *switches, str(path), member], child, max(size, 1))
                if code or child.stat().st_size != size:raise ValueError('Incomplete member extraction')
                self.expanded += size
                in_root = root or prefix is not None and str(PurePosixPath(member)).startswith(prefix)
                self.visit(child, label+'!'+member, depth+1, in_root)
            except (ValueError, OSError, subprocess.TimeoutExpired) as exc:
                self.gap(label+'!'+member, type(exc).__name__+' extracting member')

    def mark_leaf(self, label, facts, data):
        for role in facts['roles']:
            identified = (role == 'rootfs' or role == 'drivers' and (b'vermagic=' in data or '/firmware/' in label or facts.get('builtin_driver_config')) or
                          role == 'kernel' and 'kernel_versions' in facts or
                          role == 'bootloader' and 'bootloader_versions' in facts)
            if identified:self.covered[role].append(label)

    def bundle(self, download):
        provenance = json.loads((download/'provenance.json').read_text())
        for item in provenance['artifacts']:
            name = item['file']
            if not re.fullmatch(r'artifact-\d{3}\.bin', name):raise ValueError('Invalid local artifact name')
            path = download/name
            actual = fingerprint_file(path)
            if actual['sha256'] != item['sha256'] or actual['bytes'] != item['bytes']:
                self.gap(name, 'Downloaded artifact fingerprint mismatch');continue
            self.visit(path, name)
        coverage = {}
        for area, paths in self.covered.items():
            candidates = [name for name,facts in self.records.items() if area in facts.get('roles',[])]
            coverage[area] = {'status': 'inspected' if paths else 'unknown' if self.gaps or candidates else 'not-found',
                              'evidence': sorted(set(paths))[:200], 'count': len(set(paths)),
                              'candidates': candidates[:200]}
        for area, entry in coverage.items():
            if entry['status'] == 'inspected' and self.gaps:entry['status'] = 'partial'
        (self.work.parent/'selected-files.json').write_text(json.dumps(self.paths, indent=2))
        (self.work.parent/'inventory.json').write_text(json.dumps(self.inventory, indent=2))
        return {'records': self.records, 'paths': self.paths, 'containers': self.containers,
                'gaps': self.gaps, 'system_coverage': coverage, 'inventory': self.inventory}
