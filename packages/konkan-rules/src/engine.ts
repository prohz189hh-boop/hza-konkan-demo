/** Pure, deterministic rules. See ../RULES.md for explicit v1 interpretations. */
export type Color='red'|'black'|'blue'|'yellow';
export const COLORS:Color[]=['red','black','blue','yellow'];
export type Tile={id:string,color:Color,n:number,antique?:boolean};
export type Face={color:Color,n:number};
export type Meld={id:string,owner:number,tiles:Tile[],faces:Face[],kind:'group'|'run',score:number,lockedStart?:boolean,lockedEnd?:boolean};
export type Player={name:string,tag:string,bot:boolean,hand:Tile[],opened:boolean,opening:number,konkan:boolean,eliminated:boolean,locks:number,stolen:string[],konkanPicks:number};
export type PublicDiscard={id:string,tile:Tile,seat:number,order:number,state:'pit'|'taken'|'recycled'};
export type Game={publicDiscards?:PublicDiscard[],dealNumber?:number,version:number,seed:number,entropy?:number[],players:Player[],cup:Tile,joker:Face,deck:Tile[],discards:{tile:Tile,seat:number,konkan:boolean,blocked:boolean}[],melds:Meld[],captain:number,turn:number,phase:'draw'|'play',drawn:string|null,requiredOpen:boolean,mustFinish:boolean,round:number,rounds:number,reshuffles:number,scores:[number,number],status:'playing'|'roundEnd'|'finished',winner:number|null,roundPoints:number,reason:string,history:string[],results:{round:number,winner:number,points:number,kind:string}[]};
export type GameView=Omit<Game,'seed'|'entropy'|'deck'|'players'> & {deckCount:number,players:(Omit<Player,'hand'> & {hand:Tile[],tileCount:number})[],seat:number,openingRequired:number};
export type Action={type:'draw',source:'deck'|'discard',enterKonkan?:boolean}|{type:'discard',tile:string}|{type:'meld',groups:string[][]}|{type:'extend',meld:string,tile:string,side:'start'|'end'}|{type:'steal',meld:string,tile:string,joker:string}|{type:'finish',groups:string[][],discard:string}|{type:'next'};
export class RuleError extends Error{code:string;constructor(code:string){super(code);this.code=code}}
function requireRule(ok:unknown,code:string):asserts ok{if(!ok)throw new RuleError(code)}
export const points=(n:number)=>n===1||n>=10?10:n;
export function createTiles():Tile[]{return [...COLORS.flatMap(color=>Array.from({length:13},(_,i)=>[0,1].map(copy=>({id:`${color}-${i+1}-${copy}`,color,n:i+1}))).flat()),{id:'antique-0',color:'black' as Color,n:0,antique:true},{id:'antique-1',color:'black' as Color,n:0,antique:true}]}
export function jokerFor(cup:Tile):Face{return{color:cup.color,n:cup.n===1?13:cup.n-1}}
export const isJoker=(t:Tile,j:Face)=>!t.antique&&t.color===j.color&&t.n===j.n;
export const face=(t:Tile,j:Face):Face=>t.antique?j:{color:t.color,n:t.n};
function rng(g:{seed:number,entropy?:number[]}){if(g.entropy){const s=[0x61707865,0x3320646e,0x79622d32,0x6b206574,...g.entropy,g.seed++,0,0,0],x=[...s];const rot=(v:number,n:number)=>(v<<n)|(v>>>(32-n));const q=(a:number,b:number,c:number,d:number)=>{x[a]=(x[a]+x[b])|0;x[d]=rot(x[d]^x[a],16);x[c]=(x[c]+x[d])|0;x[b]=rot(x[b]^x[c],12);x[a]=(x[a]+x[b])|0;x[d]=rot(x[d]^x[a],8);x[c]=(x[c]+x[d])|0;x[b]=rot(x[b]^x[c],7)};for(let i=0;i<10;i++){q(0,4,8,12);q(1,5,9,13);q(2,6,10,14);q(3,7,11,15);q(0,5,10,15);q(1,6,11,12);q(2,7,8,13);q(3,4,9,14)}return((x[0]+s[0])>>>0)/4294967296}let x=g.seed|0;x^=x<<13;x^=x>>>17;x^=x<<5;g.seed=x>>>0;return g.seed/4294967296}
function shuffle<T>(items:T[],g:{seed:number}){const a=[...items];for(let i=a.length-1;i>0;i--){const k=Math.floor(rng(g)*(i+1));[a[i],a[k]]=[a[k],a[i]]}return a}
export function validateMeld(tiles:Tile[],j:Face,large=false):Omit<Meld,'id'|'owner'>|null{
 if(tiles.length<3||tiles.length>(large?10:5)||new Set(tiles.map(t=>t.id)).size!==tiles.length)return null;
 const natural=tiles.filter(t=>!isJoker(t,j));const wild=tiles.filter(t=>isJoker(t,j));if(wild.length>2||(tiles.length===3&&wild.length===2)||!natural.length)return null;
 const ns=natural.map(t=>face(t,j));
 if(!large&&tiles.length<=4&&ns.every(t=>t.n===ns[0].n)&&new Set(ns.map(t=>t.color)).size===ns.length){
  const missing=COLORS.filter(c=>!ns.some(t=>t.color===c));const pairs=natural.map(t=>({t,f:face(t,j)})).concat(wild.map((t,i)=>({t,f:{color:missing[i],n:ns[0].n}}))).sort((a,b)=>COLORS.indexOf(a.f.color)-COLORS.indexOf(b.f.color));
  return{tiles:pairs.map(p=>p.t),faces:pairs.map(p=>p.f),kind:'group',score:points(ns[0].n)*tiles.length};
 }
 if(!ns.every(t=>t.color===ns[0].color)||new Set(ns.map(t=>t.n)).size!==ns.length)return null;
 const candidates:Omit<Meld,'id'|'owner'>[]=[];
 for(let start=1;start+tiles.length-1<=14;start++){
  const seq=Array.from({length:tiles.length},(_,i)=>start+i===14?1:start+i);
  if(!ns.every(t=>seq.includes(t.n)))continue;
  let wi=0;const ordered=seq.map(n=>natural.find(t=>face(t,j).n===n)||wild[wi++]);
  if(ordered.some(t=>!t))continue;
  candidates.push({tiles:ordered,faces:seq.map(n=>({color:ns[0].color,n})),kind:'run',score:seq.reduce((s,n)=>s+points(n),0)});
 }
 if(wild.length===2&&candidates.length!==1)return null;
 return candidates.sort((a,b)=>b.score-a.score)[0]||null;
}
export function openingRequired(g:Pick<Game,'players'>,seat:number){
 const mate=g.players[(seat+2)%4],opps=g.players.filter((_,i)=>i%2!==seat%2),opened=opps.filter(p=>p.opened);
 if(opened.length===2)return Math.max(81,...opened.map(p=>p.opening+1));
 if(mate.opened)return 61;
 if(opened.length)return Math.max(81,opened[0].opening+1);
 return 81;
}
function log(g:Game,text:string){g.history=[...g.history.slice(-59),text]}
function deal(g:Game){
 g.dealNumber=(g.dealNumber||0)+1;
 let deck=shuffle(createTiles(),g);g.players.forEach((p,i)=>{p.hand=deck.splice(0,i===g.captain?15:14);p.opened=false;p.opening=0;p.konkan=false;p.eliminated=false;p.locks=0;p.stolen=[];p.konkanPicks=0});
 // An Antique cannot define a color/number. Choose the first numbered stock tile, leaving antiques in stock.
 const ci=deck.findIndex(t=>!t.antique);g.cup=deck.splice(ci,1)[0];g.joker=jokerFor(g.cup);g.deck=deck;g.discards=[];g.publicDiscards=[];g.melds=[];g.turn=g.captain;g.phase='play';g.drawn=null;g.requiredOpen=false;g.mustFinish=false;g.reshuffles=0;g.status='playing';g.winner=null;g.roundPoints=0;g.reason='';log(g,`ROUND|${g.round}|${g.players[g.captain].name}`);
}
export function newGame(names:{name:string,tag?:string,bot?:boolean}[],seed=1,rounds=3,entropy?:number[]):Game{
 requireRule(names.length===4,'FOUR_PLAYERS');const g:Game={version:0,seed:seed||1,entropy,players:names.map(p=>({...p,tag:p.tag||'BOT',bot:!!p.bot,hand:[],opened:false,opening:0,konkan:false,eliminated:false,locks:0,stolen:[],konkanPicks:0})),cup:null as unknown as Tile,joker:{n:1,color:'red'},deck:[],discards:[],melds:[],captain:0,turn:0,phase:'play',drawn:null,requiredOpen:false,mustFinish:false,round:1,rounds,reshuffles:0,scores:[0,0],status:'playing',winner:null,roundPoints:0,reason:'',history:[],results:[]};g.captain=Math.floor(rng(g)*4);deal(g);return g;
}
function lookup(p:Player,id:string){const t=p.hand.find(t=>t.id===id);requireRule(t,'TILE_NOT_OWNED');return t}
function takeGroups(g:Game,p:Player,ids:string[][],large=false){requireRule(ids.length>0&&ids.length<=5,'INVALID_MELD');const flat=ids.flat();requireRule(new Set(flat).size===flat.length,'DUPLICATE_TILE');return ids.map(ids=>{const m=validateMeld(ids.map(id=>lookup(p,id)),g.joker,large&&ids.length===10);requireRule(m,'INVALID_MELD');return m})}
function removeTiles(p:Player,ids:string[]){p.hand=p.hand.filter(t=>!ids.includes(t.id))}
export function penalty(p:Player,j:Face,konkan=false){if(!p.opened)return konkan?200:100;return(konkan?100:0)+p.hand.reduce((s,t)=>s+(isJoker(t,j)?p.stolen.includes(t.id)?2:25:points(face(t,j).n)),0)}
function completeRound(g:Game,seat:number,kind:string){g.winner=seat;g.roundPoints=g.players.reduce((s,p,i)=>s+(i%2===seat%2?0:penalty(p,g.joker,kind==='konkan')),0);g.scores[seat%2]+=g.roundPoints;g.reason=kind;g.results.push({round:g.round,winner:seat,points:g.roundPoints,kind});g.status=g.round>=g.rounds?'finished':'roundEnd';log(g,`WIN|${g.players[seat].name}|${g.roundPoints}|${kind}`)}
function cancelRound(g:Game){g.status='roundEnd';g.winner=null;g.roundPoints=0;g.reason='cancelled';log(g,'CANCELLED')}
function replenish(g:Game){if(g.deck.length)return;if(g.reshuffles>=2||g.discards.length<=1){cancelRound(g);return}const last=g.discards.pop()!;g.deck=shuffle(g.discards.map(d=>d.tile),g);for(const d of g.publicDiscards||[])if(d.state==='pit'&&d.tile.id!==last.tile.id)d.state='recycled';g.discards=[last];g.reshuffles++;log(g,`RESHUFFLE|${g.reshuffles}`)}
function recordDiscard(g:Game,seat:number,tile:Tile){g.publicDiscards=[...(g.publicDiscards||[]),{id:`${g.dealNumber||g.round}:${g.version}:${seat}`,tile,seat,order:(g.publicDiscards?.length||0)+1,state:'pit'}]}
function elimination(g:Game,p:Player,reason:string){p.eliminated=true;log(g,`ELIMINATED|${p.name}|${reason}`)}
export function applyAction(input:Game,seat:number,a:Action):Game{
 const g:Game=structuredClone(input);requireRule(Number.isInteger(seat)&&seat>=0&&seat<4,'INVALID_SEAT');const p=g.players[seat];
 if(a.type==='next'){requireRule(g.status==='roundEnd','ROUND_NOT_OVER');if(g.winner!==null){g.captain=g.winner;g.round++}deal(g);g.version++;return g}
 requireRule(g.status==='playing','ROUND_OVER');requireRule(g.turn===seat,'NOT_YOUR_TURN');
 if(a.type==='draw'){
  requireRule(g.phase==='draw','ALREADY_DRAWN');
  if(a.source==='deck'){
   replenish(g);if(g.status!=='playing'){g.version++;return g}const tile=g.deck.shift()!;p.hand.push(tile);g.drawn=tile.id;log(g,`DRAW|${p.name}`);
  }else{
   requireRule(!p.eliminated,'ELIMINATED');const last=g.discards.at(-1);requireRule(last&&!last.blocked&&last.seat%2!==seat%2,'DISCARD_UNAVAILABLE');
   if(!p.opened&&!p.konkan){if(a.enterKonkan)p.konkan=true;else g.requiredOpen=true}
   if(last.konkan&&!p.konkan){p.konkanPicks++;if(p.konkanPicks>=2)g.mustFinish=true}
   p.hand.push(last.tile);g.drawn=last.tile.id;const record=(g.publicDiscards||[]).findLast(d=>d.tile.id===last.tile.id&&d.state==='pit');if(record)record.state='taken';g.discards.pop();log(g,`TAKE|${p.name}`);
  }g.phase='play';
 }else{
  requireRule(g.phase==='play','DRAW_FIRST');
  if(a.type!=='discard')requireRule(!p.eliminated,'ELIMINATED');
  if(a.type==='meld'){
   requireRule(!p.konkan,'KONKAN_FINISH_ONLY');const ms=takeGroups(g,p,a.groups);const ids=a.groups.flat();requireRule(p.hand.length>ids.length,'KEEP_DISCARD');const score=ms.reduce((s,m)=>s+m.score,0);
   if(!p.opened){requireRule(score>=openingRequired(g,seat),'OPENING_TOO_LOW');p.opened=true;p.opening=score;g.requiredOpen=false;log(g,`OPEN|${p.name}|${score}`)}
   ms.forEach(m=>g.melds.push({...m,id:`m-${g.version}-${g.melds.length}`,owner:seat}));removeTiles(p,ids);
  }else if(a.type==='extend'){
   const m=g.melds.find(m=>m.id===a.meld);requireRule(m,'MELD_NOT_FOUND');const t=lookup(p,a.tile);requireRule(p.hand.length>1,'KEEP_DISCARD');requireRule(!(a.side==='start'?m.lockedStart:m.lockedEnd),'SIDE_LOCKED');
   const tiles=a.side==='start'?[t,...m.tiles]:[...m.tiles,t];const mm=validateMeld(tiles,g.joker);requireRule(mm&&mm.kind===m.kind,'INVALID_EXTENSION');
   // Preserve every previously assigned joker and natural face.
   requireRule(m.tiles.every((tile,i)=>{const k=mm.tiles.findIndex(x=>x.id===tile.id);return mm.faces[k].n===m.faces[i].n&&mm.faces[k].color===m.faces[i].color}),'JOKER_POSITION_FIXED');
   if(m.kind==='run')requireRule(a.side==='start'?mm.tiles[0].id===t.id:mm.tiles.at(-1)!.id===t.id,'INVALID_EXTENSION');
   if(!p.opened){if(p.locks>=1){elimination(g,p,'SECOND_LOCK');g.version++;return g}p.konkan=true;p.locks++;if(a.side==='start')m.lockedStart=true;else m.lockedEnd=true;log(g,`KONKAN|${p.name}`)}
   Object.assign(m,mm);removeTiles(p,[a.tile]);
  }else if(a.type==='steal'){
   requireRule(p.opened||p.konkan,'OPEN_FIRST');const m=g.melds.find(m=>m.id===a.meld);requireRule(m,'MELD_NOT_FOUND');const ji=m.tiles.findIndex(t=>t.id===a.joker&&isJoker(t,g.joker));requireRule(ji>=0,'NOT_A_JOKER');requireRule(!(m.kind==='group'&&m.tiles.length===4),'DEAD_JOKER');const t=lookup(p,a.tile);requireRule(!isJoker(t,g.joker),'EXACT_REPLACEMENT');const f=face(t,g.joker);
   if(m.kind==='run')requireRule(f.n===m.faces[ji].n&&f.color===m.faces[ji].color,'EXACT_REPLACEMENT');else requireRule(f.n===m.faces[ji].n&&!m.tiles.some((x,i)=>i!==ji&&!isJoker(x,g.joker)&&face(x,g.joker).color===f.color),'EXACT_REPLACEMENT');
   const joker=m.tiles[ji];m.tiles[ji]=t;m.faces[ji]=f;removeTiles(p,[t.id]);p.hand.push(joker);if(!p.stolen.includes(joker.id))p.stolen.push(joker.id);log(g,`STEAL|${p.name}`);
  }else if(a.type==='finish'){
   const discard=lookup(p,a.discard);requireRule(!a.groups.flat().includes(a.discard),'KEEP_DISCARD');const ms=takeGroups(g,p,a.groups,p.konkan);requireRule(a.groups.flat().length===p.hand.length-1,'INCOMPLETE_FINISH');
   if(p.konkan){requireRule(ms.length===2&&ms.some(m=>m.kind==='run'&&m.tiles.length===10),'KONKAN_NEEDS_TEN');const other=ms.find(m=>m.tiles.length!==10)!;requireRule((other.tiles.length===4&&p.locks===0)||(other.tiles.length===3&&p.locks===1),'KONKAN_NEEDS_LOCK')}
   ms.forEach(m=>g.melds.push({...m,id:`m-${g.version}-${g.melds.length}`,owner:seat}));p.hand=[];g.discards.push({tile:discard,seat,konkan:p.konkan,blocked:false});recordDiscard(g,seat,discard);completeRound(g,seat,p.konkan?'konkan':p.opened?'ordinary':'concealed');
  }else if(a.type==='discard'){
   const tile=lookup(p,a.tile);if(p.eliminated)requireRule(tile.id===g.drawn,'ELIMINATED_DRAW_ONLY');
   let justEliminated=false;if(g.requiredOpen&&!p.opened){elimination(g,p,'OPENING_REQUIRED');justEliminated=true}if(g.mustFinish&&p.hand.length!==1){elimination(g,p,'SECOND_KONKAN_PICK');justEliminated=true}
   requireRule(!p.konkan||p.hand.length>1,'KONKAN_FINISH_ONLY');removeTiles(p,[a.tile]);g.discards.push({tile,seat,konkan:p.konkan&&!p.eliminated,blocked:justEliminated});recordDiscard(g,seat,tile);log(g,`DISCARD|${p.name}|${tile.antique?'antique':tile.n}|${tile.color}`);
   if(p.hand.length===0&&!p.eliminated){completeRound(g,seat,'ordinary')}else{g.turn=(seat+1)%4;g.phase='draw';g.drawn=null;g.requiredOpen=false;g.mustFinish=false}
  }else throw new RuleError('UNKNOWN_ACTION');
 }g.version++;return g;
}
export function viewFor(g:Game,seat:number):GameView{const{seed,entropy,deck,players,...rest}=g;return{...rest,drawn:seat===g.turn?g.drawn:null,deckCount:deck.length,players:players.map((p,i)=>({...p,hand:i===seat?p.hand:[],stolen:i===seat?p.stolen:[],tileCount:p.hand.length})),seat,openingRequired:openingRequired(g,seat)}}
/** Set packing over only this player's rack. No stock or opponent hands enter planning. */
export function planHand(hand:Tile[],j:Face,minScore=0,keepDiscard=true){
 const candidates:{ids:string[],mask:number,score:number}[]=[];
 function collect(start:number,chosen:number[]){if(chosen.length>=3){const m=validateMeld(chosen.map(i=>hand[i]),j);if(m)candidates.push({ids:m.tiles.map(t=>t.id),mask:chosen.reduce((s,i)=>s|(1<<i),0),score:m.score})}if(chosen.length===5)return;for(let i=start;i<hand.length;i++)collect(i+1,[...chosen,i])}
 collect(0,[]);const byFirst=hand.map((_,i)=>candidates.filter(c=>c.mask&(1<<i)));const memo=new Map<number,{score:number,count:number,groups:string[][]}>();
 function solve(mask:number):{score:number,count:number,groups:string[][]}{if(!mask)return{score:0,count:0,groups:[]};if(memo.has(mask))return memo.get(mask)!;let i=0;while(!(mask&(1<<i)))i++;let best=solve(mask&~(1<<i));for(const c of byFirst[i])if((c.mask&mask)===c.mask){const rest=solve(mask^c.mask);const next={score:rest.score+c.score,count:rest.count+c.ids.length,groups:[c.ids,...rest.groups]};if(next.score>best.score||(next.score===best.score&&next.count>best.count))best=next}memo.set(mask,best);return best}
 if(!keepDiscard)return solve((1<<hand.length)-1);
 // Keep at least one tile for the required final discard.
 let best={score:0,count:0,groups:[] as string[][]};for(let i=0;i<hand.length;i++){const p=solve(((1<<hand.length)-1)^(1<<i));if(p.score>=minScore&&(p.score>best.score||(p.score===best.score&&p.count>best.count)))best=p}return best;
}
export function botAction(v:GameView):Action{
 const p=v.players[v.seat];if(v.phase==='draw'){
  const last=v.discards.at(-1);if(last&&!last.blocked&&last.seat%2!==v.seat%2&&!p.eliminated&&!last.konkan){const withTile=planHand([...p.hand,last.tile],v.joker,p.opened?0:v.openingRequired),without=planHand(p.hand,v.joker,0);if(withTile.score>without.score&&withTile.score>=(p.opened?0:v.openingRequired))return{type:'draw',source:'discard'}}
  return{type:'draw',source:'deck'};
 }
 if(p.eliminated)return{type:'discard',tile:v.drawn!};
 const plan=planHand(p.hand,v.joker,p.opened?0:v.openingRequired);if(plan.count===p.hand.length-1&&plan.count>0)return{type:'finish',groups:plan.groups,discard:p.hand.find(t=>!plan.groups.flat().includes(t.id))!.id};
 if(plan.groups.length&&!p.konkan)return{type:'meld',groups:plan.groups};
 if(p.opened)for(const m of v.melds)for(const tile of p.hand)for(const side of ['start','end'] as const){if(side==='start'?m.lockedStart:m.lockedEnd)continue;const mm=validateMeld(side==='start'?[tile,...m.tiles]:[...m.tiles,tile],v.joker);if(mm&&mm.kind===m.kind&&p.hand.length>1&&m.tiles.every((x,i)=>{const k=mm.tiles.findIndex(y=>y.id===x.id);return mm.faces[k].n===m.faces[i].n&&mm.faces[k].color===m.faces[i].color})&&(m.kind==='group'||(side==='start'?mm.tiles[0].id===tile.id:mm.tiles.at(-1)!.id===tile.id)))return{type:'extend',meld:m.id,tile:tile.id,side}}
 let discard=p.hand[0],lowest=Infinity;for(const tile of p.hand){let value=isJoker(tile,v.joker)?1000:0;const f=face(tile,v.joker);for(const t of p.hand)if(t.id!==tile.id){const ff=face(t,v.joker);if(ff.n===f.n&&ff.color!==f.color)value+=5;if(ff.color===f.color&&Math.abs(ff.n-f.n)<=2&&ff.n!==f.n)value+=Math.abs(ff.n-f.n)===1?4:2}value-=points(f.n)*.06;if(value<lowest){lowest=value;discard=tile}}
 return{type:'discard',tile:discard.id};
}
export const errors:Record<string,string>={NOT_YOUR_TURN:'Wait for your turn.',DRAW_FIRST:'Draw a tile first.',ALREADY_DRAWN:'You already drew. Play, then discard.',INVALID_MELD:'Use a same-color run or a same-number group of different colors.',DUPLICATE_TILE:'A tile can only be used once.',KEEP_DISCARD:'Keep one tile to discard.',OPENING_TOO_LOW:'Your groups do not reach the opening score.',DISCARD_UNAVAILABLE:'That discard cannot be taken.',OPEN_FIRST:'Open your hand before stealing a joker.',DEAD_JOKER:'A joker in a four-tile group is locked.',EXACT_REPLACEMENT:'Use the exact tile the joker represents.',SIDE_LOCKED:'That end of the meld is locked.',INVALID_EXTENSION:'That tile does not extend this meld.',JOKER_POSITION_FIXED:'An extension cannot change an existing joker’s value.',ELIMINATED:'You are out this round. Draw and discard that tile.',ELIMINATED_DRAW_ONLY:'Discard the tile you just drew.',KONKAN_FINISH_ONLY:'In Konkan mode, keep your groups in hand and use Finish.',KONKAN_NEEDS_TEN:'Konkan needs a ten-tile run and a separate group.',KONKAN_NEEDS_LOCK:'Use a four-tile group, or a three-tile group with one locked tile.',INCOMPLETE_FINISH:'Every tile except your discard must belong to a valid group.',ROUND_OVER:'This round has ended.',TILE_NOT_OWNED:'That tile is not in your hand.',ROUND_NOT_OVER:'Finish this round first.'};
