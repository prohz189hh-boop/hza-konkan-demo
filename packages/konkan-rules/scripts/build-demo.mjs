import {build} from 'esbuild';
import {mkdir} from 'node:fs/promises';
import {fileURLToPath} from 'node:url';

const dist = fileURLToPath(new URL('../examples/browser-demo/dist/', import.meta.url));
await mkdir(dist, {recursive: true});
await build({
  entryPoints: [fileURLToPath(new URL('../examples/browser-demo/main.ts', import.meta.url))],
  outfile: fileURLToPath(new URL('../examples/browser-demo/dist/app.js', import.meta.url)),
  bundle: true,
  format: 'esm',
  target: 'es2022',
  sourcemap: true,
  logLevel: 'info'
});
