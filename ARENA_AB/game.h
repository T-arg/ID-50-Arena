#ifndef GAME_H
#define GAME_H

// facing direction for a move of (x, y), index = (y + 1) * 3 + (x + 1), 255 = not moving   (same as VIRUS LQP-79)
const byte PROGMEM facingByMove[9] = { 3, 4, 5, 2, 255, 6, 1, 0, 7 };

// texts that are used in more than one place (one copy in flash)
const char sHostChooses[] PROGMEM = "HOST CHOOSES";
const char sModes[] PROGMEM = "DUEL\0FLAG\0TEAM";   // 5 bytes each: sModes + 5 * gameMode
const char sWaiting[] PROGMEM = "WAITING";
const char sReady[] PROGMEM = "READY";
const char sStart[] PROGMEM = "B START";
const char sConfirm[] PROGMEM = "B CONFIRM";
#define FS(s) ((const __FlashStringHelper *)(s))

// one shared clamp (the constrain() macro repeats its argument three times, which costs a lot of flash)
int __attribute__((noinline)) clampi(int v, int lo, int hi)
{
  return v < lo ? lo : (v > hi ? hi : v);
}

// the velocity of a bullet in each of the 8 directions (same as VIRUS LQP-79)
const char PROGMEM bulletVel[8] = { 0, -2, -3, -2, 0, 2, 3, 2 };

char bulletVx(byte dir) { return (char)pgm_read_byte(&bulletVel[dir & 7]); }
char bulletVy(byte dir) { return (char)pgm_read_byte(&bulletVel[(dir + 6) & 7]); }

int camX = 0;
int camY = 0;

// drawing ///////////////////////////////////////////////////////////////////

// drawAmy
// the four fighters: 0 Amy, 1 Dee, 2 Bob, 3 Cid. The 16x16 frame is put together from its stored pages (see bitmaps.h):
// image top, image bottom, mask top, mask bottom, read straight from flash. It is drawn into the screen buffer: erase where
// the mask is set, then OR the image. Facing left mirrors the pictures that face right.
void drawAmy(int x, int y, byte dir, byte frame, byte who)
{
  const unsigned char *ix = charIdx + (who * 20 + frame + 4 * (dir <= 4 ? dir : 8 - dir)) * 4;
  const byte *src[4];
  for (byte i = 0; i < 4; i++) src[i] = charPage + pgm_read_byte(ix + i) * 16;
  byte *buf = arduboy.getBuffer();
  byte sh = y & 7, rs = 8 - sh;                      // the sprite lands between two screen rows: the part of a row is shifted down by sh, the one before it comes up by rs
  int page = y >> 3;
  for (byte c = 0; c < 16; c++)
  {
    int sx = x + c;
    if (sx < 0 || sx > 127) continue;
    byte k = (dir > 4) ? 15 - c : c;
    byte pm = 0, pd = 0;                             // the mask and image byte of the row above
    for (byte q = 0; q < 3; q++)                     // 16 pixels cover 3 screen rows
    {
      byte cm = 0, cd = 0;
      if (q < 2) { cd = pgm_read_byte(src[q] + k); cm = pgm_read_byte(src[q + 2] + k); }
      int pg = page + q;
      if (pg >= 0 && pg <= 7)
      {
        byte *s = buf + pg * 128 + sx;
        *s = (*s & ~(byte)((cm << sh) | (pm >> rs))) | (byte)((cd << sh) | (pd >> rs));
      }
      pm = cm;
      pd = cd;
    }
  }
}

// game logic ////////////////////////////////////////////////////////////////

void spawnPlayer(byte id)
{
  Player& p = players[id];
  p.x = p.tx = pgm_read_word(&spawnX[id]);
  p.y = p.ty = pgm_read_word(&spawnY[id]);
  p.dir = (id & 1) ? 2 : 6;                       // face the middle of the arena
  p.frame = 0;
  p.walking = 0;
  p.alive = 1;
  p.health = PLAYER_START_HEALTH;
  p.flash = INVULN_SPAWN;
  p.respawn = 0;
  p.weapon = W_NONE;
}

// addSpark
// a little star where somebody was hit
void addSpark(int x, int y)
{
  byte slot = 0;
  for (byte i = 0; i < SPARK_MAX; i++)
  {
    if (sparks[i].t == 0) { slot = i; break; }
    if (sparks[i].t < sparks[slot].t) slot = i;          // all busy: replace the oldest
  }
  sparks[slot].x = x;
  sparks[slot].y = y;
  sparks[slot].t = SPARK_LIFE;
}

// frames a bullet lives, per type: bullet, double gun, rocket (half speed), flame, bomb (half speed)
const byte PROGMEM bulletLife[5] = { BULLET_LIFE, BULLET_LIFE, BULLET_LIFE * 2, FLAME_LIFE, BOMB_LIFE };

void addBullet(int x, int y, byte dir, byte owner, byte type = 0)
{
  for (byte i = 0; i < BULLET_MAX; i++)
  {
    Bullet& b = bullets[i];
    if (b.life == 0)
    {
      b.x = x;
      b.y = y;
      b.vx = bulletVx(dir);
      b.vy = bulletVy(dir);
      b.owner = owner;
      b.type = type;
      b.life = pgm_read_byte(bulletLife + type);
      return;
    }
  }
}

// sound effects (ATMlib, one square channel; a lower number wins over a higher one)
enum { SFX_WIN, SFX_BOOM, SFX_DIE, SFX_CAPTURE, SFX_HIT, SFX_PICKUP, SFX_SHOOT };
ATM_SFX_TRACK(sfxWin, ATM_VOL(45), ATM_SL_VOL((uint8_t)-1), ATM_NOTE_G4, ATM_DELAY(3), ATM_NOTE_C5, ATM_DELAY(3), ATM_NOTE_E5, ATM_DELAY(3), ATM_NOTE_G5, ATM_DELAY(4), ATM_NOTE_E5, ATM_DELAY(3), ATM_NOTE_G5, ATM_DELAY(14));
ATM_SFX_TRACK(sfxBoom, ATM_VOL(63), ATM_SL_FRQ((uint8_t)-25), ATM_NOTE_C2, ATM_DELAY(6), ATM_SL_VOL((uint8_t)-6), ATM_DELAY(11));
ATM_SFX_TRACK(sfxDie, ATM_VOL(50), ATM_SL_FRQ((uint8_t)-60), ATM_NOTE_B4, ATM_DELAY(14), ATM_SL_VOL((uint8_t)-5), ATM_DELAY(10));
ATM_SFX_TRACK(sfxCapture, ATM_VOL(45), ATM_SL_VOL((uint8_t)-1), ATM_NOTE_C5, ATM_DELAY(3), ATM_NOTE_E5, ATM_DELAY(3), ATM_NOTE_G5, ATM_DELAY(3), ATM_NOTE_E5, ATM_DELAY(2), ATM_NOTE_G5, ATM_DELAY(10));
ATM_SFX_TRACK(sfxHit, ATM_VOL(55), ATM_SL_FRQ((uint8_t)-50), ATM_NOTE_E4, ATM_DELAY(4), ATM_SL_VOL((uint8_t)-14), ATM_DELAY(4));
ATM_SFX_TRACK(sfxPickup, ATM_VOL(40), ATM_SL_VOL((uint8_t)-2), ATM_NOTE_C5, ATM_DELAY(2), ATM_NOTE_E5, ATM_DELAY(2), ATM_NOTE_G5, ATM_DELAY(6));
ATM_SFX_TRACK(sfxShoot, ATM_VOL(50), ATM_SL_FRQ((uint8_t)-127), ATM_NOTE_G5, ATM_DELAY(4));
const byte * const sfxTracks[] PROGMEM = { sfxWin, sfxBoom, sfxDie, sfxCapture, sfxHit, sfxPickup, sfxShoot };
const byte sfxFrames[] PROGMEM = { 74, 42, 59, 52, 21, 26, 11 };
byte sfxNow = 0, sfxLeft = 0;

void sfx(byte n)
{
  if (!arduboy.audio.enabled() || (sfxLeft && n > sfxNow)) return;
  sfxNow = n;
  sfxLeft = pgm_read_byte(sfxFrames + n);
  ATM.playSfx((const byte*)pgm_read_word(sfxTracks + n), CH_ONE);
}

