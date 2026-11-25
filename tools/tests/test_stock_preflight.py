"""Preflight fixtures never contact a device, OpenAI or GitHub."""
import hashlib
import io
import json
import os
from pathlib import Path
import struct
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

    def test_archive_members_reject_traversal_and_skip_links(self):
        listing='archive header\n----------\nPath = lib/app.so\nSize = 12\nMode = -rwxrwxrwx\n\nPath = lib/link\nSize = 5\nSymbolic Link = /tmp/secret\n'
        result=evidence.members(listing)
        self.assertEqual(result['lib/app.so']['bytes'],12);self.assertTrue(result['lib/link']['symlink'])
        for name in ('../escape','/absolute','-switch','file*','file?'):
            with self.assertRaises(ValueError):evidence.members('x\n----------\nPath = '+name+'\nSize = 1\n')
        with self.assertRaises(ValueError):evidence.members(listing+'\nPath = lib/app.so\nSize = 2\n')

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

    def test_dedup_includes_closed_issues_and_ignores_user_markers(self):
        digest='a'*64
        fake={'number':1,'user':{'login':'someone'},'body':audit.marker(digest),'state':'closed'}
        real={'number':2,'user':{'login':'github-actions[bot]'},'body':audit.marker(digest),'state':'closed'}
        with patch.object(audit,'github',return_value=[fake,real]) as call:
            self.assertEqual(audit.find_issue(digest)['number'],2)
            self.assertIn('state=all',call.call_args.args[0])

    def test_missing_or_duplicate_components_and_unsafe_output_rejected(self):
        data=report();audit.validate_review(data['review'],{'boot-menu'})
        with self.assertRaises(ValueError):audit.validate_review(data['review'],{'boot-menu','new-extension'})
        data['review']['components']*=2
        with self.assertRaises(ValueError):audit.validate_review(data['review'],{'boot-menu'})
        data=report();data['review']['summary']='https://example.com?X-Amz-Signature=private'
        with self.assertRaises(ValueError):audit.issue_body(data)

    def test_issue_body_escapes_mentions_html_and_markdown(self):
        data=report();data['review']['summary']='@everyone <script> ![image](evil)'
        body=audit.issue_body(data)
        self.assertNotIn('@everyone',body);self.assertNotIn('<script>',body);self.assertNotIn('![image]',body)
        self.assertIn('full Linux boot compatibility',body)
        self.assertIn(audit.marker('a'*64),body)

    def test_publish_updates_existing_issue_and_never_creates_duplicate(self):
        with tempfile.TemporaryDirectory() as tmp:
            work=Path(tmp);(work/'report.json').write_text(json.dumps(report()))
            with patch.object(audit,'find_issue',return_value={'number':12}),patch.object(audit,'github',return_value={'html_url':'https://github.com/example/repo/issues/12'}) as call:
                audit.publish(work)
                self.assertEqual(call.call_args.args[0],'/issues/12');self.assertEqual(call.call_args.args[2],'PATCH')

    def test_unknown_container_still_prepares_complete_review(self):
        with tempfile.TemporaryDirectory() as tmp:
            work=Path(tmp);(work/'download').mkdir()
            (work/'download/update.img').write_bytes(b'future container format')
            audit.inspect(work);audit.prepare(work)
            facts=json.loads((work/'evidence.json').read_text())
            self.assertEqual(facts['inspection_error'],'ValueError')
            self.assertEqual(set(facts['components']),set(evidence.source_components(audit.ROOT)))
            self.assertIsNone(facts['app_matches_current_guard'])

    def test_codex_schema_and_incomplete_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            work=Path(tmp);(work/'state.json').write_text('{}')
            (work/'evidence.json').write_text(json.dumps({'components':{'boot-menu':[]}}))
            audit.prepare(work)
            schema=json.loads((work/'review-schema.json').read_text())
            self.assertEqual(schema['properties']['components']['items']['properties']['id']['enum'],['boot-menu'])
            self.assertIn(str(work.resolve()),(work/'prompt.txt').read_text())
            (work/'codex-review.json').write_text('{"summary":"unfinished"}')
            with self.assertRaises(ValueError):audit.finish(work)
            self.assertFalse((work/'report.json').exists())

    def test_codex_report_keeps_only_curated_evidence(self):
        with tempfile.TemporaryDirectory() as tmp:
            work=Path(tmp);sample=report()
            audit.save(work/'state.json',sample['state'])
            audit.save(work/'evidence.json',{'components':{'boot-menu':[]},
                       'app_matches_current_guard':False,'layout_checks':[{'match':False}],
                       'coverage':{'unverified':['Linux runtime unavailable']},'files':{'private-reference':'omit'}})
            audit.save(work/'codex-review.json',sample['review'])
            audit.finish(work)
            result=json.loads((work/'report.json').read_text())
            self.assertEqual(result['layout_mismatches'],1)
            self.assertNotIn('private-reference',(work/'report.json').read_text())


if __name__=='__main__':unittest.main()
