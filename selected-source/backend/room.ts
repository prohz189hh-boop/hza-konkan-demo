import {DurableObject} from 'cloudflare:workers';
import {beginRoom,liveRoom,moveInRoom,tickRoom,ensure} from '../lib/game/lobby.ts';
import type {RoomState,Member} from '../lib/game/lobby.ts';
import type {Action} from '../lib/game/engine.ts';
import {matchModes} from '../lib/game/modes.ts';
import type {MatchMode,BotLevel,SeatPlan} from '../lib/game/modes.ts';
import {createMatch,settleMatch} from './supabase.ts';
import {matchWinner} from './match-result.ts';

type Env={SUPABASE_URL:string;SUPABASE_PUBLISHABLE_KEY:string;SUPABASE_SERVICE_ROLE_KEY?:string;SERVER_VERSION?:string};
type Stored={room:RoomState;matchId:string|null;settled:boolean};
type Attachment={userId:string;roomCode:string};
const now=()=>Date.now();
const botNames=['Dara','Roj','Ari','Azad'];
const botLevels:BotLevel[]=['beginner','intermediate','advanced','pro'];
const seatTeam=(seat:number)=>seat%2+1;
function safeTheme(v:unknown){const x=String(v||'royal').slice(0,40);return/^[a-z0-9_-]+$/i.test(x)?x:'royal'}
function safeName(v:unknown){return String(v||'Erbil table').normalize('NFKC').trim().replace(/[^\p{L}\p{N}_ .-]/gu,'').slice(0,40)||'Erbil table'}
function summary(s:Stored){const r=s.room;return{id:r.id,code:r.code,name:r.name,mode:r.matchMode||'regular',theme:r.theme,public:r.public,status:r.game?.status||'waiting',players:r.seats.filter(Boolean).length,rounds:r.rounds,drawSeconds:r.drawSeconds||20,playSeconds:r.playSeconds||30};}

