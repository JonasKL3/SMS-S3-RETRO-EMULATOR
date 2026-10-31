import argparse,pathlib,subprocess,os,sys,tempfile
if os.name=='nt':
    import ctypes
    ctypes.windll.kernel32.SetErrorMode(0x8003)
parser=argparse.ArgumentParser(description='SMS-S3 real core and frontend regressions. Zig 0.14.1 required.')
parser.add_argument('--zig',required=True);parser.add_argument('--build')
args=parser.parse_args();root=pathlib.Path(__file__).resolve().parents[1]
work=pathlib.Path(args.build).resolve() if args.build else root/'.test-build'
build=work/'native-sms012';build.mkdir(parents=True,exist_ok=True);zig=pathlib.Path(args.zig).resolve()
env=os.environ.copy();env['ZIG_GLOBAL_CACHE_DIR']=str(work/'zig-cache-global');env['ZIG_LOCAL_CACHE_DIR']=str(work/'zig-cache-local')
def run(command):
    result=subprocess.run([str(x) for x in command],env=env,text=True,capture_output=True,timeout=180)
    if result.stdout:print(result.stdout,flush=True)
    if result.returncode:
        print(result.stderr[:12000],flush=True);raise SystemExit(result.returncode)
flags=['-O2','-g','-DARDUINO_ARCH_ESP32','-fsanitize=undefined','-fno-sanitize-trap=undefined','-fno-sanitize-recover=undefined',
       '-I'+str(root/'tests/host_stubs'),'-I'+str(root),'-L'+str(build)]
report=build/'ubsan_report.o';run([zig,'cc','-c',root/'tests/ubsan_report.c','-o',report])
for lib in ['liblibclang_rt.ubsan_standalone.a','liblibclang_rt.ubsan_standalone_cxx.a']:run([zig,'ar','rcs',build/lib,report])
core=root/'firmware/SMS_S3/src/smsplus';fw=root/'firmware/SMS_S3';objects=[]
for source in sorted(core.rglob('*.c')):
    obj=build/(source.relative_to(core).as_posix().replace('/','_')+'.o')
    run([zig,'cc','-std=gnu11',*flags,'-I'+str(core),'-c',source,'-o',obj]);objects.append(obj)
    print('compiled '+source.name,flush=True)
adapter=build/'adapter.o';run([zig,'c++','-std=gnu++11',*flags,'-c',fw/'sms_core.cpp','-o',adapter]);objects.append(adapter)
# Original, distributable test cartridge: Z80 code enables SMS Mode4, writes
# CRAM color0 red, increments RAM and samples joypad port. NMI increments RAM2.
rom=bytearray(32768)
program=bytes.fromhex('F3 31 F0 DF 3E 04 D3 BF 3E 80 D3 BF 3E 40 D3 BF 3E 81 D3 BF 3E 0E D3 BF 3E 82 D3 BF 3E 7F D3 BF 3E 85 D3 BF 3E 00 D3 BF 3E C0 D3 BF 3E 03 D3 BE 21 00 C0 34 DB DC 32 01 C0 18 F8')
program=program[:-11]+bytes.fromhex('3E 80 D3 7F 3E 10 D3 7F 3E 90 D3 7F')+program[-11:]
rom[:len(program)]=program
rom[0x66:0x70]=bytes.fromhex('E5 21 02 C0 34 E1 ED 45 00 00')
rom[0x7ff0:0x7ff8]=b'TMR SEGA';rom[0x7fff]=0x4c
romPath=build/'sms-s3-homebrew.sms';romPath.write_bytes(rom)
sd=pathlib.Path(tempfile.mkdtemp(prefix='sd-',dir=build));(sd/'JOGOS/SMS/save').mkdir(parents=True)
extra=['-I'+str(root/'tests/state_stubs'),'-DGBC_HOST_SD_ROOT="'+sd.as_posix()+'"','-DSMS_VFS_MOUNT="'+sd.as_posix()+'"']
exe=build/'test_sms_core.exe'
run([zig,'c++','-std=gnu++11',*extra,*flags,root/'tests/test_sms_core.cpp',fw/'sms_savestates.cpp',*objects,'-o',exe]);run([exe,romPath])

exe=build/'test_sms_states.exe';run([zig,'c++','-std=gnu++11',*extra,*flags,root/'tests/test_sms_states.cpp',fw/'sms_savestates.cpp','-o',exe]);run([exe])
exe=build/'test_sms_video.exe';vf=['-I'+str(root/'tests/video_stubs'),*flags]
run([zig,'c++','-std=gnu++11',*vf,root/'tests/test_sms_video.cpp',fw/'sms_video_output.cpp','-o',exe]);run([exe])
exe=build/'test_covers.exe';cf=['-I'+str(root/'tests/cover_stubs'),'-DSMS_HOST_SD_ROOT="'+sd.as_posix()+'"',*extra,*flags]
run([zig,'c++','-std=gnu++11',*cf,root/'tests/test_covers.cpp',fw/'sms_covers.cpp','-o',exe]);run([exe])
meta=[*extra,'-DHALO_LIB_ROOT="'+(sd/'JOGOS/SMS/save').as_posix()+'"',*flags]
if os.name=='nt':meta+=['-Dfsync=_commit','-include','io.h']
exe=build/'test_metadata.exe';run([zig,'c++','-std=gnu++11',*meta,root/'tests/test_metadata.cpp',fw/'halo_library.cpp','-o',exe]);run([exe])
exe=build/'test_z80_flags.exe'
run([zig,'c++','-std=gnu++11',*flags,root/'tests/test_z80_flags.cpp','-o',exe]);run([exe])
exe=build/'test_sms_render_skip.exe'
run([zig,'c++','-std=gnu++11',*extra,*flags,root/'tests/test_sms_render_skip.cpp',*objects,'-o',exe]);run([exe,romPath])
print('SMS-S3 HOST TESTS PASS (real core + UBSan, hardware stubs only; wrapper fault tests isolate core API)',flush=True)
