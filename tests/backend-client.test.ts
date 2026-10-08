import assert from 'node:assert/strict';
import {test} from 'node:test';
import {HzaBackendClient} from '../selected-source/lib/hza-backend-client.ts';

test('socket ownership, pending closes, malformed messages and immutable message type',async()=>{
 const original=globalThis.WebSocket;
 class Socket{
  static OPEN=1;static instances:Socket[]=[];readyState=0;sent:string[]=[];
  onopen:any;onclose:any;onerror:any;onmessage:any;
  url:string;protocols:string[];
  constructor(url:string,protocols:string[]){this.url=url;this.protocols=protocols;Socket.instances.push(this)}
  open(){this.readyState=1;this.onopen?.({})}
  send(text:string){this.sent.push(text)}
  close(){this.readyState=3;this.onclose?.({code:1000,reason:''})}
 }
 globalThis.WebSocket=Socket as any;
 try{
  const client=new HzaBackendClient('https://game.test',async()=>'test-token');
  const messages:unknown[]=[];client.onMessage(m=>messages.push(m));
  const first=client.connectRoom('ABCD');const firstRejected=assert.rejects(first,/CONNECTION_SUPERSEDED/);await Promise.resolve();await Promise.resolve();
  const second=client.connectRoom('EFGH');await firstRejected;await Promise.resolve();await Promise.resolve();
  const active=Socket.instances.at(-1)!;active.open();assert.equal(await second,active);assert.equal(Socket.instances[0].readyState,3);
  active.onmessage({data:'null'});assert.deepEqual(messages,[{type:'protocol_error'}]);
  client.send('ready',{type:'start',ready:true});assert.equal(JSON.parse(active.sent[0]).type,'ready');
  const third=client.connectRoom('IJKL');const closed=assert.rejects(third,/SOCKET_CLOSED/);await Promise.resolve();await Promise.resolve();Socket.instances.at(-1)!.close();await closed;
  client.close();assert.throws(()=>client.send('ready'),/SOCKET_NOT_OPEN/);
  assert.throws(()=>new HzaBackendClient('http://public.test',async()=>''),/HTTPS_REQUIRED/);
 }finally{globalThis.WebSocket=original;}
});
