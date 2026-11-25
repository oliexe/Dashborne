"""Local preflight fixtures never contact a device or service."""
import hashlib
import io
import json
import os
from pathlib import Path
import struct
import subprocess
import shutil
import zipfile
import sys
import tempfile
import unittest
from unittest.mock import patch
import zlib

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from tools.stock_update import audit, evidence, firmware
from tools.stock_update.container import decode


def container():
    payload=bytearray(2048);payload[1080:1082]=b'\x53\xef'
    header=bytearray(0x23c);header[:9]=b'ZKSWEV1.0'
    header[0x10:0x13]=bytes((0x30,1,0x30))
    struct.pack_into('<II',header,0x18,0x23c,len(payload));header[0x20:0x30]=payload[:16]
    struct.pack_into('<I',header,0x30,0x20c)
    struct.pack_into('<I',header,0x238,zlib.crc32(header[:0x238]))
    return bytes(header)+hashlib.md5(payload).digest()+payload[16:]


class Response(io.BytesIO):
    def __init__(self,data,declared=None):
        super().__init__(data);self.headers={'Content-Length':str(len(data) if declared is None else declared)}
    def geturl(self):return 'https://example.com/update.img?secret=private-token'


def report():
    return {'state':{'version':'6.12.0','sha256':'a'*64,'commit':'b'*40},'model':'test-model',
            'component_ids':['boot-menu'],'app_matches_guard':False,'layout_mismatches':2,'layout_checks':14,
            'coverage_gaps':['Base Linux unavailable'],'inspection_error':None,
            'review':{'summary':'Needs review','plan':['Inspect boot ABI'],'unknowns':['Runtime is absent'],
                      'components':[{'id':'boot-menu','status':'unknown','finding':'Missing runtime',
                                     'evidence':['dashborne/loader.c:22'],'fixes':['Capture the new runtime'],
                                     'tests':['Cold boot with no extensions']}]}}


