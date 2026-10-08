export type MatchMode='regular'|'turbo'|'ranked';
export type BotLevel='beginner'|'intermediate'|'advanced'|'pro';
export type SeatPlan='open'|'duel'|'coop'|'solo';
export const matchModes={regular:{rounds:7,name:'Regular',ranked:false},turbo:{rounds:5,name:'Turbo',ranked:false},ranked:{rounds:7,name:'Ranked',ranked:true}} as const;
export const modeOf=(mode?:MatchMode):MatchMode=>mode||'regular';
export const sameQueue=(a:{matchMode?:MatchMode},b:{matchMode?:MatchMode})=>modeOf(a.matchMode)===modeOf(b.matchMode);
