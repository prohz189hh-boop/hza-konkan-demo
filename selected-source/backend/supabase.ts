import type {VerifiedUser} from './auth';

type Env={SUPABASE_URL:string;SUPABASE_PUBLISHABLE_KEY:string;SUPABASE_SERVICE_ROLE_KEY?:string;SERVER_VERSION?:string};
const userHeaders=(env:Env,token:string)=>({apikey:env.SUPABASE_PUBLISHABLE_KEY,Authorization:`Bearer ${token}`,'Content-Type':'application/json'});
const serviceHeaders=(env:Env)=>env.SUPABASE_SERVICE_ROLE_KEY?{apikey:env.SUPABASE_SERVICE_ROLE_KEY,Authorization:`Bearer ${env.SUPABASE_SERVICE_ROLE_KEY}`,'Content-Type':'application/json'}:null;
export async function rpc(env:Env,token:string,name:string,args:Record<string,unknown>={}){
 const r=await fetch(`${env.SUPABASE_URL}/rest/v1/rpc/${name}`,{method:'POST',headers:userHeaders(env,token),body:JSON.stringify(args)});
 const text=await r.text();if(!r.ok)throw new Error(text||`${name}_failed`);return text?JSON.parse(text):null;
}
type Row=Record<string,any>;
async function rows(response:Response):Promise<Row[]>{
 const value:unknown=await response.json();
 if(!Array.isArray(value)||value.some(row=>!row||typeof row!=='object'||Array.isArray(row)))throw new Error('INVALID_DATABASE_RESPONSE');
 return value;
}
export async function select(env:Env,token:string,path:string){
 const r=await fetch(`${env.SUPABASE_URL}/rest/v1/${path}`,{headers:userHeaders(env,token)});if(!r.ok)throw new Error('supabase_read_failed');return rows(r);
}
export async function patch(env:Env,token:string,path:string,body:unknown){
 const r=await fetch(`${env.SUPABASE_URL}/rest/v1/${path}`,{method:'PATCH',headers:{...userHeaders(env,token),Prefer:'return=representation'},body:JSON.stringify(body)});if(!r.ok)throw new Error('supabase_patch_failed');return rows(r);
}
export async function insert(env:Env,token:string,path:string,body:unknown){
 const r=await fetch(`${env.SUPABASE_URL}/rest/v1/${path}`,{method:'POST',headers:{...userHeaders(env,token),Prefer:'return=representation'},body:JSON.stringify(body)});if(!r.ok)throw new Error('supabase_insert_failed');return rows(r);
}
export async function activeSanctions(env:Env,token:string){
 const rows=await select(env,token,'player_sanctions?select=kind,reason,expires_at&active=eq.true') as Array<{kind:string;reason:string;expires_at:string|null}>;
 const now=Date.now();return rows.filter(x=>!x.expires_at||Date.parse(x.expires_at)>now);
}
export async function createMatch(env:Env,room:{id:string;mode:string;captain:string|null;players:{id:string;team:number;seat:number;bot:boolean}[]}){
 const h=serviceHeaders(env);if(!h)return null;
 const res=await fetch(`${env.SUPABASE_URL}/rest/v1/matches?select=id`,{method:'POST',headers:{...h,Prefer:'return=representation'},body:JSON.stringify({room_id:null,mode:room.mode,status:'active',captain_user_id:room.captain,server_version:env.SERVER_VERSION||'2.0.0',started_at:new Date().toISOString()})});
 if(!res.ok)return null;const rows=await res.json() as Array<{id:string}>;const id=rows[0]?.id;if(!id)return null;
 const humans=room.players.filter(p=>!p.bot).map(p=>({match_id:id,user_id:p.id,team:p.team,seat:p.seat,score:0,winner:false}));
 if(humans.length)await fetch(`${env.SUPABASE_URL}/rest/v1/match_players`,{method:'POST',headers:h,body:JSON.stringify(humans)});
 return id;
}
export async function settleMatch(env:Env,matchId:string|null,winnerTeam:number,roundCount:number,players:{id:string;team:number;bot:boolean}[],scores:[number,number],ranked:boolean){
 const h=serviceHeaders(env);if(!h||!matchId)return false;
 const results=players.filter(p=>!p.bot).map(p=>({user_id:p.id,winner:p.team===winnerTeam,score:scores[p.team-1]||0,rating_delta:ranked?(p.team===winnerTeam?25:-20):0}));
 const r=await fetch(`${env.SUPABASE_URL}/rest/v1/rpc/settle_match`,{method:'POST',headers:h,body:JSON.stringify({p_match_id:matchId,p_winner_team:winnerTeam,p_round_count:roundCount,p_results:results})});return r.ok;
}
export async function notification(env:Env,userId:string,type:string,title:string,body:string,data:unknown={}){
 const h=serviceHeaders(env);if(!h)return false;const r=await fetch(`${env.SUPABASE_URL}/rest/v1/notifications`,{method:'POST',headers:h,body:JSON.stringify({user_id:userId,type,title,body,data})});return r.ok;
}
export function memberFromUser(user:VerifiedUser){return{id:user.id,name:user.name,tag:user.tag,ready:false,bot:false,registered:true,rating:user.rating,avatar:user.avatar||'avatar-sun',frame:user.frame||'frame-classic',entrance:user.entrance||'entrance-classic'};}
