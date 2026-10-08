import {DurableObject} from 'cloudflare:workers';
import {matchModes} from '../lib/game/modes.ts';
import type {MatchMode} from '../lib/game/modes.ts';
import type {Member} from '../lib/game/lobby.ts';

type Group={id:string;leader:string;members:Member[];mode:MatchMode;region:string;at:number;allowBots:boolean;queueTicket?:string;ticketOwner?:string};
type Env={GAME_ROOMS:DurableObjectNamespace;SERVER_VERSION?:string};
type Assignment={roomCode:string;roomId:string;expires:number};
const MAX_AGE=10*60*1000;
const rating=(g:Group)=>g.members.reduce((s,m)=>s+Number(m.rating||1000),0)/Math.max(1,g.members.length);
const compatible=(a:Group,b:Group,wait:number)=>a.mode===b.mode&&a.region===b.region&&Math.abs(rating(a)-rating(b))<=(wait>120000?1000:wait>60000?600:wait>30000?350:200);
function roomCode(){return crypto.randomUUID().replaceAll('-','').slice(0,8).toUpperCase()}
function arrange(groups:Group[]){
 const people=groups.flatMap(g=>g.members.map(m=>({...m,ready:true})));if(people.length!==4)return null;
 const duo=groups.find(g=>g.members.length===2),otherDuo=groups.filter(g=>g.members.length===2).find(g=>g!==duo);
 if(duo&&otherDuo)return[duo.members[0],otherDuo.members[0],duo.members[1],otherDuo.members[1]].map(m=>({...m,ready:true}));
 if(duo){const solos=groups.filter(g=>g!==duo).flatMap(g=>g.members);return[duo.members[0],solos[0],duo.members[1],solos[1]].map(m=>({...m,ready:true}));}
 return people.map(m=>({...m,ready:true}));
}
export class Matchmaker extends DurableObject<Env>{
 async queue(){return (await this.ctx.storage.get('queue') as Group[]|undefined)||[]}
 async save(q:Group[]){await this.ctx.storage.put('queue',q)}
 clean(q:Group[]){const t=Date.now();return q.filter(g=>t-g.at<MAX_AGE)}
 choose(q:Group[],base:Group){
  const t=Date.now(),out=[base];let count=base.members.length;
  for(const g of q){if(g.id===base.id)continue;if(count+g.members.length>4)continue;if(!compatible(base,g,t-Math.min(base.at,g.at)))continue;out.push(g);count+=g.members.length;if(count===4)return out;}
  return null;
 }
 async create(groups:Group[]){
  const seats=arrange(groups);if(!seats)throw new Error('INVALID_MATCH_GROUP');const mode=groups[0].mode,code=roomCode(),id=crypto.randomUUID(),host=seats[0]!;
  const stub=this.env.GAME_ROOMS.get(this.env.GAME_ROOMS.idFromName(code));
  const r=await stub.fetch('https://internal/room/_init',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({id,code,name:`${matchModes[mode].name} · Erbil`,host,seats,mode,rounds:matchModes[mode].rounds,botLevel:'intermediate',seatPlan:'open',theme:'royal',public:true,drawSeconds:20,playSeconds:30})});
  if(!r.ok)throw new Error('ROOM_INIT_FAILED');return{roomCode:code,roomId:id,seats};
 }
 async fetch(request:Request){
  // Keep queue reads, room creation and assignment publication in one serial operation.
  return this.ctx.blockConcurrencyWhile(()=>this.handle(request));
 }
 async handle(request:Request){
  const url=new URL(request.url),body=request.method==='POST'?await request.json().catch(()=>({})) as any:{};let q=this.clean(await this.queue());
  if(request.method==='POST'&&url.pathname.endsWith('/join')){
   const group=body.group as Group;if(!group?.id||!Array.isArray(group.members)||!group.members.length||group.members.length>2)return Response.json({error:'INVALID_GROUP'},{status:400});
   if(!['regular','turbo','ranked'].includes(group.mode))return Response.json({error:'INVALID_MODE'},{status:400});
   // Cancellation may reach the DO before a join finishes upstream authentication.
   if(group.queueTicket){const until=await this.ctx.storage.get<number>(`cancel:${group.ticketOwner}:${group.queueTicket}`);if(until&&until>Date.now())return Response.json({status:'idle'});}
   const previous=await this.ctx.storage.get<Assignment>(`assignment:${group.leader}`);
   if(previous&&previous.expires>Date.now())return Response.json({status:'matched',roomCode:previous.roomCode,roomId:previous.roomId});
   q=q.filter(g=>!g.members.some(m=>group.members.some((n:Member)=>n.id===m.id)));q.push({...group,at:Date.now()});
   const chosen=this.choose(q,group);
   if(chosen){
    const ids=new Set(chosen.map(g=>g.id));q=q.filter(g=>!ids.has(g.id));const room=await this.create(chosen);
    const assignment:Assignment={roomCode:room.roomCode,roomId:room.roomId,expires:Date.now()+MAX_AGE};
    const writes:Record<string,unknown>={queue:q};
    for(const member of chosen.flatMap(g=>g.members))writes[`assignment:${member.id}`]=assignment;
    await this.ctx.storage.put(writes);await this.ctx.storage.setAlarm(assignment.expires);
    return Response.json({status:'matched',roomCode:room.roomCode,roomId:room.roomId});
   }
   await this.save(q);return Response.json({status:'queued',queued:q.reduce((s,g)=>s+g.members.length,0)});
  }
  if(request.method==='POST'&&url.pathname.endsWith('/leave')){
   const uid=String(body.userId||''),ticket=body.queueTicket as string|undefined;
   if(ticket){await this.ctx.storage.put(`cancel:${uid}:${ticket}`,Date.now()+MAX_AGE);const alarm=await this.ctx.storage.getAlarm();if(!alarm||alarm>Date.now()+MAX_AGE)await this.ctx.storage.setAlarm(Date.now()+MAX_AGE);}
   // Old-session cleanup cannot remove a newer queue attempt, even for the same account.
   q=q.filter(g=>!(g.members.some(m=>m.id===uid)&&(!ticket||(g.queueTicket===ticket&&g.ticketOwner===uid))));
   await this.save(q);return Response.json({status:'left'});
  }
  if(request.method==='GET'&&url.pathname.endsWith('/status')){
   const uid=url.searchParams.get('userId')||'';
   const assignment=await this.ctx.storage.get<Assignment>(`assignment:${uid}`);
   if(assignment&&assignment.expires>Date.now())return Response.json({status:'matched',roomCode:assignment.roomCode,roomId:assignment.roomId});
   return Response.json({status:q.some(g=>g.members.some(m=>m.id===uid))?'queued':'idle',queued:q.reduce((s,g)=>s+g.members.length,0)});
  }
  return Response.json({error:'NOT_FOUND'},{status:404});
 }
 async alarm(){
  const cancelled=await this.ctx.storage.list<number>({prefix:'cancel:'});
  const expiredTickets=[...cancelled].filter(([,until])=>until<=Date.now()).map(([key])=>key);
  if(expiredTickets.length)await this.ctx.storage.delete(expiredTickets);
  const entries=await this.ctx.storage.list<Assignment>({prefix:'assignment:'});
  const expired=[...entries].filter(([,a])=>a.expires<=Date.now()).map(([key])=>key);
  if(expired.length)await this.ctx.storage.delete(expired);
  const remaining=[...entries.values()].filter(a=>a.expires>Date.now());
  const deadlines=[...remaining.map(a=>a.expires),...[...cancelled.values()].filter(until=>until>Date.now())];
  if(deadlines.length)await this.ctx.storage.setAlarm(Math.min(...deadlines));
 }
}
