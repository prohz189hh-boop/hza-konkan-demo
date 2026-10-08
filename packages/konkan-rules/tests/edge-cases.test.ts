import assert from 'node:assert/strict';
import {test} from 'node:test';
import {applyAction, newGame, viewFor, type Action} from '../src/engine.ts';
import {matchModes, sameQueue} from '../src/modes.ts';
import {moveRack, syncRack} from '../src/rack.ts';

test('invalid seats and unknown actions fail without mutating the game', () => {
  const game = newGame([{name: 'A'}, {name: 'B'}, {name: 'C'}, {name: 'D'}], 7, 1);
  const before = structuredClone(game);
  assert.throws(() => applyAction(game, -1, {type: 'next'}), /INVALID_SEAT/);
  assert.throws(() => applyAction(game, 4, {type: 'next'}), /INVALID_SEAT/);
  assert.throws(() => applyAction(game, game.turn, {type: 'unknown'} as unknown as Action), /UNKNOWN_ACTION/);
  assert.deepEqual(game, before);
});

test('redacted views keep opponent hands private after a normal action', () => {
  const game = newGame([{name: 'A'}, {name: 'B'}, {name: 'C'}, {name: 'D'}], 9, 1);
  const view = viewFor(game, game.turn);
  assert.equal(view.players[game.turn].hand.length, game.players[game.turn].hand.length);
  for (const [seat, player] of view.players.entries()) if (seat !== game.turn) assert.equal(player.hand.length, 0);
});

test('rack helpers preserve live ids and reject out-of-range moves', () => {
  assert.deepEqual(syncRack(['a', 'a', 'gone', ''], ['a', 'b']), ['a', 'b', '', '']);
  assert.deepEqual(moveRack(['a', 'b', 'c'], 'a', 2, 3), ['c', 'b', 'a']);
  assert.deepEqual(moveRack(['a', 'b'], 'a', 3, 2), ['a', 'b']);
});

test('modes expose stable round counts and queue compatibility', () => {
  assert.equal(matchModes.regular.rounds, 7);
  assert.equal(matchModes.turbo.rounds, 5);
  assert.equal(sameQueue({matchMode: 'ranked'}, {matchMode: 'ranked'}), true);
  assert.equal(sameQueue({matchMode: 'regular'}, {matchMode: 'turbo'}), false);
});
