"""Synthetic whole-system packages exercise the actual archive traversal without firmware."""
import gzip
import hashlib
import io
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import zipfile
import zlib

from tools.stock_update import audit, evidence, firmware, system_image as system
from tools.tests.test_stock_preflight import Response


def elf():
    data=bytearray(92);data[:7]=b'\x7fELF\x01\x01\x01'
    struct.pack_into('<H',data,18,40);struct.pack_into('<I',data,32,52)
    struct.pack_into('<HH',data,46,40,1)
    return bytes(data)


def dtb():
    strings=b'compatible\0model\0'
    nodes=struct.pack('>I',1)+b'\0'*4
    for nameoff,value in ((0,b'test,board\0'),(11,b'Test board\0')):
        nodes+=struct.pack('>III',3,len(value),nameoff)+value
        nodes+=b'\0'*((-len(nodes))%4)
    nodes+=struct.pack('>II',2,9)
    return struct.pack('>10I',0xd00dfeed,40+len(nodes)+len(strings),40,40+len(nodes),0,17,16,0,len(strings),len(nodes))+nodes+strings


def fit(payload):
    strings=b'data\0type\0';nodes=b''
    def begin(name):
        nonlocal nodes
        nodes+=struct.pack('>I',1)+name.encode()+b'\0';nodes+=b'\0'*((-len(nodes))%4)
    begin('');begin('images');begin('kernel')
    nodes+=struct.pack('>III',3,len(payload),0)+payload;nodes+=b'\0'*((-len(nodes))%4)
    # Nested hash nodes are metadata, not additional FIT payloads.
    begin('hash-1');nodes+=struct.pack('>I',2)
    nodes+=struct.pack('>IIII',2,2,2,9)
    return struct.pack('>10I',0xd00dfeed,40+len(nodes)+len(strings),40,40+len(nodes),0,17,16,0,len(strings),len(nodes))+nodes+strings


def uimage(payload):
    header=struct.pack('>7I4B32s',0x27051956,0,0,len(payload),0x8000,0x8000,zlib.crc32(payload),5,2,2,1,b'fixture')
    return header[:4]+struct.pack('>I',zlib.crc32(header))+header[8:]+payload


def tar(files, symlink=False):
    out=io.BytesIO()
    with tarfile.open(fileobj=out,mode='w:gz') as archive:
        for name,data in files.items():
            member=tarfile.TarInfo(name);member.size=len(data)
            archive.addfile(member,io.BytesIO(data))
        if symlink:
            member=tarfile.TarInfo('lib/libalias.so');member.type=tarfile.SYMTYPE;member.linkname='libc.so.6'
            archive.addfile(member)
    return out.getvalue()


