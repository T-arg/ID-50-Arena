# ID-50 ARENA

Shoot-out for 2 to 4 Arduboys over the internet. Pick one of four fighters in the lobby: Amy (the sprite from VIRUS LQP-79), Dee, Bob or Cid. They share the walking feet and differ in the upper body. The host picks one of three game types and the score to reach (3, 5, 7 or 9).

No game server: a web page (`index.html`) links each Arduboy (USB) to the other players (WebRTC).

## Play

1. Flash `ARENA_AB/ARENA_AB.ino` on every Arduboy (needs the Arduboy2 library).
2. Put `docs/index.html` (with the `docs/images` folder next to it) on any HTTPS host (GitHub Pages: publish the `docs` folder) or run it from `localhost`. Web Serial does not work from a `file://` page.
3. Everybody opens the page in **Chrome, Edge or Opera** on a computer, plugs in the Arduboy and presses **Connect Arduboy**.
4. One player presses **Host a game** and shares the 4 character code (or the link). The others type it and press **Join**.
5. Start ID-50 Arena on the Arduboys. The lobby shows who is connected and the current game type. The host presses **A** to open the game setup, picks the type and the target, and presses **A** again to start.

Controls (same as VIRUS LQP-79): D-pad walks, **B** shoots and turns Amy the way she walks, **A** shoots and keeps her facing the same way (strafe).