// fireShot
// one shot of a weapon, for our own gun and for the shots of the others
void fireShot(int x, int y, byte dir, byte owner, byte weapon)
{
  sfx(SFX_SHOOT);
  int px = (bulletVy(dir) < 0) - (bulletVy(dir) > 0);      // one step sideways
  int py = (bulletVx(dir) > 0) - (bulletVx(dir) < 0);
  int j = weapon == W_FLAME ? (int)((arduboy.frameCount + x) & 3) - 1 : (weapon == W_DOUBLE ? 3 : 0);   // the flame wobbles, the double gun has two bullets 3 pixels either way
  addBullet(x + px * j, y + py * j, dir, owner, weapon);
  if (weapon == W_DOUBLE) addBullet(x - px * j, y - py * j, dir, owner, weapon);
}

// teams (team game only): teamSet 0 = P1+P2 against P3+P4, 1 = P1+P3 against P2+P4, 2 = P1+P4 against P2+P3
byte teamOf(byte i)
{
  if (teamSet == 0) return i >> 1;
  if (teamSet == 1) return i & 1;
  return (i == 0 || i == 3) ? 0 : 1;
}

bool sameTeam(byte a, byte b)
{
  return gameMode == MODE_TEAM && a != b && teamOf(a) == teamOf(b);
}

// fighterChar
// which character is drawn for player i. In a team game both players of a team look the same: the look the host gave the team.
byte fighterChar(byte i)
{
  return (gameMode == MODE_TEAM) ? teamChar[teamOf(i)] : players[i].ch;
}

// checkWin
// somebody (or some team) reached the target score
void checkWin()
{
  byte n = (gameMode == MODE_TEAM) ? 2 : MAX_PLAYERS;
  for (byte i = 0; i < n; i++)
  {
    if (score[i] >= winTarget && gameState == STATE_PLAY)
    {
      winner = i;
      gameState = STATE_OVER;
      sfx(SFX_WIN);
    }
  }
}

void startGame()
{
  buildPockets();
  if (flagPocket >= pocketCount) flagPocket = 0;
  memset(score, 0, sizeof score);
  flagHolder = 255;
  flagX = flagHomeX();
  flagY = flagHomeY();
  flagTimer = 0;
  joinedMask = rosterMask;
  readyMask = 0;                                   // the next lobby starts with nobody ready
  hostConfirm = 0;
  winner = 255;
  shotDelay = 0;
  sendTimer = 0;
  memset(bullets, 0, sizeof bullets);
  memset(sparks, 0, sizeof sparks);
  memset(booms, 0, sizeof booms);
  for (byte i = 0; i < PICKUP_MAX; i++)
  {
    pickups[i].type = 0;
    pickups[i].timer = 120 + i * 100;              // the host puts them out one after the other
  }
  for (byte i = 0; i < MAX_PLAYERS; i++)
  {
    players[i].frags = 0;
    spawnPlayer(i);
  }
  gameState = STATE_PLAY;
}

void sendFlag();                                 // defined below

void hostDrop(byte type, int x, int y);

// applyHit
// the victim decided it was hit; everybody (the victim too) runs this so all screens agree on health and frags
void applyHit(byte victim, byte shooter, byte health)
{
  Player& v = players[victim];
  v.health = health;
  sfx(health ? SFX_HIT : SFX_DIE);
  addSpark(v.x + 8, v.y + 8);

  // take the bullet away on this screen too
  for (byte i = 0; i < BULLET_MAX; i++)
  {
    Bullet& b = bullets[i];
    if (b.life && b.owner == shooter && abs(b.x - (v.x + 8)) < 12 && abs(b.y - (v.y + 9)) < 12) b.life = 0;
  }

  if (health == 0)
  {
    v.alive = 0;
    v.respawn = RESPAWN_TIME;
    if (v.weapon && myId == 0) hostDrop(v.weapon, v.x + 8, v.y + 10);       // the weapon stays where we died
    v.weapon = W_NONE;
    if (gameMode == MODE_FLAG && flagHolder == victim && myId == 0)
    {
      flagHolder = 255;                           // the host lets it fall where the carrier died
      flagX = v.x + 8;
      flagY = v.y + 10;
      flagTimer = FLAG_RETURN;
      sendFlag();
    }
    if (shooter < MAX_PLAYERS && shooter != victim && !sameTeam(shooter, victim))
    {
      players[shooter].frags++;
      if (gameMode == MODE_DUEL) score[shooter]++;
      else if (gameMode == MODE_TEAM) score[teamOf(shooter)]++;
      checkWin();
    }
  }
}

// sending ///////////////////////////////////////////////////////////////////

void sendPos()
{
  Player& me = players[myId];
  txBuf[0] = 'P';
  netPutInt(1, me.x);
  netPutInt(3, me.y);
  txBuf[5] = me.dir;
  txBuf[6] = (me.walking ? 1 : 0) | (me.alive ? 2 : 0) | (me.flash ? 4 : 0) | (me.ch << 4);
  netSend(7);
}

void sendFire(int x, int y, byte dir, byte weapon)
{
  txBuf[0] = 'F';
  netPutInt(1, x);
  netPutInt(3, y);
  txBuf[5] = dir | (weapon << 3);
  netSend(6);
}

void sendChar()
{
  txBuf[0] = 'C';
  txBuf[1] = players[myId].ch | (((readyMask >> myId) & 1) << 2);       // the character and (bit 2) ready
  netSend(2);
}

void sendHit(byte shooter, byte health)
{
  txBuf[0] = 'H';
  txBuf[1] = shooter;
  txBuf[2] = health;
  netSend(3);
}

// the host's choices, and the arena of the round. 'S' repeats them in the lobby, 'G' starts the round with them
void sendSettings(char t = 'S')
{
  txBuf[0] = t;
  txBuf[1] = gameMode;
  txBuf[2] = winTarget;
  txBuf[3] = teamSet;
  txBuf[4] = teamChar[0];
  txBuf[5] = teamChar[1];
  txBuf[6] = menuStage;
  netPutInt(7, mapSeed);
  txBuf[9] = flagPocket;
  netSend(10);
}

void sendGo()
{
  mapSeed = (unsigned int)micros() ^ ((unsigned int)arduboy.frameCount << 3);      // a new random arena for this round
  buildPockets();
  flagPocket = (mapSeed >> 5) % pocketCount;
  sendSettings('G');
}

void sendBack()
{
  txBuf[0] = 'R';
  netSend(1);
}

void sendFlag()
{
  txBuf[0] = 'L';
  txBuf[1] = flagHolder;
  netPutInt(2, flagX);
  netPutInt(4, flagY);
  netSend(6);
}

void sendScores()
{
  txBuf[0] = 'Q';
  for (byte i = 0; i < MAX_PLAYERS; i++) txBuf[1 + i] = score[i];
  netSend(5);
}

void sendClaim(byte kind, byte slot = 0)
{
  txBuf[0] = 'K';
  txBuf[1] = kind;
  txBuf[2] = slot;
  netSend(3);
}

void sendPickup(byte slot)
{
  txBuf[0] = 'U';
  txBuf[1] = slot;
  txBuf[2] = pickups[slot].type;
  netPutInt(3, pickups[slot].x);
  netPutInt(5, pickups[slot].y);
  netSend(7);
}

void sendWeapon(byte id)
{
  txBuf[0] = 'W';
  txBuf[1] = id;
  txBuf[2] = players[id].weapon;
  netSend(3);
}

// weapons, host side: only the host decides which pickup lies where and who takes it
// hostDrop
// a pickup appears at x, y (a weapon that was dropped by somebody who died)
void hostDrop(byte type, int x, int y)
{
  byte slot = 0;
  for (byte i = 0; i < PICKUP_MAX; i++) if (pickups[i].type == 0) { slot = i; break; }
  pickups[slot].type = type;
  pickups[slot].x = x;
  pickups[slot].y = y;
  sendPickup(slot);
}

// hostSpawnPickup
// a new random weapon on a free spot of the arena
void hostSpawnPickup(byte slot)
{
  rng ^= (unsigned int)micros();
  if (rng == 0) rng = 1;
  for (byte t = 0; t < 40; t++)
  {
    int x = 48 + (((unsigned int)rnd() << 8) | rnd()) % 928;
    int y = 56 + (((unsigned int)rnd() << 8) | rnd()) % 896;
    if (boxBlocked(x - 8, y - 12)) continue;
    pickups[slot].type = 1 + (rnd() & 3);
    pickups[slot].x = x;
    pickups[slot].y = y;
    sendPickup(slot);
    return;
  }
  pickups[slot].timer = 60;                        // no free spot found, try again soon
}

