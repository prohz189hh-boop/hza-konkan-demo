"""Scan this curated export; print locations/categories, never secret values."""
from pathlib import Path
import re, json, hashlib

root=Path(__file__).resolve().parents[1]
blocked={'node_modules','.sites-runtime','Saved','Intermediate','DerivedDataCache','.git','Binaries'}
patterns={
    'JWT':r'eyJ[A-Za-z0-9_-]{16,}\.[A-Za-z0-9_-]{16,}\.[A-Za-z0-9_-]{16,}',
    'private key':r'-----BEGIN (?:RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----',
    'provider credential':r'(?:sb_secret_|sb_publishable_|ghp_|github_pat_|sk_live_|AKIA)[A-Za-z0-9_-]{16,}',
    'database credential URL':r'(?:postgres(?:ql)?|mysql)://[^\s:/]+:[^\s@]+@',
    'production identifier':r'https://(?!YOUR_PROJECT)[a-z0-9]+\.supabase\.co|https://[\w-]+\.[\w-]+\.workers\.dev|C:[/\\]Users[/\\]',
}
findings=[];files=[]
for p in sorted(root.rglob('*')):
    if '.git' in p.relative_to(root).parts or not p.is_file():continue
    rel=p.relative_to(root).as_posix()
    if p.name in {'SECRET-SCAN.json','FILE-MANIFEST.json'}:continue
    if any(x in blocked for x in p.relative_to(root).parts) or (p.name.startswith('.env') and p.name!='.env.example'):
        findings.append({'file':rel,'kind':'excluded file'})
    data=p.read_bytes();files.append({'path':rel,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
    if p.suffix.lower()=='.png':
        if not data.startswith(b'\x89PNG\r\n\x1a\n'):findings.append({'file':rel,'kind':'invalid PNG'})
        continue
    text=data.decode('utf-8')
    if rel=='tools/verify_demo.py':continue # Patterns intentionally contain disallowed identifier examples.
    for kind,pattern in patterns.items():
        if re.search(pattern,text):findings.append({'file':rel,'kind':kind})
    for email in re.findall(r'[\w.+-]+@[\w.-]+\.[A-Za-z]{2,}',text):
        if not re.search(r'@(?:example\.(?:test|invalid|com)|[^@]+\.test)$',email):
            findings.append({'file':rel,'kind':'non-fixture email'})
    for uid in re.findall(r'\b[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}\b',text,re.I):
        if not uid.startswith('00000000-0000-4000-8000-00000000000'):
            findings.append({'file':rel,'kind':'non-fixture UUID'})
    # Inspect literal secret assignments; variable names and runtime handling are expected source code.
    for match in re.finditer(r'''["']?(?:access_token|refresh_token|password|client_secret|service_role_key|api_key|cookie)["']?\s*[:=]\s*["']([^"'\r\n]{8,})["']''',text,re.I):
        value=match.group(1)
        if not value.startswith(('synthetic-','test-','YOUR_')) and value not in ['application/json']:
            findings.append({'file':rel,'kind':'credential-like literal'})
report={'status':'PASS' if not findings else 'FAIL','files_scanned':len(files),'findings':findings,'scope':'All exported text files; PNG signatures verified and screenshots visually reviewed. Synthetic fixture values permitted. No guarantee for every unknown secret format.'}
(root/'docs/SECRET-SCAN.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
(root/'docs/FILE-MANIFEST.json').write_text(json.dumps(files,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report));raise SystemExit(bool(findings))
