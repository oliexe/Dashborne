"""Execute the compiled ARM constructor under emulation with mocked file I/O.
No device is accessed. Dependencies and ARM ABI are checked separately.
Run: uv run --with unicorn --with pyelftools python app/sd-probe/verify_probe.py
"""
from pathlib import Path
from io import BytesIO
import struct
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_LR, UC_ARM_REG_SP

blob=(Path(__file__).parent/'card/lib/libsdprobe.so').read_bytes()
elf=ELFFile(BytesIO(blob))
assert elf['e_machine']=='EM_ARM' and elf['e_flags'] & 0x400
needed=[t.needed for t in elf.get_section_by_name('.dynamic').iter_tags() if t.entry.d_tag=='DT_NEEDED']
assert needed==['/res/lib/libzkgui.so']
symbols=elf.get_section_by_name('.dynsym')
imports={s.name for s in symbols.iter_symbols() if s['st_shndx']=='SHN_UNDEF' and s.name}
assert imports=={'open','read','write','close','mkdir','fsync','memcpy'}

def run(valid_card):
    u=Uc(UC_ARCH_ARM,UC_MODE_ARM); base=0x100000
    u.mem_map(base,0x100000);u.mem_map(0x400000,0x10000);u.mem_map(0x500000,0x10000)
    for s in elf.iter_segments():
        if s['p_type']=='PT_LOAD':u.mem_write(base+s['p_vaddr'],s.data())
    hooks={};stub=0x500000
    for name in sorted(imports):hooks[stub]=name;u.mem_write(stub,b'\x1e\xff\x2f\xe1');stub+=4
    addresses={name:addr for addr,name in hooks.items()}
    for secname in ('.rel.dyn','.rel.plt'):
        sec=elf.get_section_by_name(secname)
        if not sec:continue
        for r in sec.iter_relocations():
            addr=base+r['r_offset'];typ=r['r_info_type']
            if typ==23:val=struct.unpack('<I',u.mem_read(addr,4))[0]+base
            elif typ==22:val=addresses[symbols.get_symbol(r['r_info_sym']).name]
            else:raise AssertionError(typ)
            u.mem_write(addr,struct.pack('<I',val))
    files={'/proc/version':b'TEST KERNEL\n','/proc/mounts':b'TEST MOUNT\n'}
    if valid_card:files['/mnt/extsd/DASH_SD_PROBE_V1']=b'DASH_SD_TEST\n'
    fds={};written=set()
    def cstr(addr):
        out=bytearray()
        while (b:=bytes(u.mem_read(addr,1)))!=b'\0':out+=b;addr+=1
        return out.decode()
    def hook(uc,addr,size,ctx):
        if addr==0x500ff0:uc.emu_stop();return
        if addr not in hooks:return
        name=hooks[addr];a,b,c=[uc.reg_read(r) for r in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2)];ret=0
        if name=='open':
            path=cstr(a)
            if b & 1:
                assert path.startswith('/mnt/extsd/DASH_TEST_RESULTS/'),path
                written.add(path)
                if b & 0o1000:files[path]=b''
                files.setdefault(path,b'')
            if path not in files:ret=-1
            else:ret=max([2,*fds])+1;fds[ret]=[path,len(files[path]) if b & 0o2000 else 0]
        elif name=='read':
            path,pos=fds[a];chunk=files[path][pos:pos+c];uc.mem_write(b,chunk);fds[a][1]+=len(chunk);ret=len(chunk)
        elif name=='write':
            path,pos=fds[a];chunk=bytes(uc.mem_read(b,c));files[path]=files[path][:pos]+chunk+files[path][pos+c:];fds[a][1]+=c;ret=c
        elif name=='close':del fds[a]
        elif name=='mkdir':assert cstr(a)=='/mnt/extsd/DASH_TEST_RESULTS/'
        elif name=='fsync':pass
        elif name=='memcpy':uc.mem_write(a,bytes(uc.mem_read(b,c)));ret=a
        else:raise AssertionError(name)
        uc.reg_write(UC_ARM_REG_R0,ret & 0xffffffff)
    u.hook_add(UC_HOOK_CODE,hook)
    entry=struct.unpack('<I',u.mem_read(base+elf.get_section_by_name('.init_array')['sh_addr'],4))[0]
    u.reg_write(UC_ARM_REG_SP,0x40fff0);u.reg_write(UC_ARM_REG_LR,0x500ff0)
    u.emu_start(entry,0x500ff4,count=1000000)
    assert u.reg_read(UC_ARM_REG_SP)==0x40fff0
    assert not fds
    if valid_card:
        assert files['/mnt/extsd/DASH_TEST_RESULTS/SD_CODE_EXECUTED.txt'].startswith(b'SUCCESS:')
        assert files['/mnt/extsd/DASH_TEST_RESULTS/kernel-version.txt']==b'TEST KERNEL\n'
        assert b'Probe complete' in files['/mnt/extsd/DASH_TEST_RESULTS/probe.log']
    else:assert not written
    print('PASS:', 'valid card creates proof and diagnostics; writes restricted to SD' if valid_card else 'absent card token makes no writes')

run(True)
run(False)
print('PASS: ARM hard-float ABI and sole installed-application dependency verified')