class PreflightTests(unittest.TestCase):
    def test_verified_download_omits_identity_and_signed_url(self):
        manifest={'version':'6.12.0','files':[{'type':0,'url':'https://example.com/image?secret=private-token'}]}
        image=container()
        with tempfile.TemporaryDirectory() as tmp,patch.object(firmware,'request',side_effect=[
                Response(json.dumps(manifest).encode()),Response(image)]):
            out=Path(tmp)/'new';result=firmware.fetch('real-cloud-id-fixture',out)
            self.assertEqual((out/'partition.img').read_bytes(),decode(image)[0])
            self.assertEqual(result['sha256'],hashlib.sha256(image).hexdigest())
            self.assertNotIn('private-token',(out/'provenance.json').read_text())
            self.assertNotIn('real-cloud-id-fixture',(out/'provenance.json').read_text())
            with self.assertRaises(FileExistsError):firmware.fetch('real-cloud-id-fixture',out)

    def test_corrupt_container_and_truncated_download(self):
        data=bytearray(container());data[-1]^=1
        with self.assertRaisesRegex(ValueError,'MD5'):decode(data)
        data=bytearray(container());data[0x40]^=1
        with self.assertRaisesRegex(ValueError,'CRC32'):decode(data)
        manifest={'version':'6.12.0','files':[{'type':0,'url':'https://example.com/image'}]}
        with tempfile.TemporaryDirectory() as tmp,patch.object(firmware,'request',side_effect=[
                Response(json.dumps(manifest).encode()),Response(b'abc',100)]):
            out=Path(tmp)/'new'
            with self.assertRaisesRegex(ValueError,'Incomplete'):firmware.fetch('real-cloud-id-fixture',out)
            self.assertFalse((out/'download.part').exists())
            self.assertFalse((out/'update.img').exists())

    def test_http_schema_and_output_boundaries(self):
        for url in ('http://example.com/file','file:///tmp/image','https://user:pass@example.com/file'):
            with self.assertRaises(ValueError):firmware.https_url(url)
        for manifest in ([],{'version':'../x','files':[]},{'version':'6.12.0','files':[]},
                         {'version':'6.12.0','files':[{'type':0,'url':'https://example.com/a'}]*2}):
            with self.assertRaises(ValueError):firmware.choose_file(manifest,0)
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'.git').mkdir()
            with self.assertRaisesRegex(ValueError,'outside'):firmware.fetch('id',root/'download')

    def test_elf_metadata_rejects_invalid_bounds_and_abi(self):
        self.assertIsNone(evidence.elf_info(b'not ELF'))
        self.assertIn('error',evidence.elf_info(b'\x7fELF'+b'\0'*60))
        data=bytearray(92);data[:7]=b'\x7fELF\x01\x01\x01'
        struct.pack_into('<H',data,18,40);struct.pack_into('<I',data,32,52)
        struct.pack_into('<HH',data,46,40,1)
        self.assertEqual(evidence.elf_info(data)['machine'],40)
        struct.pack_into('<I',data,32,1000000)
        with self.assertRaises(ValueError):evidence.elf_info(data)

    def test_every_manifest_is_included(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);ext=root/'extensions/new-feature';ext.mkdir(parents=True)
            (ext/'manifest.json').write_text(json.dumps({'id':'new-feature'}))
            (ext/'extension.c').write_text('/* fixture */')
            result=evidence.source_components(root)
            self.assertIn('linux-boot',result);self.assertIn('extra-menu',result)
            self.assertEqual(result['new-feature'],['extensions/new-feature/extension.c'])

    def test_missing_or_duplicate_components_and_unsafe_output_rejected(self):
        data=report();audit.validate_review(data['review'],{'boot-menu'})
        with self.assertRaises(ValueError):audit.validate_review(data['review'],{'boot-menu','new-extension'})
        data['review']['components']*=2
        with self.assertRaises(ValueError):audit.validate_review(data['review'],{'boot-menu'})
        data=report();data['review']['summary']='https://example.com?X-Amz-Signature=private'
        with self.assertRaises(ValueError):audit.report_markdown(data)

    def test_report_escapes_mentions_html_and_markdown(self):
        data=report();data['review']['summary']='@everyone <script> ![image](evil)'
        body=audit.report_markdown(data)
        self.assertNotIn('@everyone',body);self.assertNotIn('<script>',body);self.assertNotIn('![image]',body)
        self.assertIn('full Linux boot compatibility',body)
        self.assertIn('a'*64,body)

    def test_local_import_and_unknown_format_prepare_complete_review(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);image=root/'update.bin';image.write_bytes(b'future format')
            work=root/'audit';work.mkdir();audit.import_images(work,[image],'6.12.0')
            with patch.object(audit,'collect_bundle',side_effect=ValueError('Unknown format')):
                audit.inspect(work)
            facts=json.loads((work/'evidence.json').read_text())
            self.assertEqual(facts['inspection_error'],'ValueError')
            self.assertEqual(set(facts['components']),set(evidence.source_components(audit.ROOT)))
            self.assertIsNone(facts['app_matches_current_guard'])
            self.assertTrue((work/'prompt.txt').exists())
            self.assertTrue((work/'inspection.md').exists())
            with self.assertRaises(FileExistsError):audit.import_images(work,[image],'6.12.0')

    def test_baseline_diff_distinguishes_ambiguous_and_rootfs_files(self):
        previous={'files':{'artifact-000.bin!lib/a.so':{'sha256':'a','roles':['rootfs']},
                           'artifact-000.bin!lib/b.so':{'sha256':'b'},
                           'artifact-000.bin!lib/c.so':{'sha256':'c'}}}
        current={'files':{'artifact-003.bin!lib/a.so':{'sha256':'changed','roles':['rootfs']},
                          'artifact-000.bin!lib/b.so':{'sha256':'b'},
                          'artifact-000.bin!lib/d.so':{'sha256':'d'},
                          'artifact-001.bin!lib/d.so':{'sha256':'e'}}}
        diff=audit.compare(current,previous)
        self.assertEqual(diff['changed'],['rootfs:lib/a.so'])
        self.assertEqual(diff['removed'],['package:lib/c.so'])
        self.assertEqual(diff['ambiguous'],['package:lib/d.so'])
        self.assertEqual(diff['unchanged_count'],1)

    def test_work_directory_cannot_be_inside_a_checkout(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'.git').mkdir()
            with self.assertRaises(ValueError):audit.private_work(root/'work')

    def test_local_schema_and_incomplete_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            work=Path(tmp);(work/'state.json').write_text('{}')
            (work/'evidence.json').write_text(json.dumps({'components':{'boot-menu':[]},'coverage':{'unverified':[]}}))
            audit.prepare(work)
            schema=json.loads((work/'review-schema.json').read_text())
            self.assertEqual(schema['properties']['components']['items']['properties']['id']['enum'],['boot-menu'])
            self.assertIn(str(work.resolve()),(work/'prompt.txt').read_text())
            (work/'review.json').write_text('{"summary":"unfinished"}')
            with self.assertRaises(ValueError):audit.finish(work)
            self.assertFalse((work/'report.json').exists())

    @unittest.skipUnless(shutil.which('7zz') or shutil.which('7z'), '7-Zip required')
    def test_cli_import_inspect_baseline_and_report(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            def cli(*args):
                result=subprocess.run([sys.executable,'-m','tools.stock_update.audit',*map(str,args)],
                                      cwd=audit.ROOT,capture_output=True,text=True)
                self.assertEqual(result.returncode,0,result.stderr)
            for label,release in (('baseline','old'),('candidate','new')):
                source=root/(label+'.zip')
                with zipfile.ZipFile(source,'w') as archive:
                    archive.writestr('etc/os-release','VERSION='+release)
                    archive.writestr('lib/runtime.so','synthetic runtime')
                    archive.writestr('other-package/resources/colors.dat',release)
                work=root/label
                cli('import','--version','6.12.0','--image',source,'--work',work)
                args=['inspect','--work',work]
                if label=='candidate':args+=['--baseline',root/'baseline']
                cli(*args)
            facts=json.loads((work/'evidence.json').read_text())
            self.assertIn('rootfs:etc/os-release',facts['baseline_comparison']['changed'])
            self.assertEqual(facts['coverage']['system']['rootfs']['status'],'inspected')
            review={'summary':'Synthetic CLI test only','plan':[],'unknowns':['No real device evidence'],
                    'components':[{'id':key,'status':'unknown','finding':'Synthetic fixture',
                                   'evidence':[],'fixes':[],'tests':[]} for key in facts['components']]}
            audit.save(work/'review.json',review)
            cli('report','--work',work)
            result=json.loads((work/'report.json').read_text())
            self.assertEqual(result['baseline']['changes']['changed'],2)
            self.assertIn('Synthetic CLI test only',(work/'report.md').read_text())

    def test_local_report_keeps_only_curated_evidence(self):
        with tempfile.TemporaryDirectory() as tmp:
            work=Path(tmp);sample=report()
            audit.save(work/'state.json',sample['state'])
            audit.save(work/'evidence.json',{'components':{'boot-menu':[]},
                       'app_matches_current_guard':False,'layout_checks':[{'match':False}],
                       'coverage':{'unverified':['Linux runtime unavailable']},'files':{'private-reference':'omit'}})
            audit.save(work/'review.json',sample['review'])
            audit.finish(work)
            result=json.loads((work/'report.json').read_text())
            self.assertEqual(result['layout_mismatches'],1)
            self.assertTrue((work/'report.md').exists())
            self.assertNotIn('github',(work/'report.md').read_text())
            self.assertNotIn('private-reference',(work/'report.json').read_text())


if __name__=='__main__':unittest.main()