// hostGive
// somebody takes the pickup in this slot
void hostGive(byte id, byte slot)
{
  if (slot >= PICKUP_MAX || pickups[slot].type == 0 || !players[id].alive) return;
  players[id].weapon = pickups[slot].type;
  if (id == myId) sfx(SFX_PICKUP);
  pickups[slot].type = 0;
  pickups[slot].timer = PICKUP_RESPAWN;
  sendPickup(slot);
  sendWeapon(id);
}

// the flag game, host side: only the host decides who holds the flag, so everybody agrees
// is this fighter's body within r pixels (both ways) of a point?
bool isNear(Player& p, int x, int y, byte r)
{
  return abs(p.x + 8 - x) < r && abs(p.y + 10 - y) < r;
}

bool nearHome(byte id, byte r)
{
  return isNear(players[id], (int)pgm_read_word(&spawnX[id]) + 8, (int)pgm_read_word(&spawnY[id]) + 10, r);
}

void hostPickup(byte id)
{
  flagHolder = id;
  sendFlag();
}

void hostCapture(byte id)
{
  score[id]++;
  sfx(SFX_CAPTURE);
  flagHolder = 255;
  flagPocket = (flagPocket + 1 + (unsigned int)micros() % (pocketCount - 1)) % pocketCount;       // the flag moves to another pocket
  flagX = flagHomeX();
  flagY = flagHomeY();
  flagTimer = 0;
  sendFlag();
  sendScores();
  checkWin();
}

// a guest says it touched the flag (kind 0) or is home with it (kind 1)
void hostClaim(byte id, byte kind)
{
  if (kind == 0 && flagHolder == 255 && players[id].alive && isNear(players[id], flagX, flagY, 18)) hostPickup(id);
  else if (kind == 1 && flagHolder == id && nearHome(id, 26)) hostCapture(id);
}

bool nearPickup(Player& p, byte slot)
{
  return slot < PICKUP_MAX && isNear(p, pickups[slot].x, pickups[slot].y, 20);
}

// receiving /////////////////////////////////////////////////////////////////

// netHandle
// a complete message is waiting in rxBuf: sender, type, data...
void netHandle()
{
  byte sender = rxBuf[0];
  byte type = rxBuf[1];

  if (sender == NET_SYSTEM)
  {
    if (type == 'I' && rxBuf[2] < MAX_PLAYERS)
    {
      myId = rxBuf[2];
      rosterMask = rxBuf[3] & 0x0F;
      joinedMask &= rosterMask;                  // players that left are gone
      readyMask &= rosterMask;
      if (myId == 0 && flagHolder < MAX_PLAYERS && !(rosterMask & (1 << flagHolder)))
      {
        flagHolder = 255;                        // the carrier left: the flag goes back to its pocket
        flagX = flagHomeX();
        flagY = flagHomeY();
        sendFlag();
      }
    }
    return;
  }

  if (sender >= MAX_PLAYERS || sender == myId) return;

  if (type == 'G' || type == 'S')
  {
    if (sender != 0 || (type == 'S' && gameState != STATE_LOBBY)) return;     // only the host decides, and a late 'S' must not change a running game
    if (rxLen >= 11 && rxBuf[2] < MODE_COUNT && rxBuf[4] < 3 && rxBuf[5] < MAX_PLAYERS && rxBuf[6] < MAX_PLAYERS && rxBuf[7] < 3 && rxBuf[10] < 6)
    {
      gameMode = rxBuf[2];
      winTarget = clampi(rxBuf[3], 1, 9);
      teamSet = rxBuf[4];
      teamChar[0] = rxBuf[5];
      teamChar[1] = rxBuf[6];
      menuStage = rxBuf[7];
      mapSeed = netInt(8);
      flagPocket = rxBuf[10];
      if (type == 'G') startGame();
    }
    return;
  }

  if (type == 'R')
  {
    if (sender == 0)
    {
      gameState = STATE_LOBBY;
      menuStage = 0;
    }
    return;
  }

  if (type == 'C')
  {
    if (sender < MAX_PLAYERS)
    {
      players[sender].ch = rxBuf[2] & 3;
      readyMask = (readyMask & ~(1 << sender)) | (((rxBuf[2] >> 2) & 1) << sender);
    }
    return;
  }

  if (gameState != STATE_PLAY || !(joinedMask & (1 << sender))) return;
  Player& p = players[sender];

  if (type == 'P')
  {
    p.tx = netInt(2);
    p.ty = netInt(4);
    p.dir = rxBuf[6] & 7;
    p.walking = rxBuf[7] & 1;
    byte alive = (rxBuf[7] >> 1) & 1;
    if (alive && !p.alive) p.health = PLAYER_START_HEALTH;      // they are back after a respawn
    p.alive = alive;
    p.flash = (rxBuf[7] & 4) ? 1 : 0;
    p.ch = (rxBuf[7] >> 4) & 3;
  }
  else if (type == 'F')
  {
    fireShot(netInt(2), netInt(4), rxBuf[6] & 7, sender, (rxBuf[6] >> 3) & 7);
  }
  else if (type == 'U' && sender == 0 && rxBuf[2] < PICKUP_MAX && rxBuf[3] <= W_BOMB)
  {
    pickups[rxBuf[2]].type = rxBuf[3];
    pickups[rxBuf[2]].x = netInt(4);
    pickups[rxBuf[2]].y = netInt(6);
  }
  else if (type == 'W' && sender == 0 && rxBuf[2] < MAX_PLAYERS && rxBuf[3] <= W_BOMB)
  {
    players[rxBuf[2]].weapon = rxBuf[3];
    if (rxBuf[2] == myId) sfx(SFX_PICKUP);
  }
  else if (type == 'H')
  {
    applyHit(sender, rxBuf[2], rxBuf[3]);
  }
  else if (type == 'L' && sender == 0)
  {
    flagHolder = rxBuf[2];
    flagX = netInt(3);
    flagY = netInt(5);
  }
  else if (type == 'Q' && sender == 0)
  {
    for (byte i = 0; i < MAX_PLAYERS; i++) score[i] = rxBuf[2 + i];
    if (gameMode == MODE_FLAG) sfx(SFX_CAPTURE);
    checkWin();
  }
  else if (type == 'K' && myId == 0)
  {
    if (rxBuf[2] == 2) { if (nearPickup(players[sender], rxBuf[3])) hostGive(sender, rxBuf[3]); }
    else if (gameMode == MODE_FLAG) hostClaim(sender, rxBuf[2]);
  }
}

// states ////////////////////////////////////////////////////////////////////

// updateMe
// our own Amy: buttons, walls, shooting, respawning
void updateMe()
{
  Player& me = players[myId];

  if (me.flash > 0) me.flash--;

  if (!me.alive)
  {
    me.walking = 0;
    if (me.respawn > 0) me.respawn--;
    else spawnPlayer(myId);
    return;
  }

  bool rungun = arduboy.pressed(A_BUTTON);
  bool standgun = arduboy.pressed(B_BUTTON);

  // left wins from right, up wins from down
  char vx = 0;
  char vy = 0;
  if (arduboy.pressed(LEFT_BUTTON)) vx = -1;
  else if (arduboy.pressed(RIGHT_BUTTON)) vx = 1;
  if (arduboy.pressed(UP_BUTTON)) vy = -1;
  else if (arduboy.pressed(DOWN_BUTTON)) vy = 1;
  me.walking = (vx || vy);

  if (flagHolder == myId && (arduboy.frameCount & 3) == 0) { vx = 0; vy = 0; }   // the flag is heavy: a quarter slower
  if (!boxBlocked(me.x + vx, me.y)) me.x += vx;
  if (!boxBlocked(me.x, me.y + vy)) me.y += vy;

  // holding A strafes: Amy keeps facing the same way while moving
  byte facing = pgm_read_byte(&facingByMove[(vy + 1) * 3 + (vx + 1)]);
  if (!rungun && facing != 255) me.dir = facing;

  // shooting
  if (shotDelay > 0) shotDelay--;
  if ((rungun || standgun) && shotDelay == 0)
  {
    int bx = me.x + 7 + bulletVx(me.dir) * 2;
    int by = me.y + 8 + bulletVy(me.dir) * 2;
    fireShot(bx, by, me.dir, myId, me.weapon);
    sendFire(bx, by, me.dir, me.weapon);
    shotDelay = BLAST(me.weapon) ? ROCKET_DELAY : (me.weapon == W_FLAME ? FLAME_DELAY : SHOT_DELAY);
  }

  if (arduboy.everyXFrames(6) && me.walking) me.frame = (me.frame + 1) & 3;
}

