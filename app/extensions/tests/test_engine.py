"""Load real host-built plugins; mock only privileged file-mount operations."""
import ctypes
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class EngineTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name).resolve()
        self.card = self.root/'card'; (self.card/'extensions').mkdir(parents=True)
        self.res = self.root/'res'; self.res.mkdir()
        self.target = self.res/'screen.ftu'; self.target.write_bytes(b'original fixture')
        self.events = self.root/'events'
        stub = self.root/'mount.c'
        stub.write_text('''#include <errno.h>
int stub_mounts, stub_unmounts, stub_fail_mount, stub_fail_unmount;
int dbx_mount(const char*a,const char*b,const char*c,unsigned long flags,const void*d) {
 (void)a;(void)b;(void)c;(void)d;++stub_mounts;
 if(stub_fail_mount && (stub_fail_mount==1 || (flags&32))) {errno=5;return -1;} return 0;
}
int dbx_umount(const char*a,int b) {(void)a;(void)b;++stub_unmounts;
 if(stub_fail_unmount){errno=16;return -1;}return 0;}
''')
        output = self.root/'engine.so'
        command = ['cc','-std=c11','-shared','-fPIC','-Wall','-Wextra','-Werror',
                   '-DDBX_HOST_TEST', '-Dmount=dbx_mount','-Dumount2=dbx_umount',
                   '-DDBX_ROOT='+json.dumps(str(self.card)), '-DDBX_RES='+json.dumps(str(self.res))]
        if sys.platform == 'darwin': command += ['-D__errno_location=__error']
        subprocess.run(command+[str(ROOT/'engine.c'),str(stub),'-o',str(output)],check=True)
        self.lib = ctypes.CDLL(str(output))
        self.logger = ctypes.CFUNCTYPE(None,ctypes.c_char_p)(lambda text: None)
        self.lib.extensions_recover.argtypes = [type(self.logger)]
        self.lib.extension_probe.argtypes = [ctypes.c_char_p]
        self.lib.extensions_start.argtypes = [ctypes.POINTER(ctypes.c_char_p),ctypes.c_uint,
                                             ctypes.c_void_p,ctypes.c_void_p,ctypes.c_char_p,type(self.logger)]
        self.lib.extensions_activity.argtypes=[ctypes.c_char_p]
        self.lib.extensions_activity.restype=ctypes.c_char_p
        self.assertEqual(self.lib.extensions_recover(self.logger),1)

    def tearDown(self):
        self.lib.extensions_stop()
        self.temp.cleanup()

    def plugin(self, id, *, abi=1, declared_id=None, activate_fail=0, cleanup_fail=0,
               overlay=None, target=None, missing_getter=False, activity=None, registered=None):
        folder = self.card/'extensions'/id; folder.mkdir()
        (folder/'asset').write_bytes(b'own theme')
        source = folder/'source.c'
        operation = ''
        if overlay is not None:
            operation = 'if (h->overlay_file(h,'+json.dumps(overlay)+','+json.dumps(str(target or self.target))+')) return -1;'
        if activity is not None:
            operation += 'if(h->set_startup_activity(h,'+json.dumps(activity)+'))return -1;'
        factory = ''
        if registered is not None:
            factory = 'static void *create(void){return (void *)123;}'
            operation += 'if(!h->owns_callback(h,(void *)create))return -1;'
            operation += 'if(h->register_activity(h,'+json.dumps(registered)+',create))return -1;'
        source.write_text('''#include "dashborne_extension.h"
#include <stdio.h>
FACTORY
static void record(const char *s) {FILE*f=fopen(EVENTS,"a");if(f){fputs(s,f);fclose(f);}}
static int activate(const struct dashborne_host*h) {
 if(!h || h->abi_version!=1 || h->struct_size<sizeof(*h) || !h->resolve || !h->official_library || !h->ui_runtime) return -1;
 record("ID+\\n"); OPERATION return ACTIVATE_FAIL;
}
static int deactivate(void){record("ID-\\n");return CLEANUP_FAIL;}
DASHBORNE_EXTENSION_EXPORT const struct dashborne_extension *GETTER(void) {
 static const struct dashborne_extension d={ABI,sizeof(d),DECLARED_ID,activate,deactivate};return &d;
}
'''.replace('EVENTS',json.dumps(str(self.events))).replace('ID+',id+'+').replace('ID-',id+'-')
            .replace('FACTORY',factory).replace('OPERATION',operation).replace('ACTIVATE_FAIL',str(activate_fail))
            .replace('CLEANUP_FAIL',str(cleanup_fail)).replace('DECLARED_ID',json.dumps(declared_id or id))
            .replace('ABI',str(abi)).replace('GETTER','different_getter' if missing_getter else 'dashborne_extension_v1'))
        subprocess.run(['cc','-shared','-fPIC','-Wall','-Wextra','-Werror','-I',str(ROOT),str(source),'-o',str(folder/'libextension.so')],check=True)
        return folder

    def start(self,*ids,protected=None,runtime=2):
        array=(ctypes.c_char_p*len(ids))(*(id.encode() for id in ids))
        return self.lib.extensions_start(array,len(ids),1,runtime,str(protected).encode() if protected else None,self.logger)

    def value(self,key,value=None):
        v=ctypes.c_int.in_dll(self.lib,key)
        if value is not None:v.value=value
        return v.value

    def log(self): return self.events.read_text().splitlines() if self.events.exists() else []

    def test_activation_order_reverse_cleanup_and_reactivation(self):
        self.plugin('one');self.plugin('two')
        self.assertEqual(self.start('one','two'),2)
        self.assertEqual(self.log(),['one+','two+'])
        self.assertEqual(self.lib.extensions_stop(),1)
        self.assertEqual(self.log(),['one+','two+','two-','one-'])
        self.assertEqual(self.start('one'),1)
        self.assertEqual(self.lib.extensions_stop(),1)

    def test_custom_activity_selection_and_collision_rollback(self):
        self.plugin('one',activity='customActivity');self.plugin('two',activity='anotherActivity')
        self.assertEqual(self.start('one'),1)
        self.assertEqual(self.lib.extensions_activity(b'officialActivity'),b'customActivity')
        self.lib.extensions_stop()
        self.assertEqual(self.lib.extensions_activity(b'officialActivity'),b'officialActivity')
        self.assertEqual(self.start('one','two'),-1)
        self.assertEqual(self.lib.extensions_activity(b'officialActivity'),b'officialActivity')

    def fake_runtime(self):
        source=self.root/'runtime.c'
        source.write_text('''static void *(*factory)(void); int registrations;
void *_ZN15ActivityFactory11getInstanceEv(void){return (void *)1;}
int _ZN15ActivityFactory16registerActivityEPKcPFP8ActivityvE(void*s,const char*n,void*(*f)(void)){
 (void)s;(void)n;factory=f;++registrations;return 1;
}
void *create_last(void){return factory?factory():0;}
''')
        output=self.root/'runtime.so'
        subprocess.run(['cc','-shared','-fPIC',str(source),'-o',str(output)],check=True)
        runtime=ctypes.CDLL(str(output));runtime.create_last.restype=ctypes.c_void_p
        return runtime

    def test_activity_factory_survives_plugin_unload_and_reactivation(self):
        runtime=self.fake_runtime()
        self.plugin('one',registered='dashborneExtension_test')
        self.assertEqual(self.start('one',runtime=runtime._handle),1)
        self.assertEqual(runtime.create_last(),123)
        self.assertEqual(self.lib.extensions_stop(),1)
        self.assertIsNone(runtime.create_last())
        self.assertEqual(self.start('one',runtime=runtime._handle),1)
        self.assertEqual(runtime.create_last(),123)
        self.assertEqual(ctypes.c_int.in_dll(runtime,'registrations').value,1)
        self.assertEqual(self.lib.extensions_stop(),1)
        self.assertIsNone(runtime.create_last())

    def test_activity_collision_and_partial_activation_retire_callbacks(self):
        runtime=self.fake_runtime()
        self.plugin('one',registered='dashborneExtension_test')
        self.plugin('two',registered='dashborneExtension_test')
        self.assertEqual(self.start('one','two',runtime=runtime._handle),-1)
        self.assertIsNone(runtime.create_last())
        self.plugin('three',registered='dashborneExtension_failure',activate_fail=1)
        self.assertEqual(self.start('three',runtime=runtime._handle),-1)
        self.assertIsNone(runtime.create_last())

    def test_failed_activation_rolls_back_partial_module_and_previous_modules(self):
        self.plugin('one',overlay='asset');self.plugin('two',activate_fail=1)
        self.assertEqual(self.start('one','two'),-1)
        self.assertEqual(self.log(),['one+','two+','two-','one-'])
        self.assertEqual(self.value('stub_unmounts'),1)
        self.assertEqual((self.card/'extensions.mounts').read_text(),'')
        self.assertEqual(self.target.read_bytes(),b'original fixture')

    def test_abi_id_and_entrypoint_rejection(self):
        for id,options in [('wrong-abi',{'abi':2}),('wrong-id',{'declared_id':'other'}),('missing',{'missing_getter':True})]:
            self.plugin(id,**options)
            self.assertEqual(self.start(id),-1)
        self.assertEqual(self.log(),[])
        self.assertEqual(self.start('../escape'),-1)
        self.assertEqual(self.lib.extension_probe(b'not-installed'),0)

    def test_read_only_overlay_and_recovery_journal(self):
        self.plugin('one',overlay='asset')
        self.assertEqual(self.start('one'),1)
        self.assertEqual(self.value('stub_mounts'),2)
        self.assertEqual((self.card/'extensions.mounts').read_text(),str(self.target)+'\n')
        self.assertEqual(self.lib.extensions_stop(),1)
        (self.card/'extensions.mounts').write_text(str(self.target)+'\n')
        self.assertEqual(self.lib.extensions_recover(self.logger),1)
        self.assertEqual(self.value('stub_unmounts'),2)

    def test_overlay_paths_duplicates_and_layout_conflicts(self):
        self.plugin('one',overlay='asset');self.plugin('two',overlay='asset')
        self.assertEqual(self.start('one',protected=self.target),-1)
        self.assertEqual(self.value('stub_mounts'),0)
        self.assertEqual(self.start('one','two'),-1)
        self.assertEqual(self.value('stub_mounts'),2)
        self.plugin('escape',overlay='../one/asset')
        self.assertEqual(self.start('escape'),-1)
        folder=self.plugin('symlink',overlay='outside')
        (folder/'outside').symlink_to(self.target)
        self.assertEqual(self.start('symlink'),-1)
        self.plugin('bad-target',overlay='asset',target=self.root/'events')
        self.assertEqual(self.start('bad-target'),-1)
        self.assertEqual(self.start('one','one'),-1)

    def test_failed_mount_and_read_only_remount_rollback(self):
        self.plugin('one',overlay='asset')
        self.value('stub_fail_mount',1);self.assertEqual(self.start('one'),-1)
        self.assertEqual((self.card/'extensions.mounts').read_text(),'')
        self.value('stub_fail_mount',2);self.assertEqual(self.start('one'),-1)
        self.assertEqual(self.value('stub_unmounts'),1)
        self.value('stub_fail_mount',0);self.assertEqual(self.start('one'),1)

    def test_cleanup_failure_blocks_reactivation(self):
        self.plugin('one',cleanup_fail=1)
        self.assertEqual(self.start('one'),1)
        self.assertEqual(self.lib.extensions_stop(),0)
        self.assertEqual(self.start('one'),-2)

    def test_busy_mount_keeps_journal_and_blocks_reactivation(self):
        self.plugin('one',overlay='asset')
        self.assertEqual(self.start('one'),1)
        self.value('stub_fail_unmount',1)
        self.assertEqual(self.lib.extensions_stop(),0)
        self.assertEqual((self.card/'extensions.mounts').read_text(),str(self.target)+'\n')
        self.assertEqual(self.start('one'),-2)
        self.value('stub_fail_unmount',0)

    def test_invalid_recovery_journal_does_not_unmount_arbitrary_paths(self):
        (self.card/'extensions.mounts').write_text('/etc/passwd\n')
        self.assertEqual(self.lib.extensions_recover(self.logger),0)
        self.assertEqual(self.value('stub_unmounts'),0)
        self.assertEqual(self.start(),-2)


if __name__=='__main__':unittest.main()
