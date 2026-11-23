"""Create guarded scalar edits from a locally supplied FTU reference.

This helper returns patch metadata and the selected old/new scalar values.
It does not store a copy of the reference layout in the patch.
"""
import hashlib
import json
import struct
import zlib


def inspect(blob):
    if len(blob) < 32 or blob[:6] != b'ZKSW\xaa\x55' or blob[-6:] != b'\x55\xaaZKSW':
        raise ValueError('Invalid FTU envelope')
    offset, plain_size, packed = struct.unpack_from('<III', blob, 6)
    if offset < 18 or offset > 256 or offset + 8 + packed - 16 != len(blob) - 6:
        raise ValueError('Invalid FTU lengths')
    plain = zlib.decompress(blob[offset+8:-6])
    if len(plain) != plain_size - 8: raise ValueError('Invalid plain length')
    text = plain.decode('utf-8'); decoder = json.JSONDecoder(); spans = {}

    def whitespace(at):
        while at < len(text) and text[at].isspace(): at += 1
        return at

    def walk(at, path):
        at = whitespace(at)
        if text[at] == '{':
            at = whitespace(at+1); keys = set()
            if text[at] == '}': return at+1
            while True:
                key, end = decoder.raw_decode(text, at)
                if not isinstance(key,str) or key in keys: raise ValueError('Ambiguous object keys')
                keys.add(key); at=whitespace(end)
                if text[at] != ':': raise ValueError('Invalid object')
                at=whitespace(walk(at+1, path+(key,)))
                if text[at] == '}': return at+1
                if text[at] != ',': raise ValueError('Invalid object separator')
                at=whitespace(at+1)
        if text[at] == '[':
            at=whitespace(at+1); index=0
            if text[at] == ']': return at+1
            while True:
                at=whitespace(walk(at,path+(index,)));index+=1
                if text[at] == ']': return at+1
                if text[at] != ',': raise ValueError('Invalid array separator')
                at=whitespace(at+1)
        value,end=decoder.raw_decode(text,at)
        spans[path]=(len(text[:at].encode()),value,text[at:end].encode())
        return end

    if whitespace(walk(0,())) != len(text): raise ValueError('Trailing layout data')
    return plain, spans


def scalar_patch(blob, id, title, description, updates, version='inspected layout'):
    plain, spans = inspect(blob)
    edits=[]
    for path,value in updates:
        at,expected,raw=spans[tuple(path)]
        canonical=json.dumps(expected,ensure_ascii=True,allow_nan=False,separators=(',',':')).encode()
        replacement=json.dumps(value,ensure_ascii=True,allow_nan=False,separators=(',',':')).encode()
        if canonical != raw or len(raw) != len(replacement):
            raise ValueError('This scalar edit requires canonical, equal-length JSON tokens')
        edits.append({'path':list(path),'offset':at,'expect':expected,'value':value})
    return {'id':id,'title':title,'description':description,'target':'/res/ui/dash1act.ftu',
            'base':{'version':version,'sha256':hashlib.sha256(blob).hexdigest(),'size':len(blob),
                    'crc32':f'{zlib.crc32(blob):08x}','plain_size':len(plain),
                    'plain_crc32':f'{zlib.crc32(plain):08x}'},
            'operation':'replace-scalars','edits':edits}
