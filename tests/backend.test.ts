/** Real Durable Objects/WebSockets; fake identity service only, no production traffic. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {createRequire} from 'node:module';
import {resolve} from 'node:path';
import {chooseBot} from '../selected-source/lib/game/bots.ts';
const require=createRequire(import.meta.url);
const wrangler=require.resolve('wrangler');
const {Miniflare}=require(require.resolve('miniflare',{paths:[wrangler]}));
const {build}=require(require.resolve('esbuild',{paths:[wrangler]}));

test('authoritative Worker: assignments, membership, privacy, malformed messages and reconnect',async()=>{
 const bundled=await build({entryPoints:[resolve('selected-source/backend/worker.ts')],bundle:true,write:false,format:'esm',platform:'neutral',target:'es2022',external:['cloudflare:workers']});
 // Match the runtime pinned in the existing lockfile; production config stays unchanged.
 const mf=new Miniflare({modules:true,script:bundled.outputFiles[0].text,compatibilityDate:'2026-05-15',cf:false,
  bindings:{SUPABASE_URL:'https://identity.test',SUPABASE_PUBLISHABLE_KEY:'test-public'},
  durableObjects:{GAME_ROOMS:{className:'KonkanRoom',useSQLite:true},MATCHMAKER:{className:'Matchmaker',useSQLite:true}},
  outboundService:async(request:Request)=>{
   assert.equal(new URL(request.url).hostname,'identity.test','no real external traffic');
   const token=request.headers.get('Authorization')?.replace('Bearer ','');
   if(!token?.match(/^test-[1-9]$/))return Response.json({error:'invalid'},{status:401});
   const id=token.slice(5),path=new URL(request.url).pathname;
   if(path==='/auth/v1/user')return Response.json({id,email:`player${id}@example.test`});
   if(path==='/rest/v1/profiles')return Response.json([{id,display_name:`Player ${id}`,game_tag:`P${id}`}]);
   if(path==='/rest/v1/player_stats')return Response.json([{user_id:id,rating:1000}]);
   return Response.json([]);
  }
 });
 const sockets:any[]=[];
 const api=async(user:number,path:string,payload?:unknown)=>{
  const response=await mf.dispatchFetch(`https://game.test${path}`,{method:payload===undefined?'GET':'POST',headers:{Authorization:`Bearer test-${user}`,'Content-Type':'application/json'},...(payload===undefined?{}:{body:JSON.stringify(payload)})});
  return {status:response.status,body:await response.json() as any};
 };
 async function connect(user:number,code:string){
  const response=await mf.dispatchFetch(`https://game.test/room/${code}`,{headers:{Upgrade:'websocket','Sec-WebSocket-Protocol':`hza-konkan, auth.test-${user}`}});
  assert.equal(response.status,101);const socket=response.webSocket;assert.ok(socket);sockets.push(socket);
  const messages:any[]=[];socket.addEventListener('message',(event:any)=>messages.push(JSON.parse(event.data)));socket.accept();
  async function wait(predicate:(m:any)=>boolean){
   const end=Date.now()+5000;
   while(Date.now()<end){const i=messages.findIndex(predicate);if(i>=0)return messages.splice(i,1)[0];await new Promise(r=>setTimeout(r,10));}
   throw new Error(`timed out waiting for socket message; errors: ${messages.filter(m=>m.type==='error').map(m=>m.code).join(',')}`);
  }
  return{socket,wait,messages};
 }
 try{
  const unauthorized=await mf.dispatchFetch('https://game.test/api/matchmaking/status');assert.equal(unauthorized.status,401);
  const oldTicket='old-search-00000001',newTicket='new-search-00000002';
  await api(8,'/api/matchmaking/leave',{queueTicket:oldTicket});
  assert.equal((await api(8,'/api/matchmaking/join',{mode:'turbo',queueTicket:oldTicket})).body.status,'idle','leave-before-join cannot requeue a signed-out client');
  assert.equal((await api(8,'/api/matchmaking/join',{mode:'turbo',queueTicket:newTicket})).body.status,'queued');
  await api(8,'/api/matchmaking/leave',{queueTicket:oldTicket});
  assert.equal((await api(8,'/api/matchmaking/status')).body.status,'queued','stale cleanup cannot cancel newer search');
  await api(9,'/api/matchmaking/leave',{queueTicket:newTicket,userId:'8'});
  assert.equal((await api(8,'/api/matchmaking/status')).body.status,'queued','ticket cannot override verified identity');
  await api(8,'/api/matchmaking/leave',{queueTicket:newTicket});
  assert.equal((await api(8,'/api/matchmaking/status')).body.status,'idle');
  assert.equal((await api(8,'/api/matchmaking/join',{queueTicket:'bad'})).status,400);
  for(let user=1;user<=3;user++)assert.equal((await api(user,'/api/matchmaking/join',{mode:'regular'})).body.status,'queued');
  const last=await api(4,'/api/matchmaking/join',{mode:'regular'});assert.equal(last.body.status,'matched');const code=last.body.roomCode;
  for(let user=1;user<=4;user++){
   const assignment=await api(user,'/api/matchmaking/status?userId=9');assert.equal(assignment.body.status,'matched');assert.equal(assignment.body.roomCode,code);
  }
  assert.equal((await api(5,'/api/matchmaking/status?userId=1')).body.status,'idle');
  assert.equal((await api(1,'/api/matchmaking/join',{mode:'regular'})).body.roomCode,code,'repeat join returns original assignment');
  assert.equal((await api(5,'/api/voice/token',{room:code})).status,403);
  const voice=await api(1,'/api/voice/token',{room:code});assert.equal(voice.body.error,'voice_not_configured');
  const clients=[];const welcomes=[];for(let user=1;user<=4;user++){const c=await connect(user,code);welcomes.push(await c.wait(m=>m.type==='welcome'));clients.push(c);}
  clients[0].socket.send('null');assert.equal((await clients[0].wait(m=>m.type==='error')).code,'INVALID_MESSAGE');
  clients[0].socket.send('[]');assert.equal((await clients[0].wait(m=>m.type==='error')).code,'INVALID_MESSAGE');
  clients[0].socket.send('{');assert.equal((await clients[0].wait(m=>m.type==='error')).code,'INVALID_JSON');
  clients[0].socket.send(' '.repeat(16385));assert.equal((await clients[0].wait(m=>m.type==='error')).code,'MESSAGE_TOO_LARGE');
  clients[welcomes.findIndex(w=>w.host)].socket.send(JSON.stringify({type:'start'}));
  const states=[];for(const client of clients)states.push((await client.wait(m=>m.type==='state'&&m.room.game)).room);
  for(let client=0;client<4;client++){
   assert.ok(Math.abs(states[client].serverNow-Date.now())<5000,'WebSocket snapshot carries fresh server clock');
   const game:any=states[client].game;const seat:number=welcomes[client].seat;assert.equal(game.seat,seat);assert.equal(game.players[seat].hand.length,seat===game.captain?15:14);
   for(let other=0;other<4;other++)if(other!==seat)assert.deepEqual(game.players[other].hand,[]);
   assert.equal(game.deck,undefined);assert.equal(game.seed,undefined);assert.equal(game.entropy,undefined);assert.equal(states[client].passwordHash,undefined);
  }
  const resumed=await connect(1,code);const welcome=await resumed.wait(m=>m.type==='welcome');
  clients[0]=resumed;
  const ownSeat=welcome.seat;
  assert.equal(welcome.room.deadline,states[0].deadline);assert.deepEqual(welcome.room.game.players[ownSeat].hand,states[0].game.players[ownSeat].hand);
  assert.deepEqual(welcome.room.game.players[(ownSeat+1)%4].hand,[]);
  await new Promise(r=>setTimeout(r,Math.max(0,welcome.room.dealUntil-Date.now()+100)));
  const captain=welcome.room.game.captain,actor=captain===ownSeat?resumed:clients[welcomes.findIndex(w=>w.seat===captain)];
  actor.socket.send(JSON.stringify({type:'move',version:welcome.room.version,action:{type:'discard',tile:'forged-tile'}}));
  assert.equal((await actor.wait(m=>m.type==='error')).code,'TILE_NOT_OWNED');
  const hand=states[welcomes.findIndex(w=>w.seat===captain)].game.players[captain].hand;
  const version=welcome.room.version;
  actor.socket.send(JSON.stringify({type:'move',version,action:{type:'discard',tile:hand[0].id}}));
  const changed=await actor.wait(m=>m.type==='state'&&m.room.version>version);
  assert.equal(changed.room.game.players[captain].tileCount,14);
  actor.socket.send(JSON.stringify({type:'move',version,action:{type:'discard',tile:hand[0].id}}));
  assert.equal((await actor.wait(m=>m.type==='error')).code,'The table changed. Try your move again.');
  actor.socket.send(JSON.stringify({type:'move',version:changed.room.version,action:{type:'draw',source:'deck'}}));
  assert.equal((await actor.wait(m=>m.type==='error')).code,'NOT_YOUR_TURN');
  // Drive the real transport from redacted views until the round ends.
  let room=changed.room,moves=0;
  while(room.game.status==='playing'&&moves<1500){
   const clientIndex=welcomes.findIndex(w=>w.seat===room.game.turn),client=clients[clientIndex];
   client.socket.send(JSON.stringify({type:'get_state'}));
   const own=(await client.wait(m=>m.type==='state'&&m.room.version===room.version)).room;
   const action=chooseBot(own.game,'pro');
   client.socket.send(JSON.stringify({type:'move',version:own.version,action}));
   room=(await client.wait(m=>m.type==='state'&&m.room.version>own.version)).room;moves++;
  }
  assert.equal(room.game.status,'roundEnd','one full round through four real sockets');
  assert.equal(room.game.results.length,1);assert.ok(room.game.scores.every(Number.isFinite));
  const previousRound=room.game.round;
  clients[0].socket.send(JSON.stringify({type:'move',version:room.version,action:{type:'next'}}));
  const next=(await clients[0].wait(m=>m.type==='state'&&m.room.version>room.version)).room;
  assert.equal(next.game.round,previousRound+1);assert.equal(next.game.status,'playing');
  assert.ok(next.dealUntil>Date.now());
 }finally{for(const socket of sockets)try{socket.close()}catch{}await mf.dispose();}
});
