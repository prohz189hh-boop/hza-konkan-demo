import {KonkanRoom} from './room.ts';
import {Matchmaker} from './matchmaker.ts';
import {verifyUser} from './auth.ts';
import {activeSanctions,insert,patch,rpc,select,memberFromUser} from './supabase.ts';
import {liveKitToken} from './voice.ts';
import {confirmationPage} from './auth-confirmation.ts';
import type {MatchMode} from '../lib/game/modes.ts';

export {KonkanRoom,Matchmaker};

type Env={
 SUPABASE_URL:string;SUPABASE_PUBLISHABLE_KEY:string;SUPABASE_SERVICE_ROLE_KEY?:string;SERVER_VERSION?:string;
 ASSETS:R2Bucket;GAME_ROOMS:DurableObjectNamespace;MATCHMAKER:DurableObjectNamespace;
 LIVEKIT_URL?:string;LIVEKIT_API_KEY?:string;LIVEKIT_API_SECRET?:string;
};
const json=(body:unknown,status=200,extra:HeadersInit={})=>new Response(JSON.stringify(body),{status,headers:{'Content-Type':'application/json','Cache-Control':'no-store','X-Content-Type-Options':'nosniff',...Object.fromEntries(new Headers(extra))}});
async function body(request:Request){try{return await request.json() as Record<string,any>}catch{return{}}}
function protocols(request:Request){return(request.headers.get('Sec-WebSocket-Protocol')||'').split(',').map(x=>x.trim()).filter(Boolean)}
function passwordHashFromProtocols(request:Request){const p=protocols(request).find(x=>x.startsWith('roompw.'));return p?p.slice(7):''}
async function sha(value:string){const d=await crypto.subtle.digest('SHA-256',new TextEncoder().encode(value));return Array.from(new Uint8Array(d)).map(b=>b.toString(16).padStart(2,'0')).join('')}
function code(){return crypto.randomUUID().replaceAll('-','').slice(0,8).toUpperCase()}
function roomPath(path:string){const m=path.match(/^\/room\/([A-Za-z0-9_-]{3,20})$/);return m?m[1].toUpperCase():null}
async function requireUser(env:Env,request:Request,ws=false){const a=await verifyUser(env,request,ws);if(!a)throw Object.assign(new Error('UNAUTHORIZED'),{status:401});return a}
async function profileBundle(env:Env,token:string,id:string){
 const [p,s,w,e,i]=await Promise.all([
  select(env,token,`profiles?id=eq.${id}&select=*`),select(env,token,`player_stats?user_id=eq.${id}&select=*`),select(env,token,`wallets?user_id=eq.${id}&select=*`),select(env,token,`equipped_cosmetics?user_id=eq.${id}&select=*`),select(env,token,`inventories?user_id=eq.${id}&select=cosmetic_id,source,acquired_at`)
 ]);return{profile:p[0]||null,stats:s[0]||null,wallet:w[0]||null,equipped:e,inventory:i};
}
async function partyGroup(env:Env,token:string,user:{id:string;email:string|null;name:string;tag:string;rating:number;avatar?:string;frame?:string;entrance?:string}){
 const memberships=await select(env,token,`party_members?user_id=eq.${user.id}&select=party_id,ready,joined_at`);if(!memberships.length)return{id:user.id,leader:user.id,members:[memberFromUser(user)],party:false};
 const partyId=memberships[0].party_id;const parties=await select(env,token,`parties?id=eq.${partyId}&select=id,leader_id,status`);const party=parties[0];if(!party||party.status!=='open'||party.leader_id!==user.id)throw new Error('ONLY_DUO_LEADER_CAN_QUEUE');
 const members=await select(env,token,`party_members?party_id=eq.${partyId}&select=user_id,ready`);if(members.length!==2||members.some((m:any)=>!m.ready))throw new Error('BOTH_DUO_MEMBERS_MUST_BE_READY');
 const ids=members.map((m:any)=>m.user_id).join(',');const profiles=await select(env,token,`profiles?id=in.(${ids})&select=id,display_name,game_tag,avatar_url`);const stats=await select(env,token,`player_stats?user_id=in.(${ids})&select=user_id,rating`);
 return{id:partyId,leader:user.id,party:true,members:profiles.map((p:any)=>({id:p.id,name:p.display_name,tag:p.game_tag||`P-${p.id.slice(0,6).toUpperCase()}`,ready:true,bot:false,registered:true,rating:Number(stats.find((s:any)=>s.user_id===p.id)?.rating||0),avatar:p.avatar_url||'avatar-sun',frame:'frame-classic',entrance:'entrance-classic'}))};
}

