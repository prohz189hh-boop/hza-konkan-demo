import {applyAction, newGame, validateMeld, viewFor, type Game, type Tile} from '../../src/engine.ts';
import {chooseBot} from '../../src/bots.ts';

let game: Game;
let selected = new Set<string>();
const $ = (id: string) => document.getElementById(id)!;
const logLines: string[] = [];
const write = (message: string) => { logLines.unshift(message); $('log').textContent = logLines.slice(0, 12).join('\n'); };
const current = () => game.players[game.turn];
const tileText = (tile: Tile) => tile.antique ? 'antique' : `${tile.color} ${tile.n}`;

function render() {
  const p = current();
  $('status').textContent = `Turn: ${p.name} · phase: ${game.phase} · score ${game.scores.join('–')} · joker ${game.joker.color} ${game.joker.n}`;
  $('hand').replaceChildren(...p.hand.map((tile) => {
    const button = document.createElement('button'); button.className = `tile${selected.has(tile.id) ? ' selected' : ''}`; button.textContent = tileText(tile);
    button.onclick = () => { selected.has(tile.id) ? selected.delete(tile.id) : selected.add(tile.id); render(); }; return button;
  }));
  $('draw').toggleAttribute('disabled', game.phase !== 'draw' || !p || p.bot);
  $('discard').toggleAttribute('disabled', game.phase !== 'play' || p.bot || selected.size !== 1);
}
function reset() { game = newGame([{name: 'You'}, {name: 'Dara', bot: true}, {name: 'Roj', bot: true}, {name: 'Ari', bot: true}], 42, 1); selected.clear(); logLines.length = 0; write('New deterministic game (seed 42).'); render(); }
function advanceBots() {
  let guard = 0;
  while (game.status === 'playing' && game.players[game.turn].bot && guard++ < 100) {
    const actor = game.players[game.turn].name;
    const action = chooseBot(viewFor(game, game.turn));
    game = applyAction(game, game.turn, action);
    write(`bot ${actor}: ${JSON.stringify(action)}`);
  }
}
function act(action: Parameters<typeof applyAction>[2]) { try { game = applyAction(game, game.turn, action); selected.clear(); write(JSON.stringify(action)); advanceBots(); render(); } catch (error) { write(`Rejected: ${(error as Error).message}`); } }

$('draw').onclick = () => act({type: 'draw', source: 'deck'});
$('discard').onclick = () => { const id = [...selected][0] ?? game.drawn; if (id) act({type: 'discard', tile: id}); };
$('reset').onclick = reset;
$('inspect').onclick = () => { const tiles = current().hand.filter((tile) => selected.has(tile.id)); const meld = validateMeld(tiles, game.joker); $('meld').textContent = meld ? `Legal ${meld.kind}: ${meld.score} points\n${meld.faces.map((face) => `${face.color} ${face.n}`).join(', ')}` : 'Not a legal meld for this joker.'; };
$('bot').onclick = () => { const action = chooseBot(viewFor(game, game.turn)); $('meld').textContent = `Suggested action:\n${JSON.stringify(action, null, 2)}`; };
reset();