// updateRemotes
// the other Amys glide toward the position the network told us about
void updateRemotes()
{
  for (byte i = 0; i < MAX_PLAYERS; i++)
  {
    if (i == myId || !(joinedMask & (1 << i))) continue;
    Player& p = players[i];

    int dx = p.tx - p.x;
    int dy = p.ty - p.y;
    if (abs(dx) > 32 || abs(dy) > 32) { p.x = p.tx; p.y = p.ty; }
    else
    {
      p.x += clampi(dx, -2, 2);
      p.y += clampi(dy, -2, 2);
    }
    if (arduboy.everyXFrames(6) && p.walking) p.frame = (p.frame + 1) & 3;
  }
}

// bulletHits
// is the 2x2 bullet inside the body of this player (a box of 10 x 12 inside the 16x16 sprite)?
bool bulletHits(Bullet& b, Player& p)
{
  return b.x + 2 > p.x + 3 && b.x < p.x + 13 && b.y + 2 > p.y + 3 && b.y < p.y + 15;
}

// explode
// a rocket blows up: everybody who is close gets hurt. The victim decides, like with bullets.
void explode(int x, int y, byte owner)
{
  byte slot = 0;
  for (byte i = 0; i < BOOM_MAX; i++)
  {
    if (booms[i].t == 0) { slot = i; break; }
    if (booms[i].t < booms[slot].t) slot = i;
  }
  booms[slot].x = x;
  booms[slot].y = y;
  booms[slot].t = BOOM_LIFE;
  sfx(SFX_BOOM);

  Player& me = players[myId];
  if (owner != myId && me.alive && me.flash == 0 && !sameTeam(owner, myId) &&
      abs(me.x + 8 - x) < ROCKET_RADIUS && abs(me.y + 9 - y) < ROCKET_RADIUS)
  {
    byte left = me.health > ROCKET_DAMAGE ? me.health - ROCKET_DAMAGE : 0;
    applyHit(myId, owner, left);
    if (left > 0) me.flash = INVULN_HIT;
    sendHit(owner, left);
  }
}

// updateBullets
// moves the bullets and looks for hits.
//  - Bullets from others against us: we decide it is a hit (we know best where we are), lower our health and tell everybody.
//  - Our own bullets against the others: only a prediction on our screen. The bullet stops and a spark shows it, but the
//    damage only counts when the other player confirms with a hit message.
//  - A rocket explodes on a wall, on a player or when it runs out; whoever is in the blast decides for himself.
void updateBullets()
{
  Player& me = players[myId];

  for (byte i = 0; i < BULLET_MAX; i++)
  {
    Bullet& b = bullets[i];
    if (b.life == 0) continue;

    b.life--;
    if (!BLAST(b.type) || (b.life & 1) == 0)         // rockets and bombs fly at half speed
    {
      b.x += b.vx;
      b.y += b.vy;
      if (b.type == W_BOMB && (pixelSolid(b.x, b.y) || pixelSolid(b.x + 1, b.y + 1)))     // a wall: step back and bounce
      {
        b.x -= b.vx;
        b.y -= b.vy;
        bool hx = pixelSolid(b.x + b.vx, b.y) || pixelSolid(b.x + b.vx + 1, b.y + 1);
        bool hy = pixelSolid(b.x, b.y + b.vy) || pixelSolid(b.x + 1, b.y + b.vy + 1);
        if (hx || !hy) b.vx = -b.vx;
        if (hy || !hx) b.vy = -b.vy;
      }
    }

    bool hit = pixelSolid(b.x, b.y) || pixelSolid(b.x + 1, b.y + 1) || (b.life == 0 && BLAST(b.type));
    if (!hit && b.owner != myId)
    {
      if (me.alive && me.flash == 0 && !sameTeam(b.owner, myId) && bulletHits(b, me))
      {
        hit = true;
        if (!BLAST(b.type))
        {
          byte dmg = b.type == W_DOUBLE ? DOUBLE_DAMAGE : 1;
          byte left = me.health > dmg ? me.health - dmg : 0;
          applyHit(myId, b.owner, left);
          if (left > 0) me.flash = b.type == W_FLAME ? INVULN_FLAME : INVULN_HIT;
          sendHit(b.owner, left);
        }
      }
    }
    else if (!hit)
    {
      for (byte k = 0; k < MAX_PLAYERS; k++)
      {
        if (k == myId || !(joinedMask & (1 << k)) || sameTeam(k, myId)) continue;
        Player& p = players[k];
        if (p.alive && p.flash == 0 && bulletHits(b, p))
        {
          hit = true;
          if (!BLAST(b.type)) addSpark(b.x, b.y);
          break;
        }
      }
    }
    if (hit)
    {
      b.life = 0;
      if (BLAST(b.type)) explode(b.x, b.y, b.owner);
    }
  }

  for (byte i = 0; i < SPARK_MAX; i++)
    if (sparks[i].t > 0) sparks[i].t--;
  for (byte i = 0; i < BOOM_MAX; i++)
    if (booms[i].t > 0) booms[i].t--;
}

// updateFlag
// the flag game: everybody notices when we touch the flag or arrive home with it; the host decides what happens
void updateFlag()
{
  if (gameMode != MODE_FLAG) return;
  Player& me = players[myId];
  static byte claimTimer = 0;
  static byte flagTick = 0;
  if (claimTimer > 0) claimTimer--;

  if (myId == 0)
  {
    if (flagHolder == 255 && flagTimer > 0 && --flagTimer == 0)
    {
      flagX = flagHomeX();                              // nobody picked it up: back to its pocket
      flagY = flagHomeY();
      sendFlag();
    }
    if (++flagTick >= 30) { flagTick = 0; sendFlag(); }    // now and then, in case a message got lost
  }

  if (!me.alive) return;
  if (flagHolder == 255 && isNear(me, flagX, flagY, 10))
  {
    if (myId == 0) hostPickup(0);
    else if (claimTimer == 0) { sendClaim(0); claimTimer = 10; }
  }
  else if (flagHolder == myId && nearHome(myId, 14))
  {
    if (myId == 0) hostCapture(0);
    else if (claimTimer == 0) { sendClaim(1); claimTimer = 10; }
  }
}

// updatePickups
// the host puts new weapons out and repeats them now and then; everybody takes the one they walk over
void updatePickups()
{
  Player& me = players[myId];
  static byte pickTimer = 0;
  static byte pickTick = 0;
  if (pickTimer > 0) pickTimer--;

  if (myId == 0)
  {
    for (byte i = 0; i < PICKUP_MAX; i++)
      if (pickups[i].type == 0 && pickups[i].timer > 0 && --pickups[i].timer == 0) hostSpawnPickup(i);
    if (++pickTick >= 20)                           // in case a message got lost: one slot or one player every 1/3 s
    {
      pickTick = 0;
      static byte rot = 0;
      rot = (rot + 1) & 7;
      if (rot < PICKUP_MAX) sendPickup(rot);
      else if (rot - PICKUP_MAX < MAX_PLAYERS && (joinedMask & (1 << (rot - PICKUP_MAX)))) sendWeapon(rot - PICKUP_MAX);
    }
  }

  if (!me.alive) return;
  for (byte i = 0; i < PICKUP_MAX; i++)
  {
    if (pickups[i].type == 0 || !nearPickup(me, i)) continue;
    if (myId == 0) hostGive(0, i);
    else if (pickTimer == 0) { sendClaim(2, i); pickTimer = 10; }
  }
}

// the flag always waves: 4 frames, 8 game frames each
void drawFlagAt(int x, int y)
{
  sprites.drawPlusMask(x, y, Flag_plus_mask, (arduboy.frameCount >> 3) & 3);
}

