#!/usr/bin/env python3
"""Synthetic build-orchestration checks; no game files, network or real compiler required."""
from pathlib import Path
import tempfile
source=Path(__file__).resolve().parents[1]/'tools/build_fuc.py'
with tempfile.TemporaryDirectory(prefix='poke-fuc-build-tests-') as temporary:
    import contextlib,hashlib,importlib.util,io,json,os,shutil,struct,sys
    root=Path(temporary)/'checkout';tool=root/'profiles/mhp2g/tools/build_fuc.py'
    tool.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(source,tool)
    spec=importlib.util.spec_from_file_location('fuc_builder',tool);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
    # Synthetic ELF with the four reviewed call-site locations present as NOPs.
    address=0x0884D840;size=0x1600
    ident=b'\x7fELF\x01\x01\x01'+bytes(9)
    h=struct.pack('<16sHHIIIIIHHHHHH',ident,2,8,1,address,52,0,0,52,32,1,0,0,0)
    ph=struct.pack('<IIIIIIII',1,256,address,address,size,size,7,4)
    data=h+ph+bytes(256-len(h)-len(ph))+bytes(size)
    game=root/'mhfuc';game.mkdir(exist_ok=True);elf=game/'EBOOT.ELF';elf.write_bytes(data);(game/'disc.iso').write_bytes(b'synthetic ISO')
    try:m.validate_elf(elf)
    except ValueError:pass
    else:raise AssertionError('unreviewed ELF accepted')
    m.REVIEWED_ELF=hashlib.sha256(data).hexdigest();m.validate_elf(elf)
    # Change one NOP and deliberately update fixture hash: structural validation
    # must still reject the wrong patch instruction, independently of the digest.
    bad=bytearray(data);struct.pack_into('<I',bad,256+(0x0884D868-address),0x0e213a58)
    elf.write_bytes(bad);m.REVIEWED_ELF=m.digest(elf)
    try:m.validate_elf(elf)
    except ValueError:pass
    else:raise AssertionError('non-NOP archive patch accepted')
    elf.write_bytes(data);m.REVIEWED_ELF=m.digest(elf)
    reference=root/'out/mhp2g-renderer';reference.mkdir(parents=True,exist_ok=True)
    (reference/'CMakeCache.txt').write_text(f'CMAKE_HOME_DIRECTORY:INTERNAL={root}\nCMAKE_GENERATOR:INTERNAL=Ninja\nMHP2G_FFMPEG:STRING=OFF\nCMAKE_MAKE_PROGRAM:FILEPATH=/wrong/reference/ninja-locked\n')
    original=root/'profiles/mhp2g/generated';original.mkdir(exist_ok=True);(original/'original.cpp').write_text('original preserved\n')
    (reference/'bin').mkdir(exist_ok=True);(reference/'bin/Poke').write_text('original executable preserved\n')
    for name,body in [('psp_analyze','from pathlib import Path\nPath(sys.argv[2]).write_text("{}")\n'),('psp_recomp','from pathlib import Path\np=Path(sys.argv[3]);p.mkdir(exist_ok=True);(p/"unit.cpp").write_text("// synthetic corpus\\n");(p/"unit.hpp").write_text("// synthetic header\\n")\n')]:
     p=reference/name;p.write_text('#!/usr/bin/env python3\nimport sys\n'+body);p.chmod(0o755)
    fakebin=root/'fake-tools';fakebin.mkdir(exist_ok=True)
    cmake=fakebin/'cmake';cmake.write_text('''#!/usr/bin/env python3
    import json,sys
    from pathlib import Path
    root=Path.cwd()
    with (root/'commands.jsonl').open('a') as out:out.write(json.dumps(sys.argv[1:])+'\\n')
    if '-B' in sys.argv:
     build=Path(sys.argv[sys.argv.index('-B')+1]);assert build.name=='mhfuc'
     assert not any('CMAKE_MAKE_PROGRAM' in a for a in sys.argv)
     assert '-DMHP2G_KTX2=bundled' in sys.argv
     assert '-DMHP2G_GENERATED_DIR='+str(build/'generated') in sys.argv
    if '--build' in sys.argv and 'Poke' in sys.argv:
     build=Path(sys.argv[sys.argv.index('--build')+1]);(build/'bin').mkdir(exist_ok=True);(build/'bin/Poke').write_text('experimental executable')
    '''.replace('\n    ', '\n'));cmake.chmod(0o755)
    os.environ['PATH']=str(fakebin)+os.pathsep+os.environ['PATH']
    sys.argv=[str(tool),'--jobs','4'];assert m.main()==0
    state=root/'out/mhfuc/fuc-build-state.json';before=json.loads(state.read_text())
    assert before['elf_sha256']==m.REVIEWED_ELF
    assert m.main()==0 # same source hashes must skip regeneration
    assert json.loads(state.read_text())==before
    # Source tampering causes a new corpus, preserving the previous experimental source.
    variant=root/'out/mhfuc/generated/unit.cpp';variant.write_text('modified experimental source\n')
    assert m.main()==0
    assert list((root/'out/mhfuc').glob('generated-backup-*'))
    assert (original/'original.cpp').read_text()=='original preserved\n'
    assert (reference/'bin/Poke').read_text()=='original executable preserved\n'
    sys.argv=[str(tool),'--build-dir','out/mhp2g-renderer']
    try:
     with contextlib.redirect_stderr(io.StringIO()):m.main()
    except SystemExit as exc:assert exc.code==2
    else:raise AssertionError('normal build directory accepted')
    print('FUComplete builder checks passed: pinned ELF, NOP validation, isolated output, reuse, regeneration, original preservation.')
