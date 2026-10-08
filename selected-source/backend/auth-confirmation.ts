/** A native-game landing page. Never exchanges, stores or forwards URL credentials. */
export function confirmationPage(): Response {
 const nonce=crypto.randomUUID().replaceAll('-','');
 const html=`<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>HZA KONKAN · Email confirmation</title>
<script nonce="${nonce}">
(()=>{const fragment=new URLSearchParams(location.hash.slice(1));window.hzaConfirmationFailed=fragment.has('error')||fragment.has('error_code');history.replaceState(null,'','/auth/confirmed');})();
</script>
<style nonce="${nonce}">:root{color-scheme:dark}*{box-sizing:border-box}body{margin:0;min-height:100svh;display:grid;place-items:center;padding:24px;background:#09121e;color:#eeeae2;font:18px/1.6 system-ui,sans-serif}main{max-width:560px;border:1px solid #866441;border-radius:20px;padding:40px;background:#111f30}small{color:#c59a65;letter-spacing:.16em}h1{font-size:clamp(28px,6vw,38px);line-height:1.15;margin:20px 0}p{color:#bbc6d3}strong{color:#eeeae2}.step{border-top:1px solid #324052;padding-top:20px;margin-top:24px}footer{font-size:14px;color:#8d9bac}</style></head>
<body><main><small>HZA KONKAN</small><h1 id="title">Return to the game</h1><p id="message">If you just confirmed your email, open HZA KONKAN and sign in to continue.</p><p class="step">Choose <strong>Back to sign in</strong> in the game, then use the account you created. Your player name is set up inside HZA KONKAN.</p><footer>You can close this page. This page does not store your login.</footer><noscript><p>Close this tab and return to the game. Avoid sharing the confirmation link.</p></noscript></main>
<script nonce="${nonce}">if(window.hzaConfirmationFailed){document.getElementById('title').textContent='Check your confirmation link';document.getElementById('message').textContent='This link could not be confirmed. It may have expired or already been used. Try signing in to the game first.';}delete window.hzaConfirmationFailed;</script></body></html>`;
 return new Response(html,{headers:{
  'Content-Type':'text/html; charset=utf-8','Cache-Control':'no-store','Referrer-Policy':'no-referrer',
  'X-Content-Type-Options':'nosniff','X-Frame-Options':'DENY',
  'Content-Security-Policy':`default-src 'none'; script-src 'nonce-${nonce}'; style-src 'nonce-${nonce}'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'; connect-src 'none'`,
 }});
}
