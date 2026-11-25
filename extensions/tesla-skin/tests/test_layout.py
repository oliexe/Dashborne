"""Malformed deltas fail closed; authored changes preserve native control identities."""
import ctypes, importlib.util, json, os, subprocess, tempfile, unittest, zlib, struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('prepare',ROOT/'prepare.py');prepare=importlib.util.module_from_spec(spec);spec.loader.exec_module(prepare)
class LayoutTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.tmp=tempfile.TemporaryDirectory();lib=Path(cls.tmp.name)/'layout.so'
  subprocess.run(['clang','-shared','-fPIC','-Wall','-Wextra','-Werror',str(ROOT/'layout_patch.c'),'-lz','-o',str(lib)],check=True)
  cls.lib=ctypes.CDLL(str(lib));cls.fn=cls.lib.tesla_layout;cls.fn.argtypes=[ctypes.c_char_p,ctypes.c_uint,ctypes.c_char_p,ctypes.c_uint,ctypes.POINTER(ctypes.c_void_p),ctypes.POINTER(ctypes.c_uint)]
  cls.free=ctypes.CDLL(None).free;cls.free.argtypes=[ctypes.c_void_p]
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def apply(self,base,patch):
  out=ctypes.c_void_p();n=ctypes.c_uint();ok=self.fn(base,len(base),patch,len(patch),ctypes.byref(out),ctypes.byref(n))
  if not ok:self.assertFalse(out.value);self.assertEqual(n.value,0);return None
  b=ctypes.string_at(out,n.value);self.free(out);return b
 def test_patch_roundtrip_and_array_members(self):
  from ui_codec import encode,decode
  original={'id':0,'window__one':{'id':50,'fontSize':32,'colorTab':{'color0':16777215},'radiobuttons':[{'id':22,'fontSize':30,'text':'Keep this label'}]}}
  changed=prepare.transform(original,'awlsettings','light',set());base=encode(original);patch,_,_=prepare.compile_delta(base,changed)
  self.assertEqual(decode(self.apply(base,patch)),changed)
  self.assertNotIn(b'Keep this label',patch)
  for i in [0,4,8,12,16,20,24,28,len(patch)-1]:
   bad=bytearray(patch);bad[i]^=128;self.assertIsNone(self.apply(base,bytes(bad)))
  for length in [0,4,27,len(patch)-1]:self.assertIsNone(self.apply(base,patch[:length]))
 def test_semantics_and_transparency(self):
  original={'id':1,'visible':False,'touchable':True,'position':{'left':10,'top':20,'width':200,'height':60},'text':'Brake','colorTab':{'color0':0xff0000},'backgroundColor':-1}
  for mode in ['dash','light']:
   changed=prepare.transform(original,'dialog',mode,set());self.assertEqual(original,changed)
 def test_fonts_preserve_native_unit_labels(self):
  nodes={'textview__unit':{'id':1,'caption':'PT_GAUGE4_SUB_VAL_TOP','fontSize':36},'textview__speed':{'id':2,'caption':'PT_GAUGE4_SPEED_VAL','fontSize':160,'position':{}}}
  changed=prepare.transform(nodes,'dash1act','dash',set())
  self.assertNotIn('family',changed['textview__unit'])
  self.assertEqual(changed['textview__speed']['family'],'Inter-Regular')
  preview=prepare.transform({'window__27':{'textview__1':{'id':8,'caption':'TRIP_DIST','fontSize':36}}},'widgetEdit','light',set())
  self.assertEqual(preview['window__27']['textview__1']['fontSize'],36)
  self.assertNotIn('family',preview['window__27']['textview__1'])
  settings=prepare.transform({'id':1,'fontSize':28},'awlsettings','light',set())
  self.assertEqual(settings['family'],'Inter-Regular')
 def test_light_buttons_do_not_keep_black_fill(self):
  changed=prepare.transform({'id':1,'bgColorTab':{'color0':0,'color1':0xffffff}},'awlsettings','light',set())
  self.assertEqual(changed['bgColorTab'],{'color0':0xf1f2f3,'color1':0x171a20})
 def test_primary_action_text_stays_white(self):
  for mode in ['dash','light']:
   changed=prepare.transform({'id':1,'colorTab':{'color0':0xffffff,'color4':0x999999},'picTab':{'pic0':'dash/elements/btn_ok_bg.png'}},'dialog',mode,set())
   self.assertEqual(set(changed['colorTab'].values()),{0xffffff})
 def test_empty_object_override_and_legacy_envelope(self):
  from ui_codec import encode,decode
  base=encode({'position':{}})
  # Two installed 6.11.4 layouts use the older 17-byte envelope header.
  legacy=bytearray(base[:18]+base[19:]);struct.pack_into('<I',legacy,6,17)
  changed={'position':{'left':20,'width':100}}
  patch,_,_=prepare.compile_delta(bytes(legacy),changed)
  self.assertEqual(decode(self.apply(bytes(legacy),patch)),changed)
 def test_theme_overrides_do_not_modify_metadata_objects(self):
  previous=prepare.THEME.get('controls',{})
  try:
   prepare.THEME['controls']={'dialog':{'*':{'fontSize':20}}}
   result=prepare.transform({'id':1,'position':{'left':0,'width':100}},'dialog','dash',set())
   self.assertEqual(result['fontSize'],20)
   self.assertEqual(result['position'],{'left':0,'width':100})
  finally:prepare.THEME['controls']=previous
 def test_packaged_resource_dimensions_and_image_routes(self):
  sizes=json.loads((ROOT/'resource-sizes.json').read_text())
  for name,expected in sizes.items():
   data=(ROOT/'assets/ui'/name).read_bytes()
   self.assertEqual(data[:8],b'\x89PNG\r\n\x1a\n')
   self.assertEqual(list(struct.unpack_from('>II',data,16)),expected,name)
  routes=[n for n in sizes if ('/elements/' in n or '/preview/' in n or n.endswith(('/question_ico.png','/info_ico.png'))) and '/submenu_bar' not in n and '/ps_btn' not in n]
  self.assertEqual(len(routes),len(set(routes)))
 def test_device_captures(self):
  directory=os.environ.get('TESLA_SKIN_CAPTURE')
  if not directory:self.skipTest('Optional ADB layout capture not supplied')
  from ui_codec import decode
  resources=set(json.loads((ROOT/'resource-sizes.json').read_text()))
  for basefile in Path(directory).glob('*.ftu'):
   for mode in ['dash','light']:
    patch=ROOT/'assets'/(basefile.stem+'-'+mode+'.atp')
    if not patch.exists():continue
    original=decode(basefile.read_bytes());expected=prepare.transform(original,basefile.stem,mode,resources)
    result=self.apply(basefile.read_bytes(),patch.read_bytes());self.assertIsNotNone(result,basefile.name)
    self.assertEqual(decode(result),expected)
    self.assertEqual([n['id'] for _,n in prepare.walk(original) if 'id'in n],[n['id'] for _,n in prepare.walk(expected) if 'id'in n])
if __name__=='__main__':unittest.main()
