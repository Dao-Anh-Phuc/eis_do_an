# -*- mode: python ; coding: utf-8 -*-


a = Analysis(
    ['main1.py'],
    pathex=[],
    binaries=[],
    datas=[('dai-hoc-khoa-hoc-tu-nhien-Photoroom.png', '.'), ('Logo-DH-Quoc-Gia-Ha-Noi-VNU-Photoroom.png', '.')],
    hiddenimports=['PIL._tkinter_finder', 'scipy', 'scipy.signal', 'pandas', 'serial', 'matplotlib', 'tkinter'],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name='DienHoaApp',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon='NONE',
)
