/** Match totals decide the match; Game.winner identifies only the last round's seat. */
export function matchWinner(scores:readonly [number,number]):1|2|null{
 if(!scores.every(Number.isFinite))throw new Error('INVALID_MATCH_SCORE');
 return scores[0]===scores[1]?null:scores[0]>scores[1]?1:2;
}
