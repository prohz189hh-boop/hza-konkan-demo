export type VerifiedUser={id:string;email:string|null;name:string;tag:string;rating:number;avatar?:string;frame?:string;entrance?:string};

function protocolToken(request:Request){
 const raw=request.headers.get('Sec-WebSocket-Protocol')||'';
 for(const protocol of raw.split(',').map(x=>x.trim()))if(protocol.startsWith('auth.'))return protocol.slice(5);
 return null;
}
export function accessToken(request:Request,websocket=false){
 if(websocket)return protocolToken(request);
 const h=request.headers.get('Authorization')||'';
 return h.toLowerCase().startsWith('bearer ')?h.slice(7).trim():null;
}
async function supabaseUser(env:EnvLike,token:string){
 const r=await fetch(`${env.SUPABASE_URL}/auth/v1/user`,{headers:{apikey:env.SUPABASE_PUBLISHABLE_KEY,Authorization:`Bearer ${token}`}});
 if(!r.ok)return null;return r.json() as Promise<{id:string;email?:string;user_metadata?:Record<string,unknown>}>;
}
async function profile(env:EnvLike,token:string,id:string){
 const r=await fetch(`${env.SUPABASE_URL}/rest/v1/profiles?id=eq.${encodeURIComponent(id)}&select=id,display_name,game_tag,avatar_url`,{headers:{apikey:env.SUPABASE_PUBLISHABLE_KEY,Authorization:`Bearer ${token}`}});
 if(!r.ok)return null;const rows=await r.json() as Array<{id:string;display_name:string;game_tag:string|null;avatar_url:string|null}>;return rows[0]||null;
}
async function stats(env:EnvLike,token:string,id:string){
 const r=await fetch(`${env.SUPABASE_URL}/rest/v1/player_stats?user_id=eq.${encodeURIComponent(id)}&select=rating`,{headers:{apikey:env.SUPABASE_PUBLISHABLE_KEY,Authorization:`Bearer ${token}`}});
 if(!r.ok)return 0;const rows=await r.json() as Array<{rating:number}>;return Number(rows[0]?.rating||0);
}
export async function verifyUser(env:EnvLike,request:Request,websocket=false):Promise<{user:VerifiedUser;token:string}|null>{
 const token=accessToken(request,websocket);if(!token)return null;
 const u=await supabaseUser(env,token);if(!u?.id)return null;
 const [p,rating]=await Promise.all([profile(env,token,u.id),stats(env,token,u.id)]);
 const rawName=String(u.user_metadata?.name||u.user_metadata?.full_name||u.email?.split('@')[0]||'Player');
 return{token,user:{id:u.id,email:u.email||null,name:p?.display_name||rawName,tag:p?.game_tag||`P-${u.id.slice(0,6).toUpperCase()}`,rating,avatar:p?.avatar_url||undefined}};
}
export type EnvLike={SUPABASE_URL:string;SUPABASE_PUBLISHABLE_KEY:string};
