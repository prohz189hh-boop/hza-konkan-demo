import {createServer} from 'node:http';
import {readFile} from 'node:fs/promises';
import {build} from 'esbuild';

const root = new URL('../examples/browser-demo/', import.meta.url);
await import('./build-demo.mjs');
const server = createServer(async (request, response) => {
  const path = request.url === '/' ? 'index.html' : request.url.slice(1);
  if (path.includes('..')) { response.writeHead(400); response.end('bad path'); return; }
  try {
    const body = await readFile(new URL(path, root));
    const type = path.endsWith('.html') ? 'text/html' : path.endsWith('.js') ? 'text/javascript' : 'application/json';
    response.writeHead(200, {'content-type': `${type}; charset=utf-8`});
    response.end(body);
  } catch { response.writeHead(404); response.end('not found'); }
});
server.listen(4173, () => console.log('Konkan rules playground: http://localhost:4173'));
