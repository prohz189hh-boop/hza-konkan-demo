import assert from 'node:assert/strict';
import {test} from 'node:test';
import {matchWinner} from '../selected-source/backend/match-result.ts';
test('match result uses accumulated totals, never the final round winner',()=>{
 assert.equal(matchWinner([900,100]),1);
 assert.equal(matchWinner([100,900]),2);
 assert.equal(matchWinner([200,200]),null);
 assert.throws(()=>matchWinner([NaN,2]),/INVALID_MATCH_SCORE/);
});
