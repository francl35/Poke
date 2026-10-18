#!/usr/bin/env python3
"""Check isolated normal-launch paths and environment, without launching a game."""
import importlib.util
import os
from pathlib import Path
import shutil
import sys
import tempfile
from unittest.mock import patch

source=Path(__file__).resolve().parents[3]/'run_fuc.py'
with tempfile.TemporaryDirectory(prefix='poke-fuc-launch-tests-') as temporary:
    root=Path(temporary).resolve();script=root/'run_fuc.py';shutil.copyfile(source,script)
    spec=importlib.util.spec_from_file_location('fuc_launcher',script)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    with patch.object(sys,'argv',[str(script)]):
        try:module.main()
        except SystemExit as exc:assert 'missing' in str(exc)
        else:raise AssertionError('missing inputs accepted')
    for name in ('out/mhfuc/bin/Poke','mhfuc/EBOOT.ELF','mhfuc/disc.iso'):
        p=root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('fixture')
    def capture(binary,args,env):
        assert binary==str(root/'out/mhfuc/bin/Poke')
        assert args==[binary,'--data-dir',str(root/'out/fuc-user-data'),str(root/'mhfuc')]
        assert env['MHP2G_OVERLAY_DIR']==str(root/'out/mhfuc/bin/overlays')
        assert env['PATH']==os.environ['PATH']
        assert not any(k.startswith('MHP2G_TRACE_') for k in env)
        assert 'MHP2G_TEXTURE_PACK' not in env and 'MHP2G_GPU_DECODE' not in env
        assert env['PRESERVED_FIXTURE']=='yes'
    with patch.object(sys,'argv',[str(script)]), patch.dict(os.environ,{
        'MHP2G_TRACE_SHARPNESS':'1','MHP2G_TRACE_HUD_SCALE':'1','MHP2G_TRACE_BOOT':'1',
        'MHP2G_GPU_DECODE':'1','MHP2G_TEXTURE_PACK':'/unused/pack','PRESERVED_FIXTURE':'yes'}):
        with patch.object(module.os,'execve',side_effect=capture) as execute:
            module.main();assert execute.call_count==1
        assert os.environ['MHP2G_TRACE_SHARPNESS']=='1'
    with patch.object(sys,'argv',[str(script),'unexpected']):
        try:module.main()
        except SystemExit as exc:assert 'Usage:' in str(exc)
        else:raise AssertionError('unexpected arguments accepted')
    assert (root/'out/fuc-user-data').is_dir()
print('FUComplete launcher checks passed: isolated paths, missing inputs, environment and argument handling.')