export class KonkanRoom extends DurableObject<Env>{
 async load(){return (await this.ctx.storage.get('state') as Stored|undefined)||null}
 async save(s:Stored){s.room.updated=now();await this.ctx.storage.put('state',s);await this.schedule(s);}
 send(ws:WebSocket,data:unknown){try{ws.send(JSON.stringify(data))}catch{}}
 broadcast(s:Stored,event?:unknown){for(const ws of this.ctx.getWebSockets()){const a=ws.deserializeAttachment() as Attachment|undefined;if(!a?.userId)continue;this.send(ws,{type:'state',room:liveRoom(s.room,a.userId),event:event||null});}}
 async schedule(s:Stored){
  if(s.room.game?.status==='finished'&&!s.settled&&s.matchId&&this.env.SUPABASE_SERVICE_ROLE_KEY){await this.ctx.storage.setAlarm(now()+30000);return;}
  const r=s.room;if(!r.game||r.game.status!=='playing'){try{await this.ctx.storage.deleteAlarm()}catch{};return}
  let at=r.deadline||now()+1000;const current=r.seats[r.game.turn];if(current?.bot)at=Math.min(at,Math.max(now()+100,r.lastBot+850));
  await this.ctx.storage.setAlarm(at);
 }
 async settleIfNeeded(s:Stored){
  if(s.settled||s.room.game?.status!=='finished')return;
  const g=s.room.game,winnerTeam=matchWinner(g.scores);
  // Do not award a tie to an arbitrary team. The live RPC's draw representation
  // is not in this repository; preserve the finished match pending that contract.
  if(winnerTeam===null)return;
  const players=s.room.seats.map((m,i)=>m?{id:m.id,team:seatTeam(i),bot:m.bot}:null).filter(Boolean) as {id:string;team:number;bot:boolean}[];
  s.settled=await settleMatch(this.env,s.matchId,winnerTeam,g.round,players,g.scores,(s.room.matchMode||'regular')==='ranked');
 }
 async createMatchIfNeeded(s:Stored){
  if(s.matchId||!s.room.game)return;
  const captainMember=s.room.seats[s.room.game.captain];
  s.matchId=await createMatch(this.env,{id:s.room.id,mode:s.room.matchMode||'regular',captain:captainMember&&!captainMember.bot?captainMember.id:null,players:s.room.seats.map((m,i)=>m?{id:m.id,team:seatTeam(i),seat:i,bot:m.bot}:null).filter(Boolean) as {id:string;team:number;seat:number;bot:boolean}[]});
 }
 addMember(r:RoomState,m:Member,seatPlan:SeatPlan='open'){
  const existing=r.seats.findIndex(x=>x?.id===m.id);if(existing>=0){r.seats[existing]={...r.seats[existing]!,...m,ready:r.seats[existing]!.ready};return existing}
  let seat=-1;
  if(seatPlan==='coop'&&!r.seats[2])seat=2;
  else if(seatPlan==='duel'&&!r.seats[1])seat=1;
  else seat=r.seats.findIndex(x=>!x);
  ensure(seat>=0,'ROOM_FULL');r.seats[seat]=m;return seat;
 }
 async fetch(request:Request){return this.ctx.blockConcurrencyWhile(()=>this.handleFetch(request));}
 async handleFetch(request:Request){
  const url=new URL(request.url),method=request.method.toUpperCase();let s=await this.load();
  if(method==='POST'&&url.pathname.endsWith('/_init')){
   if(s)return Response.json({ok:true,room:summary(s)});
   const body=await request.json() as {id:string;code:string;name?:string;host:Member;seats?:Array<Member|null>;mode?:MatchMode;botLevel?:BotLevel;seatPlan?:SeatPlan;rounds?:number;theme?:string;public?:boolean;passwordHash?:string;drawSeconds?:number;playSeconds?:number};
   const mode:MatchMode=['regular','turbo','ranked'].includes(String(body.mode))?body.mode!:'regular';const rounds=body.rounds||matchModes[mode].rounds;const t=now();
   const initialSeats=Array.isArray(body.seats)&&body.seats.length===4?body.seats:[{...body.host,ready:false},null,null,null];
   const room:RoomState={id:body.id,code:body.code,name:safeName(body.name),host:body.host.id,seats:initialSeats,matchMode:mode,botLevel:botLevels.includes(body.botLevel as BotLevel)?body.botLevel:'intermediate',seatPlan:body.seatPlan||'open',rounds,timer:body.playSeconds||30,drawSeconds:body.drawSeconds||20,playSeconds:body.playSeconds||30,theme:safeTheme(body.theme),public:!!body.public,passwordHash:String(body.passwordHash||''),game:null,deadline:0,lastBot:t,created:t,updated:t,messages:[]};
   s={room,matchId:null,settled:false};await this.save(s);return Response.json({ok:true,room:summary(s)});
  }
  if(!s)return Response.json({error:'ROOM_NOT_FOUND'},{status:404});
  // Internal binding only: the public Worker never forwards this path.
  if(method==='POST'&&url.pathname==='/_membership'){
   const body=await request.json() as {userId?:string};
   return Response.json({member:!!body.userId&&s.room.seats.some(m=>m?.id===body.userId)},{status:body.userId&&s.room.seats.some(m=>m?.id===body.userId)?200:403});
  }
  if(method==='GET'&&request.headers.get('Upgrade')?.toLowerCase()!=='websocket')return Response.json({room:summary(s)});
  if(request.headers.get('Upgrade')?.toLowerCase()==='websocket'){
   const userId=request.headers.get('X-HZA-User-Id'),name=request.headers.get('X-HZA-User-Name')||'Player',tag=request.headers.get('X-HZA-User-Tag')||'PLAYER',rating=Number(request.headers.get('X-HZA-Rating')||0),passwordHash=request.headers.get('X-HZA-Password-Hash')||'';
   if(!userId)return Response.json({error:'UNAUTHORIZED'},{status:401});
   if(s.room.passwordHash&&s.room.passwordHash!==passwordHash&&!s.room.seats.some(m=>m?.id===userId))return Response.json({error:'BAD_PASSWORD'},{status:403});
   if(s.room.game&&!s.room.seats.some(m=>m?.id===userId))return Response.json({error:'MATCH_STARTED'},{status:409});
   const member:Member={id:userId,name,tag,ready:false,bot:false,registered:true,rating};const seat=this.addMember(s.room,member,s.room.seatPlan||'open');await this.save(s);
   for(const old of this.ctx.getWebSockets()){const a=old.deserializeAttachment() as Attachment|undefined;if(a?.userId===userId)try{old.close(4001,'Reconnected')}catch{}}
   const pair=new WebSocketPair(),[client,server]=Object.values(pair);this.ctx.acceptWebSocket(server);server.serializeAttachment({userId,roomCode:s.room.code} satisfies Attachment);
   this.send(server,{type:'welcome',seat,team:seatTeam(seat),host:s.room.host===userId,room:liveRoom(s.room,userId)});this.broadcast(s,{type:'joined',userId,seat});
   return new Response(null,{status:101,webSocket:client,headers:{'Sec-WebSocket-Protocol':'hza-konkan'}} as any);
  }
  return Response.json({error:'METHOD_NOT_ALLOWED'},{status:405});
 }
 async webSocketMessage(ws:WebSocket,raw:string|ArrayBuffer){return this.ctx.blockConcurrencyWhile(()=>this.handleMessage(ws,raw));}
 async handleMessage(ws:WebSocket,raw:string|ArrayBuffer){
  const a=ws.deserializeAttachment() as Attachment|undefined;if(!a?.userId)return;const s=await this.load();if(!s)return;
  const text=typeof raw==='string'?raw:new TextDecoder().decode(raw);
  if(new TextEncoder().encode(text).byteLength>16384)return this.send(ws,{type:'error',code:'MESSAGE_TOO_LARGE'});
  let msg:any;try{msg=JSON.parse(text)}catch{return this.send(ws,{type:'error',code:'INVALID_JSON'})}
  if(!msg||typeof msg!=='object'||Array.isArray(msg)||typeof msg.type!=='string')return this.send(ws,{type:'error',code:'INVALID_MESSAGE'});
  const r=s.room,seat=r.seats.findIndex(m=>m?.id===a.userId),me=seat>=0?r.seats[seat]:null;if(!me)return this.send(ws,{type:'error',code:'NOT_SEATED'});
  try{
   switch(msg.type){
    case'get_state':this.send(ws,{type:'state',room:liveRoom(r,a.userId)});return;
    case'ready':ensure(!r.game,'MATCH_STARTED');me!.ready=!!msg.ready;break;
    case'seat':ensure(!r.game,'MATCH_STARTED');ensure((r.seatPlan||'open')==='open','FIXED_SEATS');ensure(Number.isInteger(msg.seat)&&msg.seat>=0&&msg.seat<4&&!r.seats[msg.seat],'SEAT_UNAVAILABLE');r.seats[seat]=null;r.seats[msg.seat]=me;break;
    case'add_bot':ensure(!r.game&&r.host===a.userId,'HOST_ONLY');{const idx=r.seats.findIndex(x=>!x);ensure(idx>=0,'ROOM_FULL');r.seats[idx]={id:`bot:${crypto.randomUUID()}`,name:botNames[idx],tag:'BOT',ready:true,bot:true};if(botLevels.includes(msg.level))r.botLevel=msg.level;}break;
    case'remove_bot':ensure(!r.game&&r.host===a.userId,'HOST_ONLY');{const idx=r.seats.findIndex(x=>x?.id===msg.id&&x?.bot);ensure(idx>=0,'BOT_NOT_FOUND');r.seats[idx]=null;}break;
    case'set_mode':ensure(!r.game&&r.host===a.userId,'HOST_ONLY');ensure(['regular','turbo','ranked'].includes(msg.mode),'INVALID_MODE');r.matchMode=msg.mode;r.rounds=matchModes[msg.mode as MatchMode].rounds;break;
    case'start':ensure(!r.game&&r.host===a.userId,'HOST_ONLY');ensure(r.seats.every(x=>x&&(x.bot||x.ready)),'FOUR_READY_REQUIRED');if((r.matchMode||'regular')==='ranked')ensure(r.seats.every(x=>x&&!x.bot&&x.registered),'RANKED_HUMANS_ONLY');beginRoom(r,crypto.getRandomValues(new Uint32Array(1))[0]||1,now());await this.createMatchIfNeeded(s);break;
    case'move':ensure(r.game,'MATCH_NOT_STARTED');moveInRoom(r,a.userId,Number(msg.version),msg.action as Action,now());await this.settleIfNeeded(s);break;
    case'message':{const text=String(msg.text||'').trim().slice(0,160);if(text)r.messages=[...r.messages.slice(-29),{name:me!.name,text}];}break;
    case'leave':ensure(!r.game||r.game.status==='finished','MATCH_IN_PROGRESS');r.seats[seat]=null;if(r.host===a.userId){const next=r.seats.find(x=>x&&!x.bot);r.host=next?.id||'';}break;
    default:throw new Error('UNKNOWN_ACTION');
   }
   await this.save(s);this.broadcast(s,{type:'action',action:msg.type,by:a.userId});
  }catch(e){this.send(ws,{type:'error',code:e instanceof Error?e.message:'ACTION_FAILED'});}
 }
 async webSocketClose(ws:WebSocket){const a=ws.deserializeAttachment() as Attachment|undefined;if(!a?.userId)return;const s=await this.load();if(!s)return;this.broadcast(s,{type:'disconnected',userId:a.userId});}
 async alarm(){return this.ctx.blockConcurrencyWhile(async()=>{const s=await this.load();if(!s)return;const changed=tickRoom(s.room,now());if(changed||s.room.game?.status==='finished'){await this.settleIfNeeded(s);await this.save(s);if(changed)this.broadcast(s,{type:'timer'});}else await this.schedule(s);});}
}
