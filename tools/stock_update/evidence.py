"""Read-only firmware evidence. Collect hashes, ABI metadata and source integration points."""
import hashlib
import json
from pathlib import Path
import re
import struct
import zlib

MAX_FILE = 32 * 1024 * 1024
CORE = {
    'kernel': ['dashborne/build.sh', 'dashborne/watchdog_linux.c'],
    'bootloader': ['dashborne/install.py'],
    'rootfs': ['dashborne/build.sh', 'dashborne/loader.c', 'dashborne/debug.c'],
    'device-tree': ['dashborne/device.c', 'dashborne/lan.c'],
    'drivers': ['dashborne/device.c', 'dashborne/debug.c', 'dashborne/debug_force.c'],
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
        return {'error':'ELF is not the supported ELF32 little-endian ABI',
                'class':data[4] if len(data)>4 else None,'byte_order':data[5] if len(data)>5 else None}
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
    symbols={};needed=[];interpreter=None
    phoff=words('<I',28)[0];phsize,phcount=words('<HH',42)
    if phcount:
        if phsize!=32 or phcount>4096:raise ValueError('ELF program table')
        for i in range(phcount):
            ph=words('<8I',phoff+i*phsize)
            if ph[0]==3:
                raw=span(ph[1],ph[4])
                if len(raw)>4096 or not raw.endswith(b'\0'):raise ValueError('ELF interpreter')
                interpreter=raw[:-1].decode('ascii',errors='replace')
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
    return {'machine':machine,'bits':32,'flags':hex(flags),'needed':needed,'interpreter':interpreter,'exports':symbols,
            'version_identifiers':sorted({s.decode('ascii') for s in re.findall(rb'(?:GLIBCXX|GLIBC|CXXABI)_[0-9.]{1,30}',data)})[:200]}


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


def collect_bundle(repo,download,work):
    from .system_image import Inspector
    inspection=Inspector(work).bundle(download)
    components=source_components(repo);context,references=source_context(repo,components)
    wanted=set().union(*references.values());files={};exports=set();elf_errors=[]
    for name,facts in inspection['records'].items():
        path=Path(inspection['paths'][name])
        with path.open('rb') as stream:header=stream.read(4)
        if header==b'\x7fELF':
            try:
                if facts['bytes']>MAX_FILE:raise ValueError('ELF scan limit')
                elf=elf_info(path.read_bytes())
            except ValueError:
                elf={'error':'ELF metadata could not be parsed within bounds'}
            symbols=elf.pop('exports',{});exports.update(symbols)
            elf['referenced_exports']={k:v for k,v in symbols.items() if k in wanted}
            facts['elf']=elf
            if 'error' in elf:elf_errors.append(name)
        # Inventory keeps every file; the main evidence focuses on system and app integrations.
        leaf=name.rsplit('!',1)[-1]
        if (facts.get('roles') or 'elf' in facts or facts.get('integration_references') or
                leaf.endswith('.ftu') or name in inspection['containers']):files[name]=facts
    compat=(repo/'dashborne/extensions/stock_compat.h').read_text()
    sizes=re.findall(r'size!=(\d+)u',compat)
    known=re.findall(r'crc==0x([0-9a-fA-F]+)u\)db_stock_cached_version=(\d+)',compat)
    apps={name:facts for name,facts in files.items() if name.rsplit('!',1)[-1].endswith('lib/libzkgui.so')}
    app_checks={name:str(facts['bytes']) in sizes and any(facts['crc32']==h.lower().zfill(8) for h,v in known)
                for name,facts in apps.items()}
    app_match=all(app_checks.values()) if app_checks else None
    layouts=[]
    for patch in sorted((repo/'extensions').glob('*/assets/*.atp')):
        data=patch.read_bytes()
        if len(data)<28 or data[:4]!=b'ATP1':continue
        base=patch.stem.rsplit('-',1)[0]+'.ftu'
        targets={name:facts for name,facts in files.items() if name.rsplit('!',1)[-1].endswith('ui/'+base)}
        expected_size,expected_crc=struct.unpack_from('<II',data,4)
        for name,target in (targets or {base:None}).items():
            layouts.append({'patch':patch.relative_to(repo).as_posix(),'layout':name,
                            'expected_bytes':expected_size,'expected_crc32':f'{expected_crc:08x}',
                            'match':None if target is None else target['bytes']==expected_size and target['crc32']==f'{expected_crc:08x}'})
    checks={key:{'referenced_symbol_count':len(symbols),
                 'found_in_image':sorted(symbols & exports),
                 'not_found_in_image':sorted(symbols-exports)} for key,symbols in references.items()}
    kernel_versions={v for facts in files.values() for v in facts.get('kernel_versions',[])}
    driver_checks=[]
    for name,facts in files.items():
        for vermagic in facts.get('module_vermagic',[]):
            release=vermagic.split()[0] if vermagic.split() else ''
            driver_checks.append({'module':name,'vermagic':vermagic,
                                  'kernel_release_match':release in kernel_versions if kernel_versions else None})
    available_names={Path(name.rsplit('!',1)[-1]).name for name in inspection['records']}
    for entries in inspection.get('inventory',{}).values():
        available_names.update(Path(name).name for name in entries)
    dependency_checks=[{'file':name,'needed':facts['elf'].get('needed',[]),
                        'not_found_by_basename':[n for n in facts['elf'].get('needed',[]) if Path(n).name not in available_names]}
                       for name,facts in files.items() if 'elf' in facts and facts['elf'].get('needed')]
    missing=[area+': '+entry['status'] for area,entry in inspection['system_coverage'].items()
             if entry['status']!='inspected']
    if inspection['gaps']:missing.append('Some package contents could not be decoded completely; see inspection gaps.')
    if elf_errors:missing.append('Some ELF files could not be parsed; target ABI review required.')
    coverage={'system':inspection['system_coverage'],'containers':inspection['containers'],
              'inspection_gaps':inspection['gaps'],'elf_parse_errors':elf_errors,
              'file_count':len(inspection['records']),
              'unverified':missing+['Static evidence does not validate runtime hook semantics, device boot or recovery.',
                                   'No known-working full-system baseline was supplied.']}
    return {'components':components,'source_context':context,'symbol_checks':checks,'files':files,
            'app_matches_current_guard':app_match,'app_fingerprints':app_checks,
            'layout_checks':layouts,'coverage':coverage,'driver_checks':driver_checks,
            'dependency_checks':dependency_checks,
            'file_fingerprints':{name:{k:facts[k] for k in ('sha256','bytes','roles') if k in facts}
                                 for name,facts in inspection['records'].items()},
            'inspection_error':'Incomplete package coverage' if inspection['gaps'] else None}
