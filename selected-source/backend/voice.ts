function b64(bytes:Uint8Array){let s='';for(const b of bytes)s+=String.fromCharCode(b);return btoa(s).replaceAll('+','-').replaceAll('/','_').replace(/=+$/,'')}
const enc=(v:unknown)=>b64(new TextEncoder().encode(JSON.stringify(v)));
async function sign(secret:string,data:string){const k=await crypto.subtle.importKey('raw',new TextEncoder().encode(secret),{name:'HMAC',hash:'SHA-256'},false,['sign']);const sig=await crypto.subtle.sign('HMAC',k,new TextEncoder().encode(data));return b64(new Uint8Array(sig));}
export async function liveKitToken(env:{LIVEKIT_URL?:string;LIVEKIT_API_KEY?:string;LIVEKIT_API_SECRET?:string},identity:string,name:string,room:string){
 if(!env.LIVEKIT_URL||!env.LIVEKIT_API_KEY||!env.LIVEKIT_API_SECRET)throw new Error('voice_not_configured');const now=Math.floor(Date.now()/1000);const header={alg:'HS256',typ:'JWT'},payload={iss:env.LIVEKIT_API_KEY,sub:identity,nbf:now-5,exp:now+3600,name,video:{roomJoin:true,room,canPublish:true,canSubscribe:true,canPublishData:true}};const unsigned=`${enc(header)}.${enc(payload)}`;return{url:env.LIVEKIT_URL,token:`${unsigned}.${await sign(env.LIVEKIT_API_SECRET,unsigned)}`};
}
