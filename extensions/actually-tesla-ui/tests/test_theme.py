import ctypes, importlib.util, json, os, struct, subprocess, tempfile, unittest, zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('prepare',ROOT/'prepare.py');prepare=importlib.util.module_from_spec(spec);spec.loader.exec_module(prepare)

class ThemeTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.temp=tempfile.TemporaryDirectory();cls.lib=Path(cls.temp.name)/'patch.so'
  subprocess.run(['cc','-shared','-fPIC',str(ROOT/'layout_patch.c'),'-lz','-o',str(cls.lib)],check=True)
  cls.dll=ctypes.CDLL(str(cls.lib));cls.dll.tesla_layout.argtypes=[ctypes.c_void_p,ctypes.c_uint,ctypes.c_void_p,ctypes.c_uint,ctypes.POINTER(ctypes.c_void_p),ctypes.POINTER(ctypes.c_uint)]
  cls.libc=ctypes.CDLL(None);cls.libc.free.argtypes=[ctypes.c_void_p]
 @classmethod
 def tearDownClass(cls):cls.temp.cleanup()
 def apply(self,b,p):
  out=ctypes.c_void_p();n=ctypes.c_uint();ok=self.dll.tesla_layout(b,len(b),p,len(p),ctypes.byref(out),ctypes.byref(n))
  data=ctypes.string_at(out,n.value) if ok else None
  self.libc.free(out);return data
 def fixture(self):
  return {'id':0,'window__1':{'id':1,'caption':'PM_GW1x1_11','visible':False,'textview__2':{'id':500,'caption':'PM_Value1x1_11','fontSize':46,'text':'123','position':{'left':0,'top':52,'width':250,'height':58}}}}
 def blob(self,layout):
  raw=json.dumps(layout,separators=(',',':')).encode();z=zlib.compress(raw)
  return b'ZKSW\xaa\x55'+struct.pack('<III',18,len(raw)+8,len(z)+16)+bytes(8)+z+b'\x55\xaaZKSW'
 def test_layout_round_trip_preserves_live_bindings(self):
  b=self.blob(self.fixture());p,expected,n=prepare.compile_delta(b)
  self.assertGreater(n,0);result=prepare.decode(self.apply(b,p));self.assertEqual(result,expected)
  self.assertEqual(result['window__1']['textview__2']['id'],500)
  self.assertEqual(result['window__1']['textview__2']['text'],'123')
  self.assertFalse(result['window__1']['visible'])
 def test_every_truncation_rejected(self):
  b=self.blob(self.fixture());p,_,_=prepare.compile_delta(b)
  for i in range(len(p)):self.assertIsNone(self.apply(b,p[:i]))
  for i in range(len(b)):self.assertIsNone(self.apply(b[:i],p))
 def test_wrong_reference_and_payload_rejected(self):
  b=self.blob(self.fixture());p,_,_=prepare.compile_delta(b)
  bad=bytearray(p);bad[-1]^=1;self.assertIsNone(self.apply(b,bytes(bad)))
  bad=bytearray(b);bad[20]^=1;self.assertIsNone(self.apply(bytes(bad),p))
 def test_bounds_with_valid_crc(self):
  b=self.blob(self.fixture());p,_,_=prepare.compile_delta(b)
  for off in (28,32,36):
   bad=bytearray(p);struct.pack_into('<I',bad,off,0xffffffff);struct.pack_into('<I',bad,24,zlib.crc32(bad[28:]))
   self.assertIsNone(self.apply(b,bytes(bad)))
 def test_insert_property(self):
  f=self.fixture();f['window__1']['textview__2']['caption']='PM_GAUGE4_SPEED_VAL'
  b=self.blob(f);p,expected,_=prepare.compile_delta(b)
  self.assertEqual(prepare.decode(self.apply(b,p)),expected)
 def test_asset_coverage(self):
  sizes=json.loads((ROOT/'resource-sizes.json').read_text())
  self.assertGreater(len(sizes),250)
  for path,(w,h) in sizes.items():
   data=(ROOT/'assets/ui'/path).read_bytes();self.assertEqual(data[:8],b'\x89PNG\r\n\x1a\n')
   self.assertEqual(struct.unpack_from('>II',data,16),(w,h))
 def test_local_reference(self):
  path=os.environ.get('DASHBORNE_LAYOUT_REFERENCE')
  if not path:self.skipTest('Optional local reference')
  b=Path(path).read_bytes();p,expected,_=prepare.compile_delta(b)
  self.assertEqual(p,(ROOT/'assets/layout.atp').read_bytes())
  self.assertEqual(prepare.decode(self.apply(b,p)),expected)
  def identities(node):
   result=[]
   if isinstance(node,dict):
    if 'id' in node:result.append((node['id'],node.get('caption'),node.get('visible'),node.get('touchable')))
    for v in node.values():result+=identities(v)
   return result
  self.assertEqual(identities(prepare.decode(b)),identities(expected))

if __name__=='__main__':unittest.main()
