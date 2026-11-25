import importlib.util
from pathlib import Path
import unittest
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('panel',root/'generate.py');panel=importlib.util.module_from_spec(spec);spec.loader.exec_module(panel)
class Panel(unittest.TestCase):
 def test_assets_and_geometry(self):
  doc=panel.screen();self.assertLessEqual(doc['backgroundColor'],2147483647);self.assertEqual(panel.decode((root/'assets/panel.ftu').read_bytes()),doc)
  ids=[]
  def walk(node,w=1920,h=480):
   for key,value in node.items():
    if not isinstance(value,dict) or '__' not in key:continue
    ids.append(value['id']);p=value['position']
    if 'backgroundColor' in value:self.assertLessEqual(value['backgroundColor'],2147483647)
    self.assertGreaterEqual(p['left'],0);self.assertGreaterEqual(p['top'],0)
    self.assertLessEqual(p['left']+p['width'],w);self.assertLessEqual(p['top']+p['height'],h)
    walk(value,p['width'],p['height'])
  walk(doc);self.assertEqual(len(ids),len(set(ids)))
  for name in ('day','night'):
   self.assertEqual(doc['window__'+name]['seekbar__brightness']['max'],254)
   self.assertEqual(doc['window__'+name]['seekbar__black']['max'],100)
if __name__=='__main__':unittest.main()
