"""Read-only firmware evidence. Collect hashes, ABI metadata and source integration points."""
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import struct
import subprocess
import zlib

MAX_FILE = 32 * 1024 * 1024
MAX_TOTAL = 256 * 1024 * 1024
CORE = {
    'linux-boot': ['dashborne/build.sh', 'dashborne/install.py', 'dashborne/watchdog_linux.c'],
    'boot-menu': ['dashborne/loader.c', 'dashborne/state.c', 'dashborne/device.c'],
    'extension-engine': ['dashborne/extensions/engine.c', 'dashborne/extensions/stock_compat.h'],
    'adb': ['dashborne/debug.c', 'dashborne/lan.c'],
    'force-adb': ['dashborne/debug_force.c', 'dashborne/watchdog.c'],
    'extra-menu': ['dashborne/core/extra-menu/menu.c', 'dashborne/core/extra-menu/window.c'],
    'widget-runtime': ['dashborne/core/widget-runtime/' + n + '.c' for n in ('runtime','views','catalog','registry','storage')],
}


def fingerprint(data):
    return {'bytes':len(data), 'sha256':hashlib.sha256(data).hexdigest(), 'crc32':f'{zlib.crc32(data):08x}'}


def elf_info(data):
    """Bounds-checked ELF32 little-endian metadata; reject other ABIs explicitly."""
    if data[:4] != b'\x7fELF':
        return None
    if len(data)<52 or data[4:7] != b'\x01\x01\x01':
        return {'error':'ELF is not the supported ELF32 little-endian ABI'}
    def span(offset,size):
        if offset<0 or size<0 or offset+size>len(data):raise ValueError('ELF bounds')
        return data[offset:offset+size]
    def words(fmt,offset):return struct.unpack(fmt,span(offset,struct.calcsize(fmt)))
    def string(table,offset):
        if offset>=len(table):raise ValueError('ELF string bounds')
        end=table.find(b'\0',offset)
        if end<0:raise ValueError('Unterminated ELF string')
        return table[offset:end].decode('ascii',errors='replace')
    machine=words('<H',18)[0];flags=words('<I',36)[0]
    shoff=words('<I',32)[0];shsize,count=words('<HH',46)
    if shsize!=40 or not 0<count<8192:raise ValueError('Unsupported ELF section table')
    sections=[words('<10I',shoff+i*shsize) for i in range(count)]
    symbols={};needed=[]
    for s in sections:
        kind,offset,size,link,entry=s[1],s[4],s[5],s[6],s[9]
        if kind not in (6,11):continue
        if link>=count:raise ValueError('ELF section link')
        linked=sections[link];strings=span(linked[4],linked[5]);blob=span(offset,size)
        if kind==11:
            if entry!=16 or size%entry:raise ValueError('ELF symbol size')
            for i in range(0,size,entry):
                name,value,length,info,other,index=struct.unpack_from('<IIIBBH',blob,i)
                if index and info>>4 in (1,2):
                    label=string(strings,name)
                    symbols[label]={'address':hex(value),'size':length}
        else:
            if entry!=8 or size%entry:raise ValueError('ELF dynamic size')
            for i in range(0,size,entry):
                tag,value=struct.unpack_from('<II',blob,i)
                if tag==1:needed.append(string(strings,value))
    return {'machine':machine,'bits':32,'flags':hex(flags),'needed':needed,'exports':symbols}


def members(listing):
    if '\n----------\n' not in listing:raise ValueError('Unrecognized archive listing')
    result={}
    for block in listing.split('\n----------\n',1)[1].strip().split('\n\n'):
        fields=dict(line.split(' = ',1) for line in block.splitlines() if ' = ' in line)
        name=fields.get('Path','');path=PurePosixPath(name)
        if not name or path.is_absolute() or '..' in path.parts or not re.fullmatch(r'[A-Za-z0-9_./+ -]+',name) or name.startswith('-'):
            raise ValueError('Unsupported archive member path')
        if name in result:raise ValueError('Duplicate archive member')
        if fields.get('Folder')=='+':continue
        if fields.get('Symbolic Link') or fields.get('Mode','').startswith('l'):
            result[name]={'symlink':True};continue
        size=int(fields.get('Size','-1'))
        if not 0<=size<=MAX_TOTAL:raise ValueError('Invalid archive file size')
        result[name]={'bytes':size}
    if len(result)>20000 or sum(v.get('bytes',0) for v in result.values())>MAX_TOTAL:
        raise ValueError('Archive inventory exceeds audit budget')
    return result


def extract_candidates(image, work):
    seven=shutil.which('7zz') or shutil.which('7z')
    if not seven:raise ValueError('7-Zip is required')
    proc=subprocess.run([seven,'l','-slt',str(image)],capture_output=True,timeout=60)
    if proc.returncode or len(proc.stdout)>8*1024*1024:raise ValueError('Filesystem listing failed')
    inventory=members(proc.stdout.decode('utf-8'))
    # Stream individual regular files into flat owned paths; never recreate vendor paths or links.
    extracted={};total=0
    for name,info in sorted(inventory.items()):
        wanted=('.so' in PurePosixPath(name).name or name.endswith('.ftu') or
                PurePosixPath(name).name in ('zkgui','adbd','init.rc','build.prop','EasyUI.cfg','fstab'))
        if not wanted or info.get('symlink'):continue
        size=info['bytes'];total+=size
        if size>MAX_FILE or total>MAX_TOTAL:raise ValueError('Selected files exceed audit budget')
        output=work/('member-'+str(len(extracted)))
        with output.open('xb') as stream:
            proc=subprocess.run([seven,'x','-so',str(image),name],stdout=stream,stderr=subprocess.DEVNULL,timeout=30)
        if proc.returncode or output.stat().st_size!=size:raise ValueError('Filesystem member read failed')
        extracted[name]=output
    return inventory,extracted