In the lobby press **Left / Right** to choose your character (Amy, Dee, Bob or Cid; two players can't have the same one). Arrows left and right of your character show that you can pick.

### Game types

Set by the host on the setup screen (Up / Down pick a line, Left / Right change it, A starts, B goes back). After a round the host presses A to play again with the same rules or B to return to the lobby.

| Type | Rules |
|---|---|
| **DUEL** | Everybody against everybody, a frag is a point. |
| **FLAG** | One neutral flag in the middle of the arena. Touch it, carry it to your own corner and score a point. The carrier is a quarter slower. If the carrier dies the flag drops there and goes back to the middle after 15 s. A blinking dot at the edge of the screen points to the flag (or to your corner when you carry it). |
| **TEAM** | Exactly 4 players, 2 teams of 2 (P1+P2 / P3+P4, P1+P3 / P2+P4 or P1+P4 / P2+P3). No friendly fire: bullets pass through teammates. Team frags are the points. |

The first player (or team) to reach the target (3, 5, 7 or 9) wins.

Round rules: 5 hearts, 1 damage per hit, blink for 0.75 s after a hit, respawn in the corner after 2.5 s. Join before the host presses A: someone who joins mid-round waits in the lobby for the next round.

Names and chat live in the web page only (the Arduboy has no keyboard): type your name (up to 12 characters, remembered in your browser) before you host or join, and use the chat box under the player list. Messages go through the host like everything else.

### No Arduboy? Use the simulator

Press **Play in the browser** instead of Connect Arduboy. The page then runs the compiled game in the Ardens web player (the same one the other games use) and treats it like a USB Arduboy, so you can host, join, chat and play with simulated and real Arduboys mixed. Click the screen first: arrow keys walk, **Z** is A, **X** is B. You can only use one of the two (USB or simulator) per page; open a second tab to test two players on one computer.

The simulator loads `ARENA_AB/build/arduino.avr.leonardo/ARENA_AB.ino.hex` from this GitHub repo, so export a new hex (Arduino IDE: Sketch > Export Compiled Binary) and push it after every change to the sketch. Other places: `?hex=URL` loads another hex (it must be a public URL), `?player=URL` uses another copy of the Ardens web player.

Without the browser page the sketch still runs (host only, "SOLO TEST") so you can try the walls and shooting.

### If joining fails ("Negotiation of connection failed")

The two browsers found each other through the PeerJS server but could not open a direct connection. This happens when both are behind the same router (many routers can't loop traffic back), with a VPN or iCloud Private Relay, or behind a strict firewall. Try another network first (for example a phone hotspot for one player). If that is not possible, use a TURN relay: open "Connection problems?" on the page and paste the server, username and password of a TURN service (free accounts exist, e.g. Metered or Cloudflare), or put them in the address: `?turn=turn:your.server.com:3478&user=NAME&pass=SECRET`. One player with a relay is usually enough. Add `&debug` to the address for detailed PeerJS output in the browser console; the log on the page shows the network path state (checking / connected / failed) for both sides.

## How it works

```
Arduboy A <--USB serial--> page A <--WebRTC--> page HOST <--WebRTC--> page C <--postMessage--> Ardens simulator C
```

- The host page is the hub: guests only talk to the host, which forwards to everybody. Seat numbers (P1 = host, P2..P4) are handed out by the host.
- Every Arduboy moves its own Amy locally, so your controls never lag. It sends its position 20 times a second; the other Arduboys glide their copy of you toward it.
- A shot is sent once ("fired from x,y in direction d"); every Arduboy flies the bullet itself.
- The player who is hit decides it is a hit, lowers its health and tells the others. Frags and the round end follow from those messages, so all screens agree. On the shooter's screen the bullet also stops when it reaches another Amy and a spark shows the hit (a prediction: the damage only counts when the other player confirms it). A small bar under each Amy shows her hearts.
- WebRTC runs through [PeerJS](https://peerjs.com). Its free cloud server is only used for the first handshake; the game traffic goes peer to peer. To use your own PeerJS server open the page with `?broker=my.server.com` (and optional `&path=/myapp`).

### Serial protocol (`ARENA_AB/net.h`)

Frame: `0xA5, length, payload`. Arduboy to page: `type, data...`. Page to Arduboy: `sender, type, data...` (sender `0xFF` is the page itself).

| type | data | meaning |
|---|---|---|
| `I` | yourId, rosterMask | page to Arduboy, also a heartbeat every second |
| `P` | x16, y16, dir, flags | position (flags: walking, alive, blinking) |
| `F` | x16, y16, dir | a bullet was fired |
| `H` | shooterId, healthLeft | the sender was hit |
| `C` | character | lobby: the character the sender picked (also in bits 4-5 of the `P` flags) |
| `G` | mode, target, teamSet | host starts a round with these rules (mode 0 DUEL, 1 FLAG, 2 TEAM) |
| `S` | mode, target, teamSet | the host's current choice, repeated in the lobby so everybody sees it |
| `R` | | host sends everybody back to the lobby |
| `L` | holder, x16, y16 | FLAG: sent by the host: who carries the flag (255 = nobody) or where it lies |
| `Q` | s0, s1, s2, s3 | FLAG: sent by the host after a capture: the scores |
| `K` | kind | FLAG: sent to the host: 0 = I touched the flag, 1 = I am home with it |

To make another game use the same page: send frames like these from your own sketch. The page relays any payload of 1 to 11 bytes unchanged.

## Notes

- Expect 30 to 150 ms of delay between players. Bullets are slow enough that it feels fine.
- If the Arduboy is in the bootloader (shows another port) just reset it. The page never opens the port at 1200 baud.
- Tested: the game logic with a desktop build against a stubbed Arduboy2, and the page with a fake serial port and a fake PeerJS in a headless browser. Not yet tested on real hardware or against the real PeerJS cloud.

## Page version

The page in `docs/index.html` has a version number (`VERSION` at the top of its script, shown under the title and in the footer). Bump it with every change to the page: patch (1.0.1) for a fix, minor (1.1.0) for something new, and add a line here.

| Version | Change |
| --- | --- |
| 1.4.0 | Manual explains the three game types (DUEL, FLAG, TEAM), the host setup and the target score. |
| 1.3.1 | Cid sprite touched up. |
| 1.3.0 | Four fighters Amy, Dee, Bob and Cid with their own animated sprite on the page (Cy is now Cid). |
| 1.2.0 | Manual explains picking a character in the lobby. |
| 1.1.0 | Text now mentions the four characters (Amy, Bob, Cy, Dee). |
| 1.0.0 | Page restyled like the other TEAM a.r.g. game pages and moved to `docs/`, with banner, manual, version number. Includes host / join with a code, names, chat, Ardens simulator and relay settings. |

Sketch changes (the page loads the compiled hex, so export and push a new hex after these): hit sparks, a hearts bar under every Amy, a bullet that stops on the shooter's screen when it reaches another player, game types DUEL / FLAG / TEAM with a host setup screen (all players need the same hex because of the new messages), and four hand-drawn fighters (Amy, Dee, Bob, Cid: shared feet, own upper body, a mask for every frame) that every player can pick in the lobby with Left / Right. The sprites are bigger now (about 5 KB of flash).
