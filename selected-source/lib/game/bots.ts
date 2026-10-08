import {botAction,face,isJoker,planHand,points,validateMeld} from './engine.ts';
import type {Action,GameView,Tile} from './engine.ts';
import type {BotLevel} from './modes.ts';
/** All difficulties consume only the same redacted view the player receives. */
export function chooseBot(v:GameView,level:BotLevel='intermediate'):Action{
 const p=v.players[v.seat];
 if(p.eliminated)return v.phase==='draw'?{type:'draw',source:'deck'}:{type:'discard',tile:v.drawn!};
 const base=botAction(v);if(level==='intermediate')return base;
 if(level==='beginner'){
  if(v.phase==='draw')return{type:'draw',source:'deck'};
  if(base.type!=='discard'||v.version%11!==0)return base;
  const tile=[...p.hand].filter(t=>!isJoker(t,v.joker)).sort((a,b)=>a.id.localeCompare(b.id))[v.version%Math.max(1,p.hand.filter(t=>!isJoker(t,v.joker)).length)]||p.hand[0];
  return{type:'discard',tile:tile.id};
 }
 // Replace an exposed joker when legal. The 4-tile group joker is locked.
 if(v.phase==='play'&&(p.opened||p.konkan))for(const meld of v.melds){if(meld.kind==='group'&&meld.tiles.length===4)continue;for(let i=0;i<meld.tiles.length;i++){if(!isJoker(meld.tiles[i],v.joker))continue;const target=meld.faces[i];const replacement=p.hand.find(t=>{const f=face(t,v.joker);return !isJoker(t,v.joker)&&f.n===target.n&&(meld.kind==='run'?f.color===target.color:!meld.faces.some((m,k)=>k!==i&&m.color===f.color))});if(replacement)return{type:'steal',meld:meld.id,tile:replacement.id,joker:meld.tiles[i].id}}}
 if(v.phase==='draw'||base.type!=='discard'||p.konkan)return base;
 const used=new Set(planHand(p.hand,v.joker,0,false).groups.flat());
 const value=(tile:Tile)=>{if(isJoker(tile,v.joker))return 10000;let keep=used.has(tile.id)?100:0;const f=face(tile,v.joker);for(const other of p.hand){if(other.id===tile.id)continue;const n=face(other,v.joker);if(n.n===f.n&&n.color!==f.color)keep+=8;if(n.color===f.color&&Math.abs(n.n-f.n)===1)keep+=7;if(n.color===f.color&&Math.abs(n.n-f.n)===2)keep+=3;}
  if(level==='pro'){
   const next=v.players[(v.seat+1)%4];if(next.opened)for(const m of v.melds.filter(m=>m.owner===(v.seat+1)%4)){if(validateMeld([...m.tiles,tile],v.joker)||validateMeld([tile,...m.tiles],v.joker))keep+=22;}
   const discarded=(v.publicDiscards||[]).filter(d=>{const n=face(d.tile,v.joker);return d.state!=='taken'&&((n.color===f.color&&Math.abs(n.n-f.n)<=2)||(n.n===f.n&&n.color!==f.color))}).length;keep-=discarded*2;
  }return keep-points(f.n)*.1;};
 return{type:'discard',tile:[...p.hand].sort((a,b)=>value(a)-value(b)||a.id.localeCompare(b.id))[0].id};
}