// the little compass arrow (5x5, 8 frames, with mask), clockwise from north: N NE E SE S SW W NW
const unsigned char PROGMEM arrow_plus_mask[] =
{
  5, 5,
  0x08, 0x1e, 0x0c, 0x1f, 0x0e, 0x1f, 0x0c, 0x1f, 0x08, 0x1e,
  0x02, 0x0f, 0x06, 0x1f, 0x0e, 0x1f, 0x1e, 0x1f, 0x00, 0x1f,
  0x00, 0x1f, 0x1f, 0x1f, 0x0e, 0x1f, 0x04, 0x1f, 0x00, 0x0e,
  0x08, 0x1e, 0x0c, 0x1f, 0x0e, 0x1f, 0x0f, 0x1f, 0x00, 0x1f,
  0x02, 0x0f, 0x06, 0x1f, 0x0e, 0x1f, 0x06, 0x1f, 0x02, 0x0f,
  0x00, 0x1f, 0x0f, 0x1f, 0x0e, 0x1f, 0x0c, 0x1f, 0x08, 0x1e,
  0x00, 0x0e, 0x04, 0x1f, 0x0e, 0x1f, 0x1f, 0x1f, 0x00, 0x1f,
  0x00, 0x1f, 0x1e, 0x1f, 0x0e, 0x1f, 0x06, 0x1f, 0x02, 0x0f
};

// the little animated flag (7x8, 4 frames, with mask): compass pointer and the flag above a carrier's head
const unsigned char PROGMEM smallFlag_plus_mask[] =
{
  7, 8,
  0x00, 0xff, 0x7e, 0xff, 0x7c, 0xff, 0x78, 0xfe, 0x38, 0xfc, 0x10, 0x7c, 0x00, 0x38,
  0x00, 0xff, 0x7e, 0xff, 0x3e, 0xff, 0x1c, 0x7f, 0x18, 0x3e, 0x08, 0x3c, 0x00, 0x1c,
  0x00, 0xff, 0x7e, 0xff, 0x1e, 0xff, 0x0e, 0x3f, 0x06, 0x1f, 0x04, 0x0f, 0x00, 0x0e,
  0x00, 0xff, 0x7e, 0xff, 0x3e, 0xff, 0x1c, 0x7f, 0x1c, 0x3e, 0x0c, 0x3e, 0x00, 0x1e
};

void __attribute__((noinline)) drawSmallFlag(int x, int y)
{
  sprites.drawPlusMask(x, y, smallFlag_plus_mask, (arduboy.frameCount >> 3) & 3);
}

// a frame with the corners cut off (instead of the library's round rectangle, which needs a lot of flash)
void drawBox(int x, int y, byte w, byte h)
{
  arduboy.drawRect(x, y, w, h, WHITE);
  for (byte k = 0; k < 4; k++) arduboy.drawPixel((k & 1) ? x + w - 1 : x, (k & 2) ? y + h - 1 : y, BLACK);
}

// the frame of a card: thick (3 lines) when selected, 2 lines when not
void drawFrame(int x, int y, byte w, byte h, bool sel)
{
  drawBox(x, y, w, h);
  drawBox(x + 2, y + 2, w - 4, h - 4);
  if (sel) drawBox(x + 1, y + 1, w - 2, h - 2);
}

// drawMarker
// a player's number at the edge of the screen points to a fighter that is out of sight
// where something at a point of the arena shows on the screen when it is w x h pixels: top left in px, py, kept inside the screen.
// Returns true when the point itself is out of sight (so it is an edge marker)
bool edgeOf(int wx, int wy, byte w, byte h, int &px, int &py)
{
  int x = wx - camX, y = wy - camY;
  px = clampi(x - (w >> 1), 0, WIDTH - w);
  py = clampi(y - (h >> 1), HUD_H, HEIGHT - h);
  return !(x >= 0 && x < WIDTH && y >= HUD_H && y < HEIGHT);
}

void drawMarker(int wx, int wy, char label)
{
  int x, y;
  if (!edgeOf(wx, wy, 7, 7, x, y)) return;
  char t[2] = { label, 0 };
  arduboy.fillRect(x, y, 7, 7, BLACK);
  teamarg_fat_small_drawText(x, y, t);
}

// the compass pointer of the flag mode (the flag sprite, or the home dot while we carry it) as it sits on the edge now:
// returns false when it is not on the edge; px,py = top left, pw,ph = size
byte arrowFrame = 0;
const signed char PROGMEM arrowSpot[16] = { 0, -20, 14, -14, 20, 0, 14, 14, 0, 20, -14, 14, -20, 0, -14, -14 };    // x, y of the arrow around us, per direction

bool flagPointer(int &px, int &py, int &pw, int &ph)
{
  if (gameMode != MODE_FLAG) return false;
  bool dot = (flagHolder == myId);
  int wx = flagX, wy = flagY;
  if (dot) { wx = pgm_read_word(&spawnX[myId]) + 8; wy = pgm_read_word(&spawnY[myId]) + 8; }
  else if (flagHolder != 255) { wx = players[flagHolder].x + 8; wy = players[flagHolder].y + 10; }
  pw = dot ? 5 : 7;
  ph = dot ? 5 : 8;
  if (!edgeOf(wx, wy, pw, ph, px, py)) return false;
  if (dot)                                             // home arrow: on one of 8 spots around us, pointing to the home base
  {
    Player& me = players[myId];
    int dx = wx - (me.x + 8), dy = wy - (me.y + 8);
    int adx = abs(dx), ady = abs(dy);
    byte f;                                            // frames: 0 N, 1 NE, 2 E, 3 SE, 4 S, 5 SW, 6 W, 7 NW
    if (ady * 2 < adx) f = dx > 0 ? 2 : 6;
    else if (adx * 2 < ady) f = dy > 0 ? 4 : 0;
    else f = dx > 0 ? (dy > 0 ? 3 : 1) : (dy > 0 ? 5 : 7);
    arrowFrame = f;
    px = clampi(me.x - camX + 6 + (char)pgm_read_byte(&arrowSpot[f * 2]), 0, WIDTH - 5);
    py = clampi(me.y - camY + 6 + (char)pgm_read_byte(&arrowSpot[f * 2 + 1]), HUD_H, HEIGHT - 5);
  }
  return true;
}

// drawCompass
// DUEL: a number at the edge of the screen for every other fighter that is out of sight
void drawCompass()
{
  if (gameMode == MODE_FLAG)
  {
    int ax, ay, aw, ah;                                // the flag (or, while we carry it, the arrow to our home base)
    if (flagPointer(ax, ay, aw, ah))
    {
      if (flagHolder == myId) sprites.drawPlusMask(ax, ay, arrow_plus_mask, arrowFrame);
      else drawSmallFlag(ax, ay);
    }
    return;
  }
  if (gameMode != MODE_DUEL) return;
  for (byte i = 0; i < MAX_PLAYERS; i++)
    if (i != myId && (joinedMask & (1 << i)) && players[i].alive) drawMarker(players[i].x + 8, players[i].y + 8, '1' + i);
}

void drawFlag()
{
  if (gameMode == MODE_FLAG && flagHolder == 255) drawFlagAt(flagX - 6 - camX, flagY - 14 - camY);
}

