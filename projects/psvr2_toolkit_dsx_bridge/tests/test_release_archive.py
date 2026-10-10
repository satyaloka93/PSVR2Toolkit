"""Validate a generated full package without extracting or installing it."""
from pathlib import Path
import hashlib
import sys
import zipfile

archive = Path(sys.argv[1])
bridge_binary = Path(sys.argv[2])
expected = {
    'README.md', 'LICENSE', 'START_HERE.txt', 'SHA256SUMS.txt',
    'driver/driver_playstation_vr2.dll', 'driver/INSTALL_RAW_TRIGGER_DRIVER.cmd',
    'driver/install_raw_trigger_driver.ps1',
    'bridge/psvr2_toolkit_dsx_bridge_017.exe', 'bridge/psvr2_toolkit_capi_loader.dll',
    'bridge/run_cyberpunk017.cmd', 'bridge/run_cyberpunk017.ps1',
    'bridge/Cyberpunk017.Common.ps1', 'bridge/setup_cyberpunk017.cmd',
    'bridge/setup_cyberpunk017.ps1', 'bridge/cet/PSVR2 Haptics/init.lua',
}
with zipfile.ZipFile(archive) as package:
    entries = {i.filename.replace('\\', '/'): i for i in package.infolist()
               if not i.filename.replace('\\', '/').endswith('/')}
    assert set(entries) == expected, (set(entries) ^ expected)
    manifest = {}
    for line in package.read(entries['SHA256SUMS.txt']).decode('utf-8-sig').splitlines():
        digest, name = line.split('  ', 1)
        name = name.replace('\\', '/')
        assert name not in manifest
        manifest[name] = digest
        assert hashlib.sha256(package.read(entries[name])).hexdigest() == digest, name
    assert set(manifest) == expected - {'SHA256SUMS.txt'}
    assert manifest['driver/driver_playstation_vr2.dll'] == '977856e7c1d54a8aea0bf4ef19095ce901fb658d301ca4ba6bf8e077d83a2b43'
    assert manifest['bridge/psvr2_toolkit_dsx_bridge_017.exe'] == hashlib.sha256(bridge_binary.read_bytes()).hexdigest()
    assert not any(name.endswith('settings.ini') or name.endswith('settings.json') for name in entries)
print(f'PASS: {len(entries)} intended files; split directories, hashes, tested driver, built bridge, no preference overwrite.')
