# ID-50 ARENA

Shoot-out for 2 to 4 Arduboys over the internet. Everybody plays Amy (the sprite from VIRUS LQP-79), first to 10 frags wins.

No game server: a web page (`index.html`) links each Arduboy (USB) to the other players (WebRTC).

## Play

1. Flash `ARENA_AB/ARENA_AB.ino` on every Arduboy (needs the Arduboy2 library).
2. Put `index.html` on any HTTPS host (GitHub Pages works) or run it from `localhost`. Web Serial does not work from a `file://` page.
3. Everybody opens the page in **Chrome, Edge or Opera** on a computer, plugs in the Arduboy and presses **Connect Arduboy**.
4. One player presses **Host a game** and shares the 4 character code (or the link). The others type it and press **Join**.
5. Start ID-50 Arena on the Arduboys. The lobby shows who is connected. The host presses **A** to start.

Controls (same as VIRUS LQP-79): D-pad walks, **B** shoots and turns Amy the way she walks, **A** shoots and keeps her facing the same way (strafe).

Round rules: 5 hearts, 1 damage per hit, blink for 0.75 s after a hit, respawn in the corner after 2.5 s. Join before the host presses A: someone who joins mid-round waits in the lobby for the next round.

Names and chat live in the web page only (the Arduboy has no keyboard): type your name (up to 12 characters, remembered in your browser) before you host or join, and use the chat box under the player list. Messages go through the host like everything else.

Without the browser page the sketch still runs (host only, "SOLO TEST") so you can try the walls and shooting.

## How it works

```
Arduboy A <--USB serial--> page A <--WebRTC--> page HOST <--WebRTC--> page C <--USB serial--> Arduboy C
```

- The host page is the hub: guests only talk to the host, which forwards to everybody. Seat numbers (P1 = host, P2..P4) are handed out by the host.
- Every Arduboy moves its own Amy locally, so your controls never lag. It sends its position 20 times a second; the other Arduboys glide their copy of you toward it.
- A shot is sent once ("fired from x,y in direction d"); every Arduboy flies the bullet itself.
- The player who is hit decides it is a hit, lowers its health and tells the others. Frags and the round end follow from those messages, so all screens agree.
- WebRTC runs through [PeerJS](https://peerjs.com). Its free cloud server is only used for the first handshake; the game traffic goes peer to peer. To use your own PeerJS server open the page with `?broker=my.server.com` (and optional `&path=/myapp`).

### Serial protocol (`ARENA_AB/net.h`)

Frame: `0xA5, length, payload`. Arduboy to page: `type, data...`. Page to Arduboy: `sender, type, data...` (sender `0xFF` is the page itself).

| type | data | meaning |
|---|---|---|
| `I` | yourId, rosterMask | page to Arduboy, also a heartbeat every second |
| `P` | x16, y16, dir, flags | position (flags: walking, alive, blinking) |
| `F` | x16, y16, dir | a bullet was fired |
| `H` | shooterId, healthLeft | the sender was hit |
| `G` | | host starts a round |

To make another game use the same page: send frames like these from your own sketch. The page relays any payload of 1 to 11 bytes unchanged.

## Notes

- Expect 30 to 150 ms of delay between players. Bullets are slow enough that it feels fine.
- If the Arduboy is in the bootloader (shows another port) just reset it. The page never opens the port at 1200 baud.
- Tested: the game logic with a desktop build against a stubbed Arduboy2, and the page with a fake serial port and a fake PeerJS in a headless browser. Not yet tested on real hardware or against the real PeerJS cloud.
