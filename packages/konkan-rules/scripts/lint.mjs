import {readdir, readFile} from 'node:fs/promises';
import {join} from 'node:path';

const root = new URL('../src/', import.meta.url);
const files = (await readdir(root)).filter((name) => name.endsWith('.ts'));
const failures = [];
for (const name of files) {
  const text = await readFile(new URL(name, root), 'utf8');
  if (/\beval\s*\(/.test(text)) failures.push(`${name}: eval is not allowed`);
  if (/SUPABASE|LIVEKIT|SERVICE_ROLE|PRIVATE_KEY|ghp_|github_pat_/.test(text)) failures.push(`${name}: infrastructure or credential identifier leaked`);
  if (/[ \t]+$/m.test(text)) failures.push(`${name}: trailing whitespace`);
}
if (failures.length) {
  console.error(failures.join('\n'));
  process.exit(1);
}
console.log(`lint passed (${files.length} source files)`);
