import {chooseBot} from './bots.ts';
import {modeOf,sameQueue,matchModes} from './modes.ts';
import type {MatchMode,BotLevel,SeatPlan} from './modes.ts';
import type {ArcadeRoom} from './arcade.ts';
import {newGame,applyAction,botAction,viewFor} from './engine.ts';
import type {Game,Action,Tile} from './engine.ts';
export type Member={registered?:boolean,id:string,name:string,tag:string,ready:boolean,bot:boolean,avatar?:string,frame?:string,entrance?:string,rating?:number};
export type TableEvent={version:number,kind:Action['type'],seat:number,at:number,tile?:Tile};
export const DEAL_MS=4400;
export const ENTRANCE_MS=1800;
export type RoomState={matchMode?:MatchMode,botLevel?:BotLevel,seatPlan?:SeatPlan,entranceUntil?:number,drawSeconds?:number,playSeconds?:number,phaseStartedAt?:number,dealStartedAt?:number,dealUntil?:number,events?:TableEvent[],allowBots?:boolean,id:string,code:string,name:string,host:string,seats:(Member|null)[],rounds:number,timer:number,theme:string,public:boolean,passwordHash:string,game:Game|null,deadline:number,lastBot:number,created:number,updated:number,messages:{name:string,text:string}[]};
export type Party={id:string,leader:string,members:Member[],ready:string[]};
export type Arena={arcade?:ArcadeRoom[],rooms:RoomState[],parties:Party[],invites:{id:string,from:string,to:string,name:string,created:number}[],queue:{id:string,members:Member[],at:number,allowBots?:boolean,matchMode?:MatchMode}[],presence:Record<string,number>};
export const emptyArena=():Arena=>({rooms:[],parties:[],invites:[],queue:[],presence:{}});
export const member=(p:{id:string,name:string,tag:string,equipped?:string,progress?:string,identity?:string|null}):Member=>{const e=JSON.parse(p.equipped||'{}'),progress=JSON.parse(p.progress||'{}');return{registered:!!p.identity,id:p.id,name:p.name,tag:p.tag,ready:false,bot:false,avatar:e.avatar||'avatar-sun',frame:e.frame||'frame-classic',entrance:e.entrance||'entrance-classic',rating:progress.rating||1000}};
export const roomFor=(a:Arena,id:string)=>a.rooms.find(r=>r.seats.some(p=>p?.id===id));
export const partyFor=(a:Arena,id:string)=>a.parties.find(p=>p.members.some(m=>m.id===id));
export function ensure(test:unknown,message:string):asserts test{if(!test)throw new Error(message)}
export const duration=(r:RoomState,phase:'draw'|'play')=>(phase==='draw'?(r.drawSeconds||20):(r.playSeconds||30))*1000;
function arm(r:RoomState,at:number){r.phaseStartedAt=at;r.deadline=r.game?.status==='playing'?at+duration(r,r.game.phase):0}
function dealWindow(r:RoomState,now:number){r.dealStartedAt=Math.max(now,r.entranceUntil||0);r.dealUntil=r.dealStartedAt+DEAL_MS;arm(r,r.dealUntil);r.lastBot=r.dealUntil}
export function beginRoom(r:RoomState,seed:number,now:number){ensure(r.seats.length===4&&r.seats.every(p=>p?.ready),'Four ready players are required.');r.game=newGame(r.seats.map(p=>({name:p!.name,tag:p!.tag,bot:p!.bot})),seed,r.rounds,Array.from(crypto.getRandomValues(new Uint32Array(8))));r.events=[];r.entranceUntil=now+ENTRANCE_MS;dealWindow(r,now);r.updated=now}
export function liveRoom(r:RoomState,id:string,now=Date.now()){const seat=r.seats.findIndex(p=>p?.id===id);return{serverNow:now,matchMode:modeOf(r.matchMode),botLevel:r.botLevel||'intermediate',seatPlan:r.seatPlan||'open',entranceUntil:r.entranceUntil||0,id:r.id,code:r.code,name:r.name,host:r.host,seats:r.seats,rounds:r.rounds,timer:r.timer,drawSeconds:r.drawSeconds||20,playSeconds:r.playSeconds||30,phaseStartedAt:r.phaseStartedAt||0,dealStartedAt:r.dealStartedAt||0,dealUntil:r.dealUntil||0,events:r.events||[],allowBots:!!r.allowBots,theme:r.theme,public:r.public,status:r.game?.status||'waiting',messages:r.messages,version:r.game?.version||0,game:seat>=0&&r.game?viewFor(r.game,seat):null,deadline:r.deadline}}
function perform(r:RoomState,seat:number,action:Action,now:number){
 const before=r.game!;const after=applyAction(before,seat,action);r.game=after;
 const event:TableEvent={version:after.version,kind:action.type,seat,at:now};
 if(action.type==='discard'||action.type==='finish')event.tile=after.discards.at(-1)?.tile;
 r.events=[...(r.events||[]).slice(-23),event];
 if(action.type==='next')dealWindow(r,now);
 else if(before.turn!==after.turn||before.phase!==after.phase||before.status!==after.status)arm(r,now);
 r.updated=now;
}
export function moveInRoom(r:RoomState,id:string,version:number,action:Action,now:number){
 ensure(r.game,'The match has not started.');const seat=r.seats.findIndex(m=>m?.id===id);ensure(seat>=0,'You are not seated here.');
 ensure(now>=(r.dealUntil||0),'Dealing tiles. Your turn starts shortly.');
 ensure(r.game.status!=='playing'||now<r.deadline,'Your time expired. Synchronizing the table.');
 ensure(r.game.version===version,'The table changed. Try your move again.');perform(r,seat,action,now);
}
/** Timeout policy: draw from stock; then discard the drawn tile, else the lowest stable tile ID that passes the rules engine. */
export function timeoutAction(g:Game):Action{
 if(g.phase==='draw')return{type:'draw',source:'deck'};
 const tiles=[...g.players[g.turn].hand].sort((a,b)=>a.id===g.drawn?-1:b.id===g.drawn?1:a.id.localeCompare(b.id));
 for(const tile of tiles){const action:Action={type:'discard',tile:tile.id};try{applyAction(g,g.turn,action);return action}catch{}}
 // A special finish-only state must still pass normal rule validation.
 const finish=botAction(viewFor(g,g.turn));applyAction(g,g.turn,finish);return finish;
}
export function tickRoom(r:RoomState,now:number){
 if(!r.game||r.game.status!=='playing'||now<(r.dealUntil||0))return false;
 // Migrate an existing live room to phase timing once, without trusting client input.
 if(!r.phaseStartedAt){arm(r,now);return true}
 let changed=false;
 for(let i=0;i<32&&r.game.status==='playing'&&now>=r.deadline;i++){
  const due=r.deadline;perform(r,r.game.turn,timeoutAction(r.game),due);changed=true;
 }
 if(r.game.status==='playing'&&r.game.players[r.game.turn].bot&&now-r.lastBot>=850&&now<r.deadline){perform(r,r.game.turn,chooseBot(viewFor(r.game,r.game.turn),r.botLevel),now);r.lastBot=now;changed=true}
 return changed;
}
function matchModeQueue(a:Arena,now:number,id:string,code:string,seed:number,mode:MatchMode,blocked:(x:string,y:string)=>boolean){
 a.queue=a.queue.filter(q=>q.members.every(m=>now-(a.presence[m.id]||0)<45000)&&now-q.at<10*60*1000);
 const eligible=a.queue.filter(q=>modeOf(q.matchMode)===mode);let chosen:typeof eligible=[];let teamA:Member[]=[],teamB:Member[]=[];
 // Whole queue entries are indivisible, so a duo can never be split across teams.
 for(let i=0;i<eligible.length;i++){const a1=eligible[i];const firstTeams=a1.members.length===2?[[a1]]:eligible.slice(i+1).filter(q=>q.members.length===1).map(q=>[a1,q]);for(const first of firstTeams){const used=new Set(first.map(q=>q.id));const rest=eligible.filter(q=>!used.has(q.id));const seconds=rest.filter(q=>q.members.length===2).map(q=>[q]);const solos=rest.filter(q=>q.members.length===1);for(let j=0;j<solos.length;j++)for(let k=j+1;k<solos.length;k++)seconds.push([solos[j],solos[k]]);for(const second of seconds){const people=[...first,...second].flatMap(q=>q.members);if(people.some((p,i)=>people.slice(i+1).some(q=>(blocked(p.id,q.id)||blocked(q.id,p.id)))))continue;chosen=[...first,...second];teamA=first.flatMap(q=>q.members);teamB=second.flatMap(q=>q.members);break}if(chosen.length)break}if(chosen.length)break}
 if(!chosen.length){
 const oldest=eligible.find(q=>mode!=='ranked'&&q.allowBots!==false&&now-q.at>=20000);if(!oldest)return null;
 const teams:Member[][]=[[],[]];const candidate=[oldest,...eligible.filter(q=>q!==oldest&&q.allowBots!==false)];
 for(const q of candidate){const people=chosen.flatMap(q=>q.members);if(q.members.some(p=>people.some(m=>blocked(p.id,m.id)||blocked(m.id,p.id))))continue;const team=teams.find(t=>t.length+q.members.length<=2);if(!team)continue;team.push(...q.members);chosen.push(q)}
 teams.forEach((team,k)=>{while(team.length<2){const n=k*2+team.length;team.push({id:`${id}:bot:${n}`,name:['Dara','Roj','Ari','Azad'][n],tag:'BOT',ready:true,bot:true})}});[teamA,teamB]=teams;
 }
 ensure(a.rooms.length<24,'All tables are occupied. Try again shortly.');
 const r:RoomState={id,code,matchMode:mode,botLevel:'intermediate',name:matchModes[mode].name+' · Erbil',host:teamA[0].id,seats:[teamA[0],teamB[0],teamA[1],teamB[1]].map(m=>({...m,ready:true})),rounds:matchModes[mode].rounds,timer:30,drawSeconds:20,playSeconds:30,allowBots:chosen.every(q=>q.allowBots!==false),theme:'royal',public:true,passwordHash:'',game:null,deadline:0,lastBot:now,created:now,updated:now,messages:[]};beginRoom(r,seed,now);a.rooms.push(r);a.queue=a.queue.filter(q=>!chosen.some(c=>c.id===q.id));return r;
}

export function matchQueue(a:Arena,now:number,id:string,code:string,seed:number,blocked:(x:string,y:string)=>boolean=()=>false){for(const mode of [...new Set(a.queue.map(q=>modeOf(q.matchMode)))]){const result=matchModeQueue(a,now,id,code,seed,mode,blocked);if(result)return result}return null}
