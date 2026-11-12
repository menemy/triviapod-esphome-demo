#!/usr/bin/env python3
"""Focused host tests plus staged-public-image provenance/manifest checks. No serial I/O."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import wave

ROOT=Path(__file__).resolve().parents[1]

def check_export(require_device_run=False):
    docs=ROOT/'docs';meta=json.loads((docs/'firmware/build.json').read_text());manifest=json.loads((docs/'manifest.json').read_text());visible=json.loads((docs/'version.json').read_text())
    assert manifest['version']==meta['version']==visible['version']
    assert manifest['home_assistant_domain']=='esphome'
    assert len(manifest['builds'])==1 and manifest['builds'][0]['chipFamily']=='ESP32-S3'
    immutable='triviapod-demo-'+meta['images']['firmware.factory.bin']['sha256_prefix']+'.factory.bin'
    assert manifest['builds'][0]['parts']==[{'path':'firmware/'+immutable,'offset':0}]
    image=docs/'firmware/triviapod-demo.factory.bin';data=image.read_bytes()
    assert data[0]==0xE9 and len(data)<32*1024*1024
    digest=hashlib.sha256(data).hexdigest();expected=meta['images']['firmware.factory.bin']
    assert digest==expected['sha256']==visible['factory_sha256'] and len(data)==expected['bytes']
    assert (docs/'firmware'/immutable).read_bytes()==data
    assert digest in (docs/'firmware/SHA256SUMS').read_text()
    validation=json.loads((docs/'firmware/validation.json').read_text())
    assert validation['factory_sha256']==digest
    if require_device_run and not validation.get('ran_on_device'):
        raise SystemExit('Generic image has not run on the device; installer publication is blocked')
    for name,expected_hash in meta['source_sha256'].items():
        path=(ROOT/name).resolve()
        assert path.is_relative_to(ROOT) and not any(v in path.relative_to(ROOT).parts for v in ['secrets.yaml','.esphome','.build','.git'])
        assert hashlib.sha256(path.read_bytes()).hexdigest()==expected_hash,'Staged image source mismatch: '+name
    config=(ROOT/'triviapod-demo.yaml').read_text();header=(ROOT/'version_info.h').read_text()
    assert f'version: "{meta["version"]}" # GENERATED_BUILD_VERSION' in config
    assert f'char VERSION[] = "{meta["version"]}";' in header
    inputs=[ROOT/'triviapod-demo.yaml',*(ROOT/'packages').glob('*.yaml')]
    for path in inputs:
        text=path.read_text()
        assert '!secret' not in text and not re.search(r'(?m)^\s*(ssid|password|key|encryption)\s*:',text),str(path)
        assert not re.search(r'/Users/[^/]+/',text),str(path)
    ui=(ROOT/'packages/ui.yaml').read_text();audio=(ROOT/'packages/audio.yaml').read_text()
    assert 'A440' not in ui and 'Speaker Tone A' not in audio and 'Tone 1/2k' in ui
    assert 'platform: homeassistant' not in ui and 'http://example.invalid/snapshot.jpg' in ui
    assert 'last.draw_cover(it)' in ui and 'improv_serial:' in config
    assert 'esp_read_mac' not in (ROOT/'components/triviapod/triviapod.cpp').read_text()
    with wave.open(str(docs/'firmware/tone-1k-2k-16k-mono.wav'),'rb') as sound:
        assert (sound.getnchannels(),sound.getsampwidth(),sound.getframerate(),sound.getnframes())==(1,2,16000,16000)
    print('PASS: staged generic image,checksums,source hashes,version,model/offset and credential-free configuration')

def host_tests():
    compiler=os.environ.get('CXX') or shutil.which('clang++') or shutil.which('g++')
    assert compiler,'Install a C++17 compiler for host checks'
    with tempfile.TemporaryDirectory(prefix='triviapod-host-') as folder:
        for name in ['test_tone','test_camera_cover','test_rtc']:
            binary=Path(folder)/name
            subprocess.run([compiler,'-std=c++17','-I',str(ROOT),str(ROOT/'tests'/f'{name}.cpp'),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
    subprocess.run([shutil.which('python3') or 'python3',str(ROOT/'tests/test_media_guard.py')],cwd=ROOT,check=True)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--export-only',action='store_true');parser.add_argument('--require-device-run',action='store_true');args=parser.parse_args()
    if not args.export_only:host_tests()
    check_export(require_device_run=args.require_device_run)
if __name__=='__main__':main()