export default{
 async fetch(request:Request,env:Env){
  const url=new URL(request.url),path=url.pathname,method=request.method.toUpperCase();
  try{
   if(path==='/auth/confirmed'&&method==='GET')return confirmationPage();
   if(path==='/health')return json({status:'ready',service:'hza-konkan-multiplayer',version:env.SERVER_VERSION||'2.0.0',supabase:!!env.SUPABASE_URL,r2:!!env.ASSETS,rooms:!!env.GAME_ROOMS,matchmaker:!!env.MATCHMAKER,matchPersistence:!!env.SUPABASE_SERVICE_ROLE_KEY,voice:!!(env.LIVEKIT_URL&&env.LIVEKIT_API_KEY&&env.LIVEKIT_API_SECRET)});

   if(path.startsWith('/assets/')){
    const key=decodeURIComponent(path.slice(8));if(!key||key.includes('..'))return json({error:'INVALID_ASSET'},400);const obj=await env.ASSETS.get(key);if(!obj)return json({error:'NOT_FOUND'},404);const h=new Headers();obj.writeHttpMetadata(h);h.set('etag',obj.httpEtag);h.set('Cache-Control','public,max-age=86400');return new Response(obj.body,{headers:h});
   }

   const room=roomPath(path);
   if(room){
    const stub=env.GAME_ROOMS.get(env.GAME_ROOMS.idFromName(room));
    if(request.headers.get('Upgrade')?.toLowerCase()==='websocket'){
     const a=await requireUser(env,request,true);const sanctions=await activeSanctions(env,a.token);const ban=sanctions.find(x=>x.kind==='account_ban'||x.kind==='matchmaking_ban');if(ban)return json({error:'SANCTIONED',...ban},403);
     const h=new Headers(request.headers);h.set('X-HZA-User-Id',a.user.id);h.set('X-HZA-User-Name',a.user.name);h.set('X-HZA-User-Tag',a.user.tag);h.set('X-HZA-Rating',String(a.user.rating));h.set('X-HZA-Password-Hash',passwordHashFromProtocols(request));
     return stub.fetch(new Request(request.url,{method:'GET',headers:h}));
    }
    return stub.fetch(request);
   }

   if(path==='/api/me'&&method==='GET'){const a=await requireUser(env,request);return json({user:a.user,...await profileBundle(env,a.token,a.user.id)});}
   if(path==='/api/profile'&&method==='PATCH'){const a=await requireUser(env,request),b=await body(request);const update:Record<string,unknown>={};if(typeof b.display_name==='string')update.display_name=b.display_name.trim().slice(0,24);if(typeof b.game_tag==='string')update.game_tag=b.game_tag.trim().toUpperCase().slice(0,20);if(typeof b.avatar_url==='string')update.avatar_url=b.avatar_url.slice(0,500);return json({profile:(await patch(env,a.token,`profiles?id=eq.${a.user.id}`,update))[0]});}

   if(path==='/api/friends'&&method==='GET'){const a=await requireUser(env,request);return json({friends:await select(env,a.token,'friends?select=user_id,friend_id,created_at'),requests:await select(env,a.token,'friend_requests?status=eq.pending&select=*')});}
   if(path==='/api/friends/request'&&method==='POST'){const a=await requireUser(env,request),b=await body(request);return json({id:await rpc(env,a.token,'send_friend_request',{p_receiver:b.user_id})});}
   if(path==='/api/friends/respond'&&method==='POST'){const a=await requireUser(env,request),b=await body(request);return json(await rpc(env,a.token,'respond_friend_request',{p_request_id:b.request_id,p_accept:!!b.accept}));}
   if(path==='/api/friends/remove'&&method==='POST'){const a=await requireUser(env,request),b=await body(request);await rpc(env,a.token,'remove_friend',{p_friend:b.user_id});return json({ok:true});}
   if(path==='/api/block'&&method==='POST'){const a=await requireUser(env,request),b=await body(request);await rpc(env,a.token,b.block===false?'unblock_user':'block_user',{p_user:b.user_id});return json({ok:true});}

   if(path==='/api/party'&&method==='GET'){const a=await requireUser(env,request);return json({parties:await select(env,a.token,'parties?select=*'),members:await select(env,a.token,'party_members?select=*'),invites:await select(env,a.token,'party_invites?status=eq.pending&select=*')});}
   if(path==='/api/party/create'&&method==='POST'){const a=await requireUser(env,request);return json({party_id:await rpc(env,a.token,'create_duo_party')});}
   if(path==='/api/party/invite'&&method==='POST'){const a=await requireUser(env,request),b=await body(request);return json({invite_id:await rpc(env,a.token,'invite_to_party',{p_invitee:b.user_id})});}
   if(path==='/api/party/respond'&&method==='POST'){const a=await requireUser(env,request),b=await body(request);return json(await rpc(env,a.token,'respond_party_invite',{p_invite:b.invite_id,p_accept:!!b.accept}));}
   if(path==='/api/party/ready'&&method==='POST'){const a=await requireUser(env,request),b=await body(request);await rpc(env,a.token,'set_party_ready',{p_ready:!!b.ready});return json({ok:true});}
   if(path==='/api/party/leave'&&method==='POST'){const a=await requireUser(env,request);await rpc(env,a.token,'leave_party');return json({ok:true});}

   if(path==='/api/shop/catalog'&&method==='GET'){const a=await requireUser(env,request);return json({cosmetics:await select(env,a.token,'cosmetics?active=eq.true&select=*')});}
   if(path==='/api/shop/purchase'&&method==='POST'){const a=await requireUser(env,request),b=await body(request);return json(await rpc(env,a.token,'purchase_cosmetic',{p_cosmetic_id:b.cosmetic_id}));}
   if(path==='/api/shop/equip'&&method==='POST'){const a=await requireUser(env,request),b=await body(request);return json(await rpc(env,a.token,'equip_cosmetic',{p_slot:b.slot,p_cosmetic_id:b.cosmetic_id}));}
   if(path==='/api/shop/unequip'&&method==='POST'){const a=await requireUser(env,request),b=await body(request);return json(await rpc(env,a.token,'unequip_cosmetic',{p_slot:b.slot}));}
   if(path==='/api/gifts/send'&&method==='POST'){const a=await requireUser(env,request),b=await body(request);return json({gift_id:await rpc(env,a.token,'send_cosmetic_gift',{p_receiver:b.user_id,p_cosmetic_id:b.cosmetic_id})});}
   if(path==='/api/gifts/claim'&&method==='POST'){const a=await requireUser(env,request),b=await body(request);return json({cosmetic_id:await rpc(env,a.token,'claim_cosmetic_gift',{p_gift_id:b.gift_id})});}
   if(path==='/api/daily/claim'&&method==='POST'){const a=await requireUser(env,request);return json(await rpc(env,a.token,'claim_daily_reward'));}
   if(path==='/api/leaderboard'&&method==='GET'){const a=await requireUser(env,request);return json({ranked:await select(env,a.token,'leaderboard_ranked?select=*&limit=100'),season:await select(env,a.token,'leaderboard_season?select=*&limit=100')});}

   if(path==='/api/notifications'&&method==='GET'){const a=await requireUser(env,request);return json({notifications:await select(env,a.token,'notifications?select=*&order=created_at.desc&limit=100')});}
   if(path==='/api/notifications/read'&&method==='POST'){const a=await requireUser(env,request),b=await body(request);await rpc(env,a.token,'mark_notification_read',{p_notification_id:b.id});return json({ok:true});}
   if(path==='/api/device-token'&&method==='POST'){const a=await requireUser(env,request),b=await body(request);return json({id:await rpc(env,a.token,'register_device_token',{p_platform:b.platform,p_token:b.token})});}
   if(path==='/api/report'&&method==='POST'){const a=await requireUser(env,request),b=await body(request);const rows=await insert(env,a.token,'reports',{reporter_id:a.user.id,reported_user_id:b.user_id,reason:String(b.reason||'').slice(0,80),details:String(b.details||'').slice(0,1000)});return json({report:rows[0]||null});}

   if(path==='/api/rooms/create'&&method==='POST'){
    const a=await requireUser(env,request),b=await body(request),roomCode=code(),roomId=crypto.randomUUID(),pw=typeof b.password==='string'&&b.password?await sha(`room:${b.password}`):'';const stub=env.GAME_ROOMS.get(env.GAME_ROOMS.idFromName(roomCode));const host=memberFromUser(a.user);
    const r=await stub.fetch('https://internal/room/_init',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({id:roomId,code:roomCode,name:b.name,host,mode:b.mode||'regular',botLevel:b.botLevel||'intermediate',seatPlan:b.seatPlan||'open',theme:b.theme||'royal',public:!!b.public,passwordHash:pw,drawSeconds:Number(b.drawSeconds||20),playSeconds:Number(b.playSeconds||30)})});if(!r.ok)throw new Error('ROOM_CREATE_FAILED');return json({room:await r.json()});
   }

   if(path==='/api/matchmaking/join'&&method==='POST'){
    const a=await requireUser(env,request),b=await body(request),mode=(['regular','turbo','ranked'].includes(b.mode)?b.mode:'regular') as MatchMode;const sanctions=await activeSanctions(env,a.token);if(sanctions.some(x=>x.kind==='account_ban'||x.kind==='matchmaking_ban'))return json({error:'SANCTIONED'},403);const group=await partyGroup(env,a.token,a.user);if(mode==='ranked'&&group.members.some((m:any)=>!m.registered))return json({error:'RANKED_REQUIRES_REGISTERED_PLAYERS'},400);
    if(b.queueTicket!==undefined&&(typeof b.queueTicket!=='string'||!/^[A-Za-z0-9-]{16,64}$/.test(b.queueTicket)))return json({error:'INVALID_QUEUE_TICKET'},400);
    const mm=env.MATCHMAKER.get(env.MATCHMAKER.idFromName('global'));const r=await mm.fetch('https://internal/matchmaker/join',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({group:{id:group.id,leader:group.leader,members:group.members,mode,region:String(b.region||'auto').slice(0,32),at:Date.now(),allowBots:b.allowBots!==false,queueTicket:b.queueTicket,ticketOwner:a.user.id}})});return new Response(r.body,{status:r.status,headers:r.headers});
   }
   if(path==='/api/matchmaking/leave'&&method==='POST'){const a=await requireUser(env,request),b=await body(request);if(b.queueTicket!==undefined&&(typeof b.queueTicket!=='string'||!/^[A-Za-z0-9-]{16,64}$/.test(b.queueTicket)))return json({error:'INVALID_QUEUE_TICKET'},400);const mm=env.MATCHMAKER.get(env.MATCHMAKER.idFromName('global'));return mm.fetch('https://internal/matchmaker/leave',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({userId:a.user.id,queueTicket:b.queueTicket})});}
   if(path==='/api/matchmaking/status'&&method==='GET'){const a=await requireUser(env,request);const mm=env.MATCHMAKER.get(env.MATCHMAKER.idFromName('global'));return mm.fetch(`https://internal/matchmaker/status?userId=${encodeURIComponent(a.user.id)}`);}

   if(path==='/api/voice/token'&&method==='POST'){
    const a=await requireUser(env,request),b=await body(request),roomCode=String(b.room||'').toUpperCase();
    if(!/^[A-Z0-9_-]{3,20}$/.test(roomCode))return json({error:'INVALID_ROOM'},400);
    const sanctions=await activeSanctions(env,a.token);
    if(sanctions.some(s=>s.kind==='account_ban'||s.kind==='matchmaking_ban'))return json({error:'SANCTIONED'},403);
    const room=env.GAME_ROOMS.get(env.GAME_ROOMS.idFromName(roomCode));
    const membership=await room.fetch('https://internal/_membership',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({userId:a.user.id})});
    if(!membership.ok)return json({error:'NOT_ROOM_MEMBER'},403);
    return json(await liveKitToken(env,a.user.id,a.user.name,roomCode));
   }

   return json({name:'HZA KONKAN Backend',version:env.SERVER_VERSION||'2.0.0',status:'online'},404);
  }catch(e:any){const status=Number(e?.status||400);const message=e instanceof Error?e.message:'REQUEST_FAILED';return json({error:message},status);}
 }
}