def bundle(work, blobs):
    download=work/'download';download.mkdir()
    items=[]
    for i,data in enumerate(blobs):
        name=f'artifact-{i:03d}.bin';(download/name).write_bytes(data)
        items.append({'type':i,'file':name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
    (download/'provenance.json').write_text(json.dumps({'artifacts':items}))
    return download


class FetchBundleTests(unittest.TestCase):
    def test_all_types_download_once_and_identity_covers_non_app_changes(self):
        manifest={'version':'6.12.0','files':[{'type':t,'url':f'https://example.com/{t}?token=secret'} for t in (0,1,4)]}
        with tempfile.TemporaryDirectory() as tmp,patch.object(firmware,'request',side_effect=[
                Response(json.dumps(manifest).encode()),Response(b'app'),Response(b'kernel'),Response(b'rootfs')]) as request:
            work=Path(tmp)/'bundle';facts=firmware.fetch_bundle('test-cloud-id',work)
            self.assertEqual(request.call_count,4)
            self.assertEqual([x['type'] for x in facts['artifacts']],[0,1,4])
            self.assertNotIn('token=secret',(work/'provenance.json').read_text())
            self.assertEqual(facts['sha256'],firmware.bundle_digest(list(reversed(facts['artifacts']))))
            facts['artifacts'][1]['sha256']='f'*64
            self.assertNotEqual(facts['sha256'],firmware.bundle_digest(facts['artifacts']))

    def test_failed_secondary_download_cannot_claim_complete_fetch(self):
        manifest={'version':'6.12.0','files':[{'type':i,'url':f'https://example.com/{i}'} for i in (0,4)]}
        with tempfile.TemporaryDirectory() as tmp,patch.object(firmware,'request',side_effect=[
                Response(json.dumps(manifest).encode()),Response(b'app'),Response(b'bad',20)]):
            work=Path(tmp)/'bundle'
            with self.assertRaises(ValueError):firmware.fetch_bundle('test-id',work)
            self.assertFalse((work/'provenance.json').exists())


@unittest.skipUnless(shutil.which('7zz') or shutil.which('7z'),'7-Zip required for archive integration')
class SystemImageTests(unittest.TestCase):
    def test_nested_kernel_bootloader_rootfs_device_tree_and_drivers(self):
        root=tar({'etc/os-release':b'NAME=Test Linux\n',
                  'etc/init.d/S01dash':b'mount /mnt/extsd; startupLibPath=/mnt/extsd/Dashborne/lib/libdashborne.so',
                  'lib/libc.so.6':elf(),'lib/modules/6.1.2/test.ko':elf()+b'\0vermagic=6.1.2 SMP ARMv7\0depends=cfg80211\0',
                  'bin/zkgui':elf()+b'\0EasyUI.cfg\0startupLibPath\0'},symlink=True)
        config=gzip.compress(b'CONFIG_MODULES=y\nCONFIG_MMC=y\nCONFIG_INPUT_EVDEV=y\n')
        kernel=uimage(gzip.compress(b'Linux version 6.1.2 (test)\0IKCFG_ST'+config+b'IKCFG_ED'))
        out=io.BytesIO()
        with zipfile.ZipFile(out,'w') as archive:
            archive.writestr('rootfs.tar.gz',root);archive.writestr('uImage',kernel)
            archive.writestr('u-boot.bin',b'U-Boot 2026.01\0mmcblk\0')
            archive.writestr('board.dtb',dtb())
        with tempfile.TemporaryDirectory() as tmp:
            work=Path(tmp);download=bundle(work,[out.getvalue()])
            result=evidence.collect_bundle(audit.ROOT,download,work/'selected')
            coverage=result['coverage']
            self.assertEqual(coverage['inspection_gaps'],[])
            self.assertEqual({k:v['status'] for k,v in coverage['system'].items()},dict.fromkeys(system.SYSTEM_AREAS,'inspected'))
            self.assertTrue(any('kernel_config' in f for f in result['files'].values()))
            self.assertTrue(any('module_vermagic' in f for f in result['files'].values()))
            self.assertTrue(result['driver_checks'][0]['kernel_release_match'])
            self.assertTrue(any('elf' in f for f in result['files'].values()))
            self.assertTrue(any('startupLibPath' in f.get('integration_references',[]) for f in result['files'].values()))
            self.assertTrue(all(not p.is_symlink() for p in (work/'selected').iterdir()))
            self.assertTrue(all(not p.stat().st_mode&0o111 for p in (work/'selected').iterdir()))
            self.assertIsNone(result['app_matches_current_guard'])

    def test_opaque_artifact_does_not_hide_good_artifacts_or_claim_absence(self):
        with tempfile.TemporaryDirectory() as tmp:
            work=Path(tmp);download=bundle(work,[b'unknown encrypted image',b'Linux version 6.1.2\0'])
            result=system.Inspector(work/'selected').bundle(download)
            self.assertEqual(result['system_coverage']['kernel']['status'],'partial')
            self.assertEqual(result['system_coverage']['rootfs']['status'],'unknown')
            self.assertEqual(len(result['gaps']),1)

    def test_app_only_tree_does_not_count_as_linux_rootfs(self):
        app=tar({'lib/libzkgui.so':elf(),'ui/Main.ftu':b'layout'})
        with tempfile.TemporaryDirectory() as tmp:
            work=Path(tmp);download=bundle(work,[app])
            result=system.Inspector(work/'selected').bundle(download)
            self.assertEqual(result['system_coverage']['rootfs']['status'],'not-found')
            self.assertEqual(result['gaps'],[])

    def test_traversal_archive_is_not_extracted(self):
        out=io.BytesIO()
        with zipfile.ZipFile(out,'w') as archive:archive.writestr('../outside',b'bad')
        with tempfile.TemporaryDirectory() as tmp:
            work=Path(tmp);download=bundle(work,[out.getvalue()])
            result=system.Inspector(work/'selected').bundle(download)
            self.assertTrue(result['gaps']);self.assertFalse((work/'outside').exists())

    @unittest.skipUnless(shutil.which('mke2fs'),'mke2fs required for disk-image fixture')
    def test_mbr_disk_with_ext_rootfs_is_traversed(self):
        with tempfile.TemporaryDirectory() as tmp:
            work=Path(tmp);source=work/'source';(source/'etc').mkdir(parents=True);(source/'lib').mkdir()
            (source/'etc/os-release').write_text('NAME=Fixture Linux\n')
            (source/'lib/libc.so.6').write_bytes(elf())
            fs=work/'rootfs.img'
            subprocess.run([shutil.which('mke2fs'),'-q','-t','ext4','-F','-d',str(source),str(fs),'4096'],
                           check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=30)
            data=fs.read_bytes();header=bytearray(1024**2)
            header[447:450]=bytes((32,33,0));header[451:454]=bytes((162,34,0))
            header[450]=0x83;struct.pack_into('<II',header,454,2048,len(data)//512);header[510:512]=b'\x55\xaa'
            # A second partition proves the disk handler does not stop at the first filesystem.
            header[463:466]=bytes((162,35,0));header[466]=0xda;header[467:470]=bytes((162,35,0))
            struct.pack_into('<II',header,470,10240,1)
            boot=b'U-Boot 2026.01\0'+b'\0'*(512-len(b'U-Boot 2026.01\0'))
            download=bundle(work,[header+data+boot]);result=system.Inspector(work/'selected').bundle(download)
            self.assertEqual(result['gaps'],[])
            self.assertEqual(result['system_coverage']['rootfs']['status'],'inspected')
            self.assertIn('MBR',{c['format'] for c in result['containers'].values()})
            self.assertEqual(result['system_coverage']['bootloader']['status'],'inspected')
            self.assertTrue(any(name.endswith('!lib/libc.so.6') for name in result['records']))

    def test_fit_payload_is_inspected_without_treating_hash_as_image(self):
        with tempfile.TemporaryDirectory() as tmp:
            work=Path(tmp);download=bundle(work,[fit(gzip.compress(b'Linux version 6.9.1\0'))])
            result=system.Inspector(work/'selected').bundle(download)
            self.assertEqual(result['gaps'],[])
            self.assertEqual(result['system_coverage']['kernel']['status'],'inspected')
            self.assertEqual(result['system_coverage']['device-tree']['status'],'unknown')
            self.assertEqual(result['containers']['artifact-000.bin']['image_count'],1)

    def test_driver_kernel_mismatch_is_recorded(self):
        root=tar({'etc/inittab':b'init','lib/modules/test.ko':elf()+b'\0vermagic=5.4.0 SMP\0'})
        with tempfile.TemporaryDirectory() as tmp:
            work=Path(tmp);download=bundle(work,[root,b'Linux version 6.1.0\0'])
            result=evidence.collect_bundle(audit.ROOT,download,work/'selected')
            self.assertFalse(result['driver_checks'][0]['kernel_release_match'])

    def test_corrupt_uimage_and_depth_budget_are_explicit_gaps(self):
        data=bytearray(uimage(b'Linux version 6.1.2\0'));data[-1]^=1
        with tempfile.TemporaryDirectory() as tmp:
            work=Path(tmp);download=bundle(work,[data,tar({'nested.gz':gzip.compress(b'payload')})])
            with patch.object(system,'MAX_DEPTH',0):result=system.Inspector(work/'selected').bundle(download)
            self.assertGreaterEqual(len(result['gaps']),2)
            self.assertEqual(result['system_coverage']['kernel']['status'],'unknown')


class MetadataTests(unittest.TestCase):
    def test_device_tree_properties_and_bounds(self):
        facts,images,_=system.dtb_metadata(dtb())
        self.assertFalse(images);self.assertEqual(facts['properties'][0]['value'],'test,board,')
        with self.assertRaises(ValueError):system.dtb_metadata(dtb()[:-1])

    def test_elf_interpreter_and_program_bounds(self):
        interp=b'/lib/ld-linux-armhf.so.3\0'
        data=bytearray(124)+interp;data[:7]=b'\x7fELF\x01\x01\x01'
        struct.pack_into('<H',data,18,40);struct.pack_into('<II',data,28,52,84)
        struct.pack_into('<HHHH',data,42,32,1,40,1)
        struct.pack_into('<8I',data,52,3,124,0,0,len(interp),len(interp),0,1)
        self.assertEqual(evidence.elf_info(data)['interpreter'],interp[:-1].decode())
        struct.pack_into('<I',data,56,999999)
        with self.assertRaises(ValueError):evidence.elf_info(data)

    def test_root_namespace_and_alias_paths(self):
        self.assertIsNone(system.root_prefix(['lib/libzkgui.so','ui/Main.ftu']))
        self.assertEqual(system.root_prefix(['rootfs/etc/inittab','rootfs/lib/ld-linux.so.3']),'rootfs/')
        for name in ('../escape','/etc/passwd','-o/tmp','a*','a?'):
            with self.assertRaises(ValueError):system.archive_members('x\n----------\nPath = '+name+'\nSize = 1\n')
        listing='x\n----------\nPath = ./foo\nSize = 1\n\nPath = foo\nSize = 1\n'
        with self.assertRaises(ValueError):system.archive_members(listing)


if __name__=='__main__':unittest.main()