def source_components(repo):
    result={key:list(value) for key,value in CORE.items()}
    for p in sorted((repo/'extensions').glob('*/manifest.json')):
        item=json.loads(p.read_text());key=item['id']
        if not re.fullmatch(r'[a-z0-9_-]{1,31}',key) or key in result:raise ValueError('Invalid extension ID')
        # Include headers: offset and ABI tables frequently live outside manifest sources.
        result[key]=[f.relative_to(repo).as_posix() for f in sorted(p.parent.rglob('*'))
                     if f.suffix in ('.c','.h','.py') and 'assets' not in f.parts and 'tests' not in f.parts]
    return result


def source_context(repo,components):
    context={};references={};total=0
    relevant=re.compile(r'dlsym|BIND|RESOLVE|SYMBOL|0x[0-9a-fA-F]{4,}|DB_STOCK|db_stock|compatible|RUNTIME_|lib.*\.so|startupLibPath|/dev/|/etc/|/mnt/|/proc/')
    for component,paths in components.items():
        lines=[];symbols=set()
        for name in paths:
            data=(repo/name).read_text();rows=data.splitlines()
            symbols.update(re.findall(r'"(_Z[A-Za-z0-9_]+)"',data))
            indexes=set()
            for i,row in enumerate(rows):
                if relevant.search(row):indexes.update(range(max(0,i-1),min(len(rows),i+2)))
            lines.extend(f'{name}:{i+1}: {rows[i]}' for i in sorted(indexes))
        full='\n'.join(lines);context[component]={'text':full[:18000],'truncated':len(full)>18000}
        references[component]=symbols;total+=len(context[component]['text'])
    if total>400000:raise ValueError('Source context exceeds review budget')
    return context,references


def collect(repo,image,work):
    catalog,paths=extract_candidates(image,work)
    (work.parent/'selected-files.json').write_text(json.dumps({name:str(path.resolve()) for name,path in paths.items()}))
    components=source_components(repo);context,references=source_context(repo,components)
    wanted=set().union(*references.values());files={};exports=set();elf_errors=[]
    for name,path in paths.items():
        data=path.read_bytes();facts=fingerprint(data)
        try:elf=elf_info(data)
        except ValueError:
            elf={'error':'ELF metadata could not be parsed'}
        if elf:
            symbols=elf.pop('exports',{});exports.update(symbols)
            elf['referenced_exports']={k:v for k,v in symbols.items() if k in wanted}
            facts['elf']=elf
            if 'error' in elf:elf_errors.append(name)
        files[name]=facts
    app=next((files[p] for p in ('lib/libzkgui.so','res/lib/libzkgui.so') if p in files),None)
    compat=(repo/'dashborne/extensions/stock_compat.h').read_text()
    sizes=re.findall(r'size!=(\d+)u',compat)
    known=re.findall(r'crc==0x([0-9a-fA-F]+)u\)db_stock_cached_version=(\d+)',compat)
    app_match=bool(app and str(app['bytes']) in sizes and any(app['crc32']==h.lower().zfill(8) for h,v in known))
    # ATP headers carry the expected original layout length and CRC32; never send the layout itself.
    layouts=[]
    for patch in sorted((repo/'extensions').glob('*/assets/*.atp')):
        data=patch.read_bytes()
        if len(data)<28 or data[:4]!=b'ATP1':continue
        base=patch.stem.rsplit('-',1)[0]+'.ftu'
        target=next((facts for name,facts in files.items() if name in ('ui/'+base,'res/ui/'+base)),None)
        expected_size,expected_crc=struct.unpack_from('<II',data,4)
        layouts.append({'patch':patch.relative_to(repo).as_posix(),'layout':base,
                        'expected_bytes':expected_size,'expected_crc32':f'{expected_crc:08x}',
                        'match':None if target is None else target['bytes']==expected_size and target['crc32']==f'{expected_crc:08x}'})
    checks={key:{'referenced_symbol_count':len(symbols),
                 'found_in_image':sorted(symbols & exports),
                 'not_found_in_image':sorted(symbols-exports)} for key,symbols in references.items()}
    # An application partition's lib/ is mounted under /res/lib/, not the rootfs /lib/.
    coverage={'image_kind':'verified single-partition display update',
              'partition_members':len(catalog),'file_types_inspected':['ELF metadata','layout fingerprints'],
              'base_linux':'not established by this application partition',
              'unverified':['kernel/bootloader/device tree','rootfs dynamic linker and libc',
                            '/bin/zkgui SD startup discovery','/lib/libeasyui.so boot-menu runtime ABI',
                            'init/mount/ADB services','display/touch/Wi-Fi drivers','runtime hook offsets and signatures',
                            'device tests and recovery'], 'elf_parse_errors':elf_errors}
    return {'components':components,'source_context':context,'symbol_checks':checks,'files':files,
            'app_matches_current_guard':app_match,'app_fingerprint':app,'layout_checks':layouts,'coverage':coverage}
