#!/usr/bin/env python3
"""Build the C updater with pinned, static dependencies using Clang/LLD."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tarfile
import urllib.request

HERE = Path(__file__).resolve().parent
BUILD = HERE.parent / 'build'
DEPS = BUILD / 'updater-deps'
SOURCES = DEPS / 'src'
LOCK = json.loads((HERE / 'dependencies.json').read_text())
JOBS = str(min(os.cpu_count() or 2, 8))


def run(args, cwd=None, env=None):
    subprocess.run([str(a) for a in args], cwd=cwd, env=env, check=True)


def source(name):
    url, digest = LOCK[name]
    archive = DEPS / 'downloads' / name
    archive.parent.mkdir(parents=True, exist_ok=True)
    if not archive.exists():
        print(f'Downloading {name}', flush=True)
        with urllib.request.urlopen(url, timeout=120) as response:
            data = response.read(150 * 1024 * 1024)
        if hashlib.sha256(data).hexdigest() != digest:
            raise ValueError(f'{name}: source checksum mismatch')
        archive.write_bytes(data)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != digest:
        raise ValueError(f'{name}: cached source checksum mismatch')
    target = SOURCES / name
    if not (target / '.source-sha256').exists() or (target / '.source-sha256').read_text() != digest:
        shutil.rmtree(target, ignore_errors=True)
        target.mkdir(parents=True)
        with tarfile.open(archive) as tar:
            for member in tar.getmembers():
                parts = member.name.split('/')[1:]
                if not parts or not '/'.join(parts):
                    continue
                if '..' in parts or member.name.startswith('/') or not (member.isfile() or member.isdir()):
                    # Upstream test symlinks are not build inputs.
                    continue
                member.name = '/'.join(parts)
                tar.extract(member, target)
        (target / '.source-sha256').write_text(digest)
    return target


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--host', action='store_true')
    parser.add_argument('--test', action='store_true')
    args = parser.parse_args()
    mode = 'host' if args.host else 'arm'
    root = DEPS / mode
    prefix = root / 'prefix'
    root.mkdir(parents=True, exist_ok=True)
    cc = shutil.which(os.environ.get('HOST_CC' if args.host else 'DASHBORNE_CC', 'clang'))
    if not cc:
        raise SystemExit('Clang is required')
    cc = str(Path(cc).absolute())
    signature = hashlib.sha256((json.dumps(LOCK, sort_keys=True) + subprocess.check_output([cc, '--version'], text=True) + str(Path(cc).resolve())).encode()).hexdigest()
    stamp = root / 'toolchain.sha256'
    if stamp.exists() and stamp.read_text() != signature:
        shutil.rmtree(root)
        root.mkdir(parents=True)
    stamp.write_text(signature)
    ar = shutil.which('llvm-ar') or str(Path(cc).parent / 'llvm-ar')
    ranlib = shutil.which('llvm-ranlib') or str(Path(cc).parent / 'llvm-ranlib')
    ld = shutil.which(os.environ.get('DASHBORNE_LD', 'ld.lld'))
    compiler_flags = ['-Os', '-ffunction-sections', '-fdata-sections']
    target_flags = []
    toolchain = root / 'toolchain.cmake'
    runtime = None
    if not args.host:
        if not ld:
            raise SystemExit('LLD is required')
        musl = source('musl')
        for name in ('compiler-rt', 'cmake', 'llvm'):
            source(name)
        target_flags = ['--target=armv7-linux-musleabihf', '-march=armv7-a', '-mfpu=vfpv3-d16', '-mfloat-abi=hard', '-marm']
        env = dict(os.environ, CC=shlex.join([cc, *target_flags]), AR=ar, RANLIB=ranlib, CFLAGS='-Os', LDFLAGS=f'--ld-path={ld}', LIBCC='')
        musl_build = root / 'musl'
        musl_build.mkdir(exist_ok=True)
        if not (prefix / 'lib/libc.a').exists():
            run(['sh', musl / 'configure', '--target=arm-linux-musleabihf', f'--prefix={prefix}', '--disable-shared', '--disable-wrapper'], musl_build, env)
            run(['make', '-j' + JOBS, 'install'], musl_build, env)
        target_flags += [f'--sysroot={prefix}']
        toolchain.write_text(f'''set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER "{cc}")
set(CMAKE_ASM_COMPILER "{cc}")
set(CMAKE_C_COMPILER_TARGET armv7-linux-musleabihf)
set(CMAKE_ASM_COMPILER_TARGET armv7-linux-musleabihf)
set(CMAKE_SYSROOT "{prefix}")
set(CMAKE_AR "{ar}")
set(CMAKE_RANLIB "{ranlib}")
set(CMAKE_C_FLAGS_INIT "-march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -marm -Os -ffunction-sections -fdata-sections")
set(CMAKE_ASM_FLAGS_INIT "-march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -marm")
set(CMAKE_EXE_LINKER_FLAGS_INIT "--ld-path={ld} -static")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_FIND_ROOT_PATH "{prefix}")
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
''')
        rt_build = root / 'compiler-rt'
        if not list(rt_build.glob('lib/**/libclang_rt.builtins*.a')):
            run(['cmake', '-S', SOURCES / 'compiler-rt/lib/builtins', '-B', rt_build, '-G', 'Ninja', f'-DCMAKE_TOOLCHAIN_FILE={toolchain}', '-DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON', '-DCOMPILER_RT_BAREMETAL_BUILD=ON', '-DCOMPILER_RT_INCLUDE_TESTS=OFF', '-DCOMPILER_RT_BUILD_STANDALONE_LIBATOMIC=OFF'])
            run(['cmake', '--build', rt_build, '-j', JOBS])
        runtime = next(rt_build.glob('lib/**/libclang_rt.builtins*.a'))
        with toolchain.open('a') as out:
            out.write(f'set(CMAKE_EXE_LINKER_FLAGS_INIT "--ld-path={ld} -static -nostdlib {prefix}/lib/crt1.o {prefix}/lib/crti.o")\n')
            out.write(f'set(CMAKE_C_STANDARD_LIBRARIES "{runtime} -lc {runtime} {prefix}/lib/crtn.o")\n')
    else:
        toolchain.write_text(f'set(CMAKE_C_COMPILER "{cc}")\nset(CMAKE_C_FLAGS_INIT "-Os -ffunction-sections -fdata-sections")\n')

    def cmake(name, options, outputs):
        src = source(name)
        stamp = root / (name + '.stamp')
        signature = hashlib.sha256((json.dumps(LOCK[name]) + str(toolchain.read_text()) + str(options) + (HERE / 'mbedtls_config.h').read_text()).encode()).hexdigest()
        if stamp.exists() and stamp.read_text() == signature and all((prefix / p).exists() for p in outputs):
            return
        b = root / name
        shutil.rmtree(b, ignore_errors=True)
        run(['cmake', '-S', src, '-B', b, '-G', 'Ninja', f'-DCMAKE_TOOLCHAIN_FILE={toolchain}', f'-DCMAKE_INSTALL_PREFIX={prefix}', f'-DCMAKE_PREFIX_PATH={prefix}', '-DCMAKE_INSTALL_LIBDIR=lib', '-DCMAKE_BUILD_TYPE=MinSizeRel', '-DCMAKE_POLICY_VERSION_MINIMUM=3.5', *options])
        run(['cmake', '--build', b, '-j', JOBS])
        run(['cmake', '--install', b])
        stamp.write_text(signature)

    cmake('mbedtls', ['-DENABLE_TESTING=OFF', '-DENABLE_PROGRAMS=OFF', '-DUSE_SHARED_MBEDTLS_LIBRARY=OFF', f'-DMBEDTLS_USER_CONFIG_FILE={HERE / "mbedtls_config.h"}'], ['lib/libmbedtls.a', 'lib/libmbedcrypto.a'])
    cmake('zlib', ['-DZLIB_BUILD_TESTING=OFF', '-DZLIB_BUILD_SHARED=OFF', '-DZLIB_BUILD_STATIC=ON'], ['lib/libz.a'])
    options = ['-DBUILD_CURL_EXE=OFF', '-DBUILD_SHARED_LIBS=OFF', '-DBUILD_STATIC_LIBS=ON', '-DBUILD_TESTING=OFF', '-DCURL_USE_MBEDTLS=ON', '-DCURL_USE_PKGCONFIG=OFF', '-DMBEDTLS_USE_STATIC_LIBS=ON', '-DCURL_USE_OPENSSL=OFF', '-DCURL_USE_LIBPSL=OFF', '-DCURL_USE_LIBSSH2=OFF', '-DCURL_USE_LIBSSH=OFF', '-DCURL_USE_GSSAPI=OFF', '-DUSE_LIBIDN2=OFF', '-DCURL_ZLIB=OFF', '-DCURL_BROTLI=OFF', '-DCURL_ZSTD=OFF', '-DUSE_NGHTTP2=OFF', '-DCURL_USE_LIBRTMP=OFF', '-DCURL_USE_LIBUV=OFF', '-DENABLE_UNIX_SOCKETS=OFF', '-DCURL_DISABLE_INSTALL=OFF', '-DBUILD_LIBCURL_DOCS=OFF', '-DBUILD_MISC_DOCS=OFF', '-DENABLE_CURL_MANUAL=OFF', '-DCURL_CA_BUNDLE=none', '-DCURL_CA_PATH=none']
    for name in ('DICT', 'FILE', 'FTP', 'GOPHER', 'IMAP', 'LDAP', 'LDAPS', 'MQTT', 'POP3', 'RTSP', 'SCP', 'SFTP', 'SMB', 'SMTP', 'TELNET', 'TFTP', 'WEBSOCKETS', 'COOKIES', 'HSTS', 'ALTSVC', 'NETRC'):
        options.append(f'-DCURL_DISABLE_{name}=ON')
    cmake('curl', options, ['lib/libcurl.a'])
    cjson = source('cjson')
    # Include roots in the executable, including the recovery copy outside Dashborne/.
    roots = (HERE / 'roots.pem').read_bytes()
    (root / 'roots.h').write_text('static const unsigned char roots[] =\n' + '\n'.join(json.dumps(line.decode() + '\n') for line in roots.splitlines()) + ';\n')
    sources = [HERE / (name + '.c') for name in ('main', 'util', 'install', 'network')]
    output = BUILD / ('updater-test' if args.host else 'updater-arm-test') if args.test else (BUILD / 'updater-host' if args.host else BUILD / 'card/Dashborne/bin/updater')
    output.parent.mkdir(parents=True, exist_ok=True)
    flags = [*target_flags, *compiler_flags, '-std=c11', '-Wall', '-Wextra', '-Werror', '-DCJSON_NESTING_LIMIT=64', f'-DMBEDTLS_USER_CONFIG_FILE="{HERE / "mbedtls_config.h"}"', f'-DUPDATER_BUILD_DATE="{datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d")}"', '-I' + str(root), '-I' + str(cjson), '-I' + str(prefix / 'include')]
    if args.test:
        flags += ['-DUPDATER_TEST']
    if not args.host:
        flags += [f'--ld-path={ld}', '-static', '-Wl,--gc-sections', '-Wl,-s']
    else:
        flags += ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    libs = [prefix / 'lib' / n for n in ['libcurl.a', 'libmbedtls.a', 'libmbedx509.a', 'libmbedcrypto.a', 'libeverest.a', 'libp256m.a', 'libz.a']]
    # Explicit startup/libc link keeps the target independent of a preinstalled ARM sysroot.
    if not args.host:
        flags += ['-nostdlib']
        sources = [prefix / 'lib/crt1.o', prefix / 'lib/crti.o', *sources]
        libs += [runtime, prefix / 'lib/libc.a', runtime, prefix / 'lib/crtn.o']
    cjson_object = root / 'cjson.o'
    compile_flags = [f for f in flags if not f.startswith('-Wl,') and f not in ('-nostdlib', '-static') and not f.startswith('--ld-path=')]
    run([cc, *compile_flags, '-Wno-deprecated-declarations', '-c', cjson / 'cJSON.c', '-o', cjson_object])
    extra = ['-framework', 'CoreFoundation', '-framework', 'SystemConfiguration'] if args.host and os.uname().sysname == 'Darwin' else []
    run([cc, *flags, *sources, cjson_object, *libs, '-lm', '-pthread', *extra, '-o', output])
    if not args.host and not args.test:
        licenses = BUILD / 'card/Dashborne/licenses/updater'
        shutil.rmtree(licenses, ignore_errors=True)
        licenses.mkdir(parents=True)
        for name, file in [('musl', 'COPYRIGHT'), ('compiler-rt', 'LICENSE.TXT'), ('mbedtls', 'LICENSE'), ('curl', 'COPYING'), ('zlib', 'LICENSE'), ('cjson', 'LICENSE')]:
            shutil.copyfile(SOURCES / name / file, licenses / (name + '-LICENSE'))
        for name in ('MPL-2.0', 'NOTICE'):
            shutil.copyfile(HERE / name, licenses / name)
    print(output)

if __name__ == '__main__':
    main()
