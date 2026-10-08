export type BackendMessage={type:string;[key:string]:unknown};
export type AccessTokenProvider=()=>Promise<string>;

export class HzaBackendClient{
 private socket:WebSocket|null=null;
 private generation=0;
 private cancelConnect:(()=>void)|null=null;
 private origin:string;
 private getAccessToken:AccessTokenProvider;
 private listeners=new Set<(message:BackendMessage)=>void>();
 constructor(origin:string,getAccessToken:AccessTokenProvider){this.origin=origin.replace(/\/+$/,'');this.getAccessToken=getAccessToken;const u=new URL(this.origin);if(u.protocol!=='https:'&&!(u.protocol==='http:'&&['localhost','127.0.0.1','[::1]'].includes(u.hostname)))throw new Error('HTTPS_REQUIRED');if(u.username||u.password||u.search||u.hash||u.pathname!=='/')throw new Error('INVALID_ORIGIN')}
 onMessage(fn:(message:BackendMessage)=>void){this.listeners.add(fn);return()=>this.listeners.delete(fn)}
 private emit(message:BackendMessage){for(const fn of this.listeners)fn(message)}
 private async token(){const token=await this.getAccessToken();if(!token)throw new Error('NOT_SIGNED_IN');return token}
 async api<T=any>(path:string,init:RequestInit={}){const token=await this.token();const headers=new Headers(init.headers);headers.set('Authorization',`Bearer ${token}`);if(init.body&&!headers.has('Content-Type'))headers.set('Content-Type','application/json');const r=await fetch(`${this.origin}${path}`,{...init,headers});const payload:unknown=await r.json().catch(()=>({}));if(!r.ok){const error=payload&&typeof payload==='object'&&'error' in payload?payload.error:null;throw new Error(typeof error==='string'?error:`HTTP_${r.status}`)}return payload as T}
 async me(){return this.api('/api/me')}
 async updateProfile(input:{display_name?:string;game_tag?:string;avatar_url?:string}){return this.api('/api/profile',{method:'PATCH',body:JSON.stringify(input)})}
 async createRoom(input:Record<string,unknown>){return this.api('/api/rooms/create',{method:'POST',body:JSON.stringify(input)})}
 async joinQueue(input:{mode:'regular'|'turbo'|'ranked';region?:string;allowBots?:boolean}){return this.api('/api/matchmaking/join',{method:'POST',body:JSON.stringify(input)})}
 async leaveQueue(){return this.api('/api/matchmaking/leave',{method:'POST',body:'{}'})}
 async queueStatus(){return this.api('/api/matchmaking/status')}
 async connectRoom(code:string,password=''){
  this.close();const generation=this.generation;
  if(!/^[a-z0-9_-]{3,20}$/i.test(code))throw new Error('INVALID_ROOM');
  const token=await this.token(),wsOrigin=this.origin.replace(/^http/,'ws'),protocols=['hza-konkan',`auth.${token}`];
  if(password)protocols.push(`roompw.${await this.hash(`room:${password}`)}`);
  if(generation!==this.generation)throw new Error('CONNECTION_SUPERSEDED');
  const socket=new WebSocket(`${wsOrigin}/room/${encodeURIComponent(code.toUpperCase())}`,protocols);this.socket=socket;
  const current=()=>generation===this.generation&&this.socket===socket;
  socket.onmessage=e=>{if(!current())return;let message:unknown;try{message=JSON.parse(e.data)}catch{this.emit({type:'protocol_error'});return}if(!message||typeof message!=='object'||Array.isArray(message)||!('type' in message)||typeof message.type!=='string'){this.emit({type:'protocol_error'});return}this.emit(message as BackendMessage)};
  return new Promise<WebSocket>((resolve,reject)=>{
   let settled=false;
   const finish=(error?:string)=>{if(settled)return;settled=true;clearTimeout(timer);if(current())this.cancelConnect=null;if(error)reject(new Error(error));else resolve(socket)};
   const timer=setTimeout(()=>{finish('SOCKET_TIMEOUT');if(current())this.close()},10000);
   this.cancelConnect=()=>finish('CONNECTION_SUPERSEDED');
   socket.onopen=()=>{if(current())finish()};
   socket.onerror=()=>{if(!current())return;finish('SOCKET_FAILED');this.emit({type:'socket_error'});this.close()};
   socket.onclose=e=>{if(!current())return;finish('SOCKET_CLOSED');this.socket=null;this.emit({type:'socket_closed',code:e.code,reason:e.reason})};
  });
 }
 send(type:string,payload:Record<string,unknown>={}){if(!this.socket||this.socket.readyState!==WebSocket.OPEN)throw new Error('SOCKET_NOT_OPEN');this.socket.send(JSON.stringify({...payload,type}))}
 ready(value=true){this.send('ready',{ready:value})}
 start(){this.send('start')}
 move(version:number,action:unknown){this.send('move',{version,action})}
 addBot(level:'beginner'|'intermediate'|'advanced'|'pro'='intermediate'){this.send('add_bot',{level})}
 removeBot(id:string){this.send('remove_bot',{id})}
 message(text:string){this.send('message',{text})}
 leave(){this.send('leave')}
 close(){this.cancelConnect?.();this.cancelConnect=null;this.generation++;const socket=this.socket;this.socket=null;if(socket){socket.onopen=null;socket.onclose=null;socket.onerror=null;socket.onmessage=null;socket.close(1000,'client_close')}}
 private async hash(value:string){const d=await crypto.subtle.digest('SHA-256',new TextEncoder().encode(value));return Array.from(new Uint8Array(d)).map(b=>b.toString(16).padStart(2,'0')).join('')}
}
