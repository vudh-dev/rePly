from pathlib import Path
import zipfile, hashlib, re
root=Path(__file__).resolve().parent.parent
out=root/'release'/'rePly-0.1.0-beta.1-source.zip'
exclude={'node_modules','dist','release','.git','.payload-build','build','build-host','__pycache__','preview','debug'}
files=[]
for p in root.rglob('*'):
    rel=p.relative_to(root)
    if not p.is_file() or any(x in exclude for x in rel.parts) or p.name=='.DS_Store' or p.suffix in {'.log','.pyc'}: continue
    if p.name in {'xovi.cpp','xovi.h'}:continue
    files.append(p)
patterns=[rb'sk-(?:proj|ant)-[A-Za-z0-9_-]{25,}',rb'-----BEGIN (?:OPENSSH|RSA|EC) PRIVATE KEY',rb'/Users/[A-Za-z0-9_-]+/(?:Documents|\.ssh)/']
for p in files:
    data=p.read_bytes()
    if any(re.search(pattern,data) for pattern in patterns):
        # The audit script deliberately contains its own signatures.
        if p!=Path(__file__).resolve():raise SystemExit('Sensitive-pattern audit failed: '+str(p.relative_to(root)))
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
    for p in sorted(files):z.write(p,Path('reply')/p.relative_to(root))
with (root/'release'/'SHA256SUMS').open('w') as sums:
    for p in sorted((root/'release').glob('*.zip')):sums.write(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n')
print(f'Source archive: {len(files)} files, {out.stat().st_size} bytes. Sensitive-pattern audit passed.')