// the weapon pickups (16x16, 4 frames, with mask) on a white disc: bomb, double gun, flame, rocket
const unsigned char PROGMEM weaponPickup_plus_mask[] =
{
  16, 16,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x60, 0xf0, 0xf0, 0xf8, 0xd0, 0xf8, 0x90, 0xf8, 0xe0, 0xf8,
  0x18, 0xfc, 0x3c, 0xfe, 0xb4, 0xfe, 0xa4, 0xfe, 0xb8, 0xfc, 0x00, 0xb8, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x04, 0x04, 0x0e, 0x06, 0x0f, 0x03, 0x07, 0x00, 0x03, 0x01, 0x03, 0x01, 0x23, 0x20, 0x73,
  0x30, 0x7f, 0x1b, 0x3f, 0x07, 0x1f, 0x0e, 0x1f, 0x0c, 0x1f, 0x07, 0x0f, 0x00, 0x07, 0x00, 0x00,
  0x00, 0x00, 0x00, 0xe0, 0xe0, 0xf0, 0xf0, 0xf8, 0xb0, 0xf8, 0x70, 0xf8, 0xb0, 0xf8, 0xb0, 0xf8,
  0xb0, 0xf8, 0xb0, 0xfc, 0xb8, 0xfc, 0xb0, 0xfc, 0x80, 0xf8, 0xc0, 0xe0, 0x80, 0xe0, 0x00, 0xc0,
  0x00, 0x00, 0x00, 0x07, 0x03, 0x07, 0x03, 0x07, 0x00, 0x3f, 0x1f, 0x3f, 0x1f, 0x3f, 0x05, 0x3f,
  0x03, 0x0f, 0x01, 0x07, 0x01, 0x03, 0x01, 0x03, 0x01, 0x03, 0x01, 0x03, 0x01, 0x03, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x80, 0x80, 0xe0, 0xc0, 0xf0, 0x00, 0xf8, 0xc0, 0xfc, 0x10, 0xfe, 0xe2, 0xff,
  0x8c, 0xff, 0x70, 0xfe, 0xe0, 0xfc, 0x00, 0xf8, 0x60, 0xf0, 0x80, 0xe0, 0x00, 0x80, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x03, 0x1f, 0x1e, 0x3f, 0x18, 0x3f, 0x3d, 0x7f, 0x3f, 0x7f, 0x3f, 0x7f,
  0x3f, 0x7f, 0x3b, 0x7f, 0x3c, 0x7f, 0x1f, 0x3f, 0x0e, 0x1f, 0x03, 0x0f, 0x00, 0x03, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0xc0,
  0x40, 0xe0, 0x20, 0xf0, 0x00, 0xf8, 0x00, 0x7c, 0x00, 0x3c, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x08, 0x3c, 0x14, 0x3e, 0x02, 0x1f, 0x01, 0x0f, 0x00, 0x07,
  0x00, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// the white disc behind them (16x16, no mask)
const unsigned char PROGMEM pickupDisc[] =
{
  0xe0, 0xf8, 0xfc, 0xfe, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xfe, 0xfc, 0xf8, 0xe0,
  0x07, 0x1f, 0x3f, 0x7f, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0x7f, 0x3f, 0x1f, 0x07
};

// on the field: the pickup itself, sliding in at the edge with the background. completely out of sight: the same 4 pixel dot for every weapon, blinking at the edge
void drawPickups()
{
  int fx = 0, fy = 0, fw = 0, fh = 0;
  bool hasFlag = flagPointer(fx, fy, fw, fh);        // the flag (or home dot) pointer always stays visible
  for (byte i = 0; i < PICKUP_MAX; i++)
  {
    byte type = pickups[i].type;
    if (type == 0) continue;
    int x = pickups[i].x - camX;
    int y = pickups[i].y - camY;
    bool onScreen = (x > -8 && x < WIDTH + 8 && y > HUD_H - 8 && y < HEIGHT + 8);      // any part of it in sight: it slides in with the background
    int ax, ay, aw;                                  // the box this one takes
    if (onScreen) { ax = x - 8; ay = y - 8; aw = 16; }
    else { aw = 4; edgeOf(pickups[i].x, pickups[i].y, 4, 4, ax, ay); }
    if (hasFlag && ax < fx + fw && ax + aw > fx && ay < fy + fh && ay + aw > fy) continue;

    if (onScreen)
    {
      arduboy.drawBitmap(x - 8, y - 8, pickupDisc, 16, 16, WHITE);
      // the icon bobs 1 pixel. Frames: 0 bomb, 1 double gun, 2 flame, 3 rocket (weapon types 4, 1, 3, 2)
      sprites.drawPlusMask(x - 8, y - 8 + ((arduboy.frameCount >> 4) & 1), weaponPickup_plus_mask, (type & 3) ^ ((type >> 1) & 1));
    }
    else if (!(arduboy.frameCount & 8))
    {
      arduboy.fillRect(ax + 1, ay, 2, 4, WHITE);        // a round 4 pixel dot
      arduboy.fillRect(ax, ay + 1, 4, 2, WHITE);
    }
  }
}

// 8 dots around a point, r pixels away (the diagonal ones a bit closer)
void drawRing(int x, int y, byte r)
{
  for (char a = -1; a <= 1; a++)
    for (char c = -1; c <= 1; c++)
    {
      if (!a && !c) continue;
      int k = (a && c) ? r - r / 4 : r;
      arduboy.drawPixel(x + (a ? (a > 0 ? k : -k) : 0), y + (c ? (c > 0 ? k : -k) : 0), WHITE);
    }
}

// an explosion: three rings of dots that grow to the size of the blast
void drawBooms()
{
  for (byte i = 0; i < BOOM_MAX; i++)
  {
    if (booms[i].t == 0) continue;
    byte r = 3 + (BOOM_LIFE - booms[i].t) * 2;
    int x = booms[i].x - camX, y = booms[i].y - camY;
    drawRing(x, y, r);
    drawRing(x, y, r * 2 / 3);
    drawRing(x, y, r / 3);
    arduboy.drawPixel(x, y, WHITE);
  }
}

// the thrown grenade (6x8, 4 frames, with mask)
const unsigned char PROGMEM Bombs_plus_mask[] =
{
  6, 8,
  0x00, 0x20, 0x20, 0x70, 0x10, 0x3c, 0x0c, 0x1e, 0x0c, 0x1e, 0x00, 0x0c,
  0x00, 0x04, 0x04, 0x0e, 0x08, 0x3c, 0x30, 0x78, 0x30, 0x78, 0x00, 0x30,
  0x00, 0x30, 0x30, 0x78, 0x30, 0x78, 0x08, 0x3c, 0x04, 0x0e, 0x00, 0x04,
  0x00, 0x18, 0x18, 0x3c, 0x18, 0x3c, 0x20, 0x78, 0x40, 0xe0, 0x00, 0x40
};

// how high the grenade is above the ground in each frame of a hop (24 frames); the last hops are half as high
const byte PROGMEM grenadeHop[24] = { 0, 1, 3, 4, 5, 6, 7, 8, 8, 9, 9, 9, 9, 9, 9, 9, 8, 8, 7, 6, 5, 4, 3, 1 };

// the flame of the flamethrower (8x8, 4 frames small to big, with mask)
const unsigned char PROGMEM Flame_plus_mask[] =
{
  8, 8,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x70, 0xf8, 0x78, 0xfc, 0x60, 0xf8, 0x00, 0x60, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x30, 0x30, 0x78, 0x60, 0xfc, 0x5c, 0xfe, 0x78, 0xfc, 0x20, 0x78, 0x00, 0x20,
  0x00, 0x00, 0x00, 0x30, 0x30, 0x7c, 0x7c, 0xfe, 0x62, 0xff, 0x78, 0xfe, 0x30, 0x78, 0x00, 0x30,
  0x00, 0x30, 0x30, 0x7c, 0x7c, 0xfe, 0x78, 0xff, 0x77, 0xff, 0x7c, 0xff, 0x38, 0x7c, 0x00, 0x38
};

void drawBullets()
{
  for (byte i = 0; i < BULLET_MAX; i++)
  {
    Bullet& b = bullets[i];
    if (b.life == 0) continue;
    int x = b.x - camX;
    int y = b.y - camY;
    if (x >= 0 && y >= HUD_H && x < 127 && y < 63) {
      if (b.type == W_BOMB)                              // the grenade hops along the ground (a shadow stays there) and tumbles; the hops get lower
      {
        arduboy.drawFastHLine(x - 1, y + 3, 4, WHITE);
        sprites.drawPlusMask(x - 2, y - 3 - (pgm_read_byte(grenadeHop + b.life % 24) >> (b.life < 50)), Bombs_plus_mask, (arduboy.frameCount >> 2) & 3);
      }
      else if (b.type == W_FLAME) sprites.drawPlusMask(x - 3, y - 3, Flame_plus_mask, (FLAME_LIFE - b.life) / 4);      // a flame grows while it flies: 4 frames, small to big
      else if (b.type == W_ROCKET)                       // two lines (8 pixels, on the diagonals 6) with a flickering exhaust pixel behind
      {
        char dx = (b.vx > 0) - (b.vx < 0), dy = (b.vy > 0) - (b.vy < 0);
        bool diag = dx && dy;
        char px = diag ? dx : -dy, py = diag ? 0 : dx;   // the second line lies next to the first
        byte len = diag ? 6 : 8;
        int bx = x + 1, by = y + 1;                      // the head, then backwards
        for (byte i = 0; i <= len; i++)
        {
          if (i == len)
          {
            if (arduboy.frameCount & 1) { bx += px; by += py; }
            arduboy.drawPixel(bx, by, WHITE);
          }
          else
          {
            arduboy.drawPixel(bx, by, WHITE);
            arduboy.drawPixel(bx + px, by + py, WHITE);
          }
          bx -= dx;
          by -= dy;
        }
      }
      else arduboy.fillRect(x, y, 2, 2, WHITE);
    }
  }
}

void drawSparks()
{
  for (byte i = 0; i < SPARK_MAX; i++)
  {
    if (sparks[i].t == 0) continue;
    int x = sparks[i].x - camX;
    int y = sparks[i].y - camY;
    byte r = 1 + (SPARK_LIFE - sparks[i].t) / 2;           // grows from 1 to 5 pixels
    for (char a = -1; a <= 1; a++)                         // the middle, 4 diagonal and 4 straight dots
      for (char c = -1; c <= 1; c++)
      {
        char k = (a && c) ? r : r + 1;
        arduboy.drawPixel(x + a * k, y + c * k, WHITE);
      }
  }
}

void drawPlayers()
{
  for (byte i = 0; i < MAX_PLAYERS; i++)
  {
    Player& p = players[i];
    if (!(joinedMask & (1 << i)) || !p.alive) continue;
    int x = p.x - camX;
    int y = p.y - camY;
    if (x < -16 || x > 127 || y < -16 || y > 63) continue;

    // a blinking Amy can't be hurt
    if (!(p.flash && (arduboy.frameCount & 4))) drawAmy(x, y, p.dir, p.frame, fighterChar(i));

    // hearts left: a little bar under her feet, 2 pixels per heart
    arduboy.fillRect(x + 3, y + 16, p.health * 2, 2, WHITE);

    // the carried flag waves above the carrier's head
    if (gameMode == MODE_FLAG && flagHolder == i) drawSmallFlag(x + 5, y - 9);
  }
}

// small 3x5 HUD font (no mask): 0-9, P, A, B, one byte per column, LSB = top row
const byte smallFont[13][3] PROGMEM =
{
  {0x1F,0x11,0x1F}, {0x12,0x1F,0x10}, {0x1D,0x15,0x17}, {0x11,0x15,0x1F}, {0x07,0x04,0x1F},
  {0x17,0x15,0x1D}, {0x1F,0x15,0x1D}, {0x01,0x01,0x1F}, {0x1F,0x15,0x1F}, {0x17,0x15,0x1F},
  {0x1F,0x05,0x07},                                   // 10 = P
  {0x1F,0x05,0x1F},                                   // 11 = A
  {0x1F,0x15,0x0A}                                    // 12 = B
};

// one small glyph, 3 pixels wide; returns the x for the next one (1 pixel gap)
int drawSmallGlyph(int x, int y, byte i)
{
  arduboy.drawBitmap(x, y, smallFont[i], 3, 5, WHITE);      // drawBitmap reads from flash itself
  return x + 4;
}

// "P2:1" (first = 10, second = 2) or "A:3" (first = 11, second = 0): a label, a colon (1 pixel wide, 1 gap) and the score
void drawSmall(int x, int y, byte first, byte second, byte sc)
{
  x = drawSmallGlyph(x, y, first);
  if (second) x = drawSmallGlyph(x, y, second);
  arduboy.drawPixel(x, y + 1, WHITE);
  arduboy.drawPixel(x, y + 3, WHITE);
  drawSmallGlyph(x + 2, y, sc % 10);
}

void drawHud()
{
  Player& me = players[myId];
  arduboy.fillRect(0, 0, WIDTH, HUD_H, BLACK);
  for (byte i = 0; i < me.health; i++) arduboy.fillRect(i * 3, 1, 2, 5, WHITE);          // hearts: 2 pixels wide

  if (gameMode == MODE_TEAM)
  {
    // team scores: A:3  B:2, our own team underlined
    for (byte t = 0; t < 2; t++)
    {
      int x = 40 + t * 40;
      drawSmall(x, 1, 11 + t, 0, score[t]);
      if (teamOf(myId) == t) arduboy.drawFastHLine(x, 7, 9, WHITE);
    }
  }
  else
  {
    for (byte i = 0; i < MAX_PLAYERS; i++)
    {
      if (!(joinedMask & (1 << i))) continue;
      int x = 30 + i * 25;
      drawSmall(x, 1, 10, i + 1, score[i]);
      if (i == myId) arduboy.drawFastHLine(x, 7, 13, WHITE);
    }
  }
}

// is this character already taken by another player in the lobby?
bool charTaken(byte c)
{
  for (byte i = 0; i < MAX_PLAYERS; i++)
    if (i != myId && (rosterMask & (1 << i)) && players[i].ch == c) return true;
  return false;
}

// drawNum
// one digit in the text font
void __attribute__((noinline)) drawNum(int x, int y, byte n)
{
  char b[2] = { (char)('0' + n), 0 };
  teamarg_fat_small_drawText(x, y, b);
}

// drawWord
// a line of text in the fat font centred on cx: a word from flash (or 0) and then the players of a mask: "WAITING P2 P4"
void drawWord(int cx, int y, const __FlashStringHelper *text, byte mask = 0)
{
  char b[24];
  byte n = 0;
  if (text) { strcpy_P(b, (const char *)text); n = strlen(b); }
  for (byte i = 0; i < MAX_PLAYERS; i++)
    if (mask & (1 << i))
    {
      if (n) b[n++] = ' ';
      b[n++] = 'P';
      b[n++] = '1' + i;
    }
  b[n] = 0;
  teamarg_fat_small_drawText(cx - (n * 6 + 2) / 2, y, b);
}

// the arrows around a character: 1 : 3 : 5 pixels high, pointing outwards
void drawArrow(int x, bool right)
{
  int c = right ? x + 20 : x - 5, d = right ? -1 : 1;
  arduboy.drawPixel(c, 22, WHITE);
  arduboy.drawFastVLine(c + d, 21, 3, WHITE);
  arduboy.drawFastVLine(c + 2 * d, 20, 5, WHITE);
}

// drawGameCard
// one of the three game type cards: first the frame (thick when selected), then the animated characters over it:
// they have a mask, so they cut into the frame. Where they stand is the mock-up; they only walk in the direction they are shown.
// per character: card (0 duel, 1 flag, 2 team), x and y inside the card, direction, which fighter, walk frame offset
const signed char PROGMEM cardFighters[8][6] =
{
  { 0, 14,  5, 1, 0, 0 },                            // duel: Amy walks southwest, Cid southeast, Dee northwest
  { 0, -1, 12, 7, 3, 1 },
  { 0, 15, 22, 3, 1, 2 },
  { 1,  1,  4, 7, 2, 0 },                            // flag: Bob walks to the flag
  { 2,  0,  4, 7, 2, 0 },                            // team: two teams of two, Bob on the left (southeast), Dee on the right (southwest)
  { 2,  0, 21, 7, 2, 2 },
  { 2, 15,  3, 1, 1, 1 },
  { 2, 15, 20, 1, 1, 3 }
};

void drawGameCard(byte c, bool selected)
{
  int x = 8 + c * 40;
  const int y = 12;
  byte f = arduboy.frameCount >> 3;

  drawFrame(x, y, 32, 40, selected);
  for (byte i = 0; i < 8; i++)
  {
    const signed char *e = cardFighters[i];
    if ((byte)pgm_read_byte(e) == c)
      drawAmy(x + (signed char)pgm_read_byte(e + 1), y + (signed char)pgm_read_byte(e + 2), pgm_read_byte(e + 3), (f + pgm_read_byte(e + 5)) & 3, pgm_read_byte(e + 4));
  }
  if (c == MODE_FLAG) sprites.drawPlusMask(x + 15, y + 20, Flag_plus_mask, f & 3);
}

// the lobby is three screens. The host moves through them with A (B goes back), the guests see the same screen:
//   0 game type, 1 amount of wins, 2 characters (everybody picks their own; in a team game the host picks the 2 looks)
void stateLobby()
{
  static bool sendNow = false;                     // tell the others about our character / ready right now
  static byte teamSel = 0;                         // team game: the team the host is changing
  static byte walkTick = 0, walkDir = 0;           // they walk on the spot: 3 walk cycles (12 steps) per direction, all 8 directions
  bool host = (myId == 0);
  bool team = (gameMode == MODE_TEAM);
  char d = arduboy.justPressed(RIGHT_BUTTON) - arduboy.justPressed(LEFT_BUTTON);
  bool changed = false;

  if (menuStage == 0)
  {
    // game type
    if (host && d)
    {
      byte m = gameMode + d;
      gameMode = (m == 255) ? MODE_COUNT - 1 : (m == MODE_COUNT) ? 0 : m;
      changed = true;
    }
    for (byte c = 0; c < MODE_COUNT; c++)
    {
      drawGameCard(c, c == gameMode);
      drawWord(24 + c * 40, 55, FS(sModes + c * 5));
    }
    if (notice > 0) notice--;
  }
  else if (menuStage == 1)
  {
    // amount of wins: 3 5 7 9
    if (host && d) { winTarget = ((((winTarget >> 1) + 3 + d) & 3) + 1) * 2 + 1; changed = true; }   // 3 5 7 9 around
    for (byte i = 0; i < 4; i++)
    {
      byte x = 6 + i * 30;
      drawFrame(x, 16, 26, 38, winTarget == 3 + 2 * i);
      drawNum(x + 9, 32, 3 + 2 * i);
    }
  }
  else
  {
    // characters
    byte walkStep = (arduboy.frameCount >> 3) & 3;  // 8 frames per step, 4 steps per cycle
    if (!(arduboy.frameCount & 7) && ++walkTick == 12) { walkTick = 0; walkDir = (walkDir + 1) & 7; }

    if (team && host && (arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(DOWN_BUTTON))) teamSel ^= 1;
    if (d && (team ? host : !((readyMask >> myId) & 1)))     // (not after B: A first)
    {
      byte &mine = team ? teamChar[teamSel] : myChar;
      byte c = team ? mine : players[myId].ch;
      for (byte n = 0; n < MAX_PLAYERS; n++)
      {
        c = (c + d) & (MAX_PLAYERS - 1);
        if (team ? (c != teamChar[teamSel ^ 1]) : !charTaken(c)) { mine = c; break; }
      }
      changed = true;
      sendNow = true;
    }
    if (myChar != 255) players[myId].ch = myChar;
    if (sendNow || !(arduboy.frameCount & 31)) { sendNow = false; sendChar(); }   // and now and then, for players who just joined

    for (byte i = 0; i < MAX_PLAYERS; i++)
    {
      int x = 8 + i * 30;
      bool here = rosterMask & (1 << i);
      if (here) drawAmy(x, 14, walkDir, walkStep, fighterChar(i));
      else arduboy.drawRect(x + 4, 18, 8, 8, WHITE);
      if (!team && i == myId && here && !((readyMask >> i) & 1))
      {
        drawArrow(x, 0);
        drawArrow(x, 1);
      }
      if (i == myId && here) arduboy.drawFastHLine(x, 40, 16, WHITE);      // that's us
      drawWord(x + 6, 32, 0, 1 << i);                // "P1" ...
    }
    if (team && host)
    {
      drawArrow(8 + teamSel * 60, 0);                 // the pair of players that is being changed
      drawArrow(8 + teamSel * 60 + 30, 1);
    }

    // who is ready (host) / what to do (guests)
    byte wait = rosterMask & ~readyMask & 0x0E;
    byte rd = readyMask & rosterMask;
    if (host && !team)
    {
      if (hostConfirm >= 2)                          // B twice and somebody is still not ready: overrule them?
      {
        arduboy.fillRect(4, 26, 120, 37, BLACK);
        drawBox(4, 26, 120, 37);
        drawWord(64, 29, F("STILL WAITING FOR"));
        drawWord(64, 38, 0, wait);
        drawWord(64, 47, F("OVERRULE ?"));
        drawWord(64, 55, FS(sConfirm));
      }
      else if (wait)
      {
        drawWord(64, 47, FS(sWaiting), wait);
        if (rd) drawWord(64, 55, FS(sReady), rd);
      }
      else
      {
        drawWord(64, 47, FS(sReady), rd & 0x0E);
        drawWord(64, 55, FS(sStart));
      }
    }
    else
    {
      drawWord(49, 47, F("GAME TYPE:"));
      drawWord(97, 47, FS(sModes + gameMode * 5));
      if (!team) drawWord(64, 55, ((readyMask >> myId) & 1) ? FS(sReady) : FS(sConfirm));
      else if (host) drawWord(64, 55, FS(sStart));
    }
    if (notice > 0)                                  // a team game needs 4 players: the pairs blink
    {
      notice--;
      if (notice & 4) arduboy.drawRect(0, 12, 128, 24, WHITE);
    }
  }

  // the title: the host (and, picking their character, a guest in a normal game) sees what to do, the guests see who decides
  drawWord(64, 1, (host || (menuStage == 2 && !team)) ? FS(menuStage == 0 ? PSTR("PICK GAME TYPE") : (menuStage == 1 ? PSTR("HOW MANY VICTORIES?") : PSTR("PICK CHARACTER"))) : FS(sHostChooses));

  if (host)
  {
    if (changed || !(arduboy.frameCount & 31)) sendSettings();      // so the guests see the host's choices
    if (arduboy.justPressed(B_BUTTON))
    {
      if (menuStage < 2) { menuStage++; sendSettings(); }
      else if (gameMode == MODE_TEAM && linked && rosterMask != 0x0F) notice = 90;
      else if (gameMode == MODE_TEAM || !(rosterMask & ~readyMask & 0x0E) || hostConfirm >= 2)
      {
        sendGo();
        startGame();
      }
      else { hostConfirm++; readyMask |= 1; sendNow = true; }       // not everybody is ready: 1st B = I am ready, 2nd B = the question
    }
    else if (arduboy.justPressed(A_BUTTON))
    {
      if (hostConfirm) { hostConfirm = 0; readyMask &= ~1; sendNow = true; }
      else if (menuStage > 0) { menuStage--; sendSettings(); }
    }
  }
  else if (menuStage == 2 && gameMode != MODE_TEAM)      // guests: B = ready, A = changed my mind
  {
    if (arduboy.justPressed(B_BUTTON)) { readyMask |= 1 << myId; sendNow = true; }
    if (arduboy.justPressed(A_BUTTON)) { readyMask &= ~(1 << myId); sendNow = true; }
  }
}

void statePlay()
{
  updateMe();
  updateRemotes();
  updateBullets();
  updateFlag();
  updatePickups();

  if (gameState != STATE_PLAY) return;             // that last hit ended the round

  if (++sendTimer >= SEND_EVERY)
  {
    sendTimer = 0;
    sendPos();
  }

  // camera: Amy in the middle of the play field (below the score bar), clamped to the arena
  Player& me = players[myId];
  camX = clampi(me.x - 56, 0, ARENA_W - WIDTH);
  camY = clampi(me.y - 28, 0, ARENA_H - HEIGHT);

  drawLevel(camX, camY);
  drawFlag();
  drawPickups();
  drawBullets();
  drawPlayers();
  drawCompass();
  drawSparks();
  drawBooms();
  drawHud();

  if (!me.alive)                                   // seconds until we are back
  {
    arduboy.fillRect(58, 26, 11, 11, BLACK);
    drawNum(60, 28, 1 + me.respawn / 60);
  }
}

// stateOver
// no words: everybody with the score under them, the winner (or the winning team) keeps walking
void stateOver()
{
  byte f = (arduboy.frameCount >> 3) & 3;
  for (byte i = 0; i < MAX_PLAYERS; i++)
  {
    if (!(joinedMask & (1 << i))) continue;
    int x = 8 + i * 30;
    bool won = (gameMode == MODE_TEAM) ? (teamOf(i) == winner) : (i == winner);
    drawAmy(x, 18, 0, won ? f : 0, fighterChar(i));
    drawNum(x + 4, 40, score[gameMode == MODE_TEAM ? teamOf(i) : i]);
    if (i == myId) arduboy.drawFastHLine(x, 49, 16, WHITE);
  }

  // PLAYER 2 HAS WON / TEAM A HAS WON, and what the host can do now
  char t[17];
  if (gameMode == MODE_TEAM) { strcpy_P(t, PSTR("TEAM A HAS WON")); t[5] = 'A' + (winner & 1); }
  else { strcpy_P(t, PSTR("PLAYER 1 HAS WON")); t[7] = '1' + (winner & 3); }
  teamarg_fat_small_drawText(64 - (strlen(t) * 6 + 2) / 2, 1, t);
  if (myId == 0) drawWord(64, 56, F("A RETURN    B RESTART"));
  else drawWord(64, 56, FS(sHostChooses));

  // the host: B plays again, A goes back to the lobby
  if (myId == 0)
  {
    if (arduboy.justPressed(B_BUTTON))
    {
      sendGo();
      startGame();
    }
    else if (arduboy.justPressed(A_BUTTON))
    {
      sendBack();
      gameState = STATE_LOBBY;
      menuStage = 0;
    }
  }
}

#endif
