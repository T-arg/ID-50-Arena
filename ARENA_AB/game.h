#ifndef GAME_H
#define GAME_H

// facing direction for a move of (x, y), index = (y + 1) * 3 + (x + 1), 255 = not moving   (same as VIRUS LQP-79)
const byte PROGMEM facingByMove[9] = { 3, 4, 5, 2, 255, 6, 1, 0, 7 };

// the velocity of a bullet in each of the 8 directions (same as VIRUS LQP-79)
const char PROGMEM bulletVel[8] = { 0, -2, -3, -2, 0, 2, 3, 2 };

char bulletVx(byte dir) { return (char)pgm_read_byte(&bulletVel[dir & 7]); }
char bulletVy(byte dir) { return (char)pgm_read_byte(&bulletVel[(dir + 6) & 7]); }

int camX = 0;
int camY = 0;

// drawing ///////////////////////////////////////////////////////////////////

// drawFighter
// draws a 16x16 image with its mask (both 32 bytes in RAM: 2 pages of 16 columns) straight into the screen
// buffer: erase where the mask is set, then OR the image. flip mirrors it left to right.
void drawFighter(int x, int y, const byte *im, const byte *mk, bool flip)
{
  byte *buf = arduboy.getBuffer();
  byte shift = y & 7;
  for (byte c = 0; c < 16; c++)
  {
    int sx = x + c;
    if (sx < 0 || sx > 127) continue;
    byte k = flip ? 15 - c : c;
    for (byte p = 0; p < 2; p++)
    {
      byte i = (p << 4) + k;
      unsigned int m = (unsigned int)mk[i] << shift;
      unsigned int d = (unsigned int)im[i] << shift;
      for (byte h = 0; h < 2; h++)
      {
        int page = (y >> 3) + p + h;
        if (page < 0 || page > 7) continue;
        byte *s = buf + page * 128 + sx;
        *s = (*s & ~(byte)(m >> (h << 3))) | (byte)(d >> (h << 3));
      }
    }
  }
}

// drawAmy
// the four fighters: 0 Amy, 1 Dee, 2 Bob, 3 Cid. The frame is put together from its stored pages (see bitmaps.h).
void drawAmy(int x, int y, byte dir, byte frame, byte who)
{
  byte im[32], mk[32];
  const unsigned char *ix = charIdx + (who * 20 + frame + 4 * (dir <= 4 ? dir : 8 - dir)) * 4;
  for (byte p = 0; p < 2; p++)
  {
    memcpy_P(im + p * 16, charPage + pgm_read_byte(ix + p) * 16, 16);
    memcpy_P(mk + p * 16, charPage + pgm_read_byte(ix + 2 + p) * 16, 16);
  }
  drawFighter(x, y, im, mk, dir > 4);
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
      b.life = type == W_ROCKET ? BULLET_LIFE * 2 : (type == W_FLAME ? FLAME_LIFE : BULLET_LIFE);      // a rocket flies at half speed
      return;
    }
  }
}

// fireShot
// one shot of a weapon, for our own gun and for the shots of the others
void fireShot(int x, int y, byte dir, byte owner, byte weapon)
{
  if (weapon == W_DOUBLE)
  {
    char vx = bulletVx(dir), vy = bulletVy(dir);
    int ox = ((vy < 0) - (vy > 0)) * 3;               // sideways, 3 pixels either way
    int oy = ((vx > 0) - (vx < 0)) * 3;
    addBullet(x + ox, y + oy, dir, owner);
    addBullet(x - ox, y - oy, dir, owner);
  }
  else if (weapon == W_FLAME)
  {
    char vx = bulletVx(dir), vy = bulletVy(dir);
    int j = (int)((arduboy.frameCount * 5 + x) % 5) - 2;      // the stream wobbles a little sideways
    addBullet(x + ((vy < 0) - (vy > 0)) * j, y + ((vx > 0) - (vx < 0)) * j, dir, owner, W_FLAME);
  }
  else addBullet(x, y, dir, owner, weapon == W_ROCKET ? W_ROCKET : 0);
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
    }
  }
}

void startGame()
{
  buildPockets();
  if (flagPocket >= pocketCount) flagPocket = 0;
  for (byte i = 0; i < MAX_PLAYERS; i++) score[i] = 0;
  flagHolder = 255;
  flagX = flagHomeX();
  flagY = flagHomeY();
  flagTimer = 0;
  joinedMask = rosterMask;
  winner = 255;
  shotDelay = 0;
  sendTimer = 0;
  for (byte i = 0; i < BULLET_MAX; i++) bullets[i].life = 0;
  for (byte i = 0; i < SPARK_MAX; i++) sparks[i].t = 0;
  for (byte i = 0; i < BOOM_MAX; i++) booms[i].t = 0;
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
  txBuf[1] = players[myId].ch;
  netSend(2);
}

void sendHit(byte shooter, byte health)
{
  txBuf[0] = 'H';
  txBuf[1] = shooter;
  txBuf[2] = health;
  netSend(3);
}

void sendSettings()
{
  txBuf[0] = 'S';
  txBuf[1] = gameMode;
  txBuf[2] = winTarget;
  txBuf[3] = teamSet;
  txBuf[4] = teamChar[0];
  txBuf[5] = teamChar[1];
  txBuf[6] = menuStage;
  netSend(7);
}

void sendGo()
{
  mapSeed = (unsigned int)micros() ^ ((unsigned int)arduboy.frameCount << 3);      // a new random arena for this round
  buildPockets();
  flagPocket = (mapSeed >> 5) % pocketCount;
  txBuf[0] = 'G';
  txBuf[1] = gameMode;
  txBuf[2] = winTarget;
  txBuf[3] = teamSet;
  txBuf[4] = teamChar[0];
  txBuf[5] = teamChar[1];
  txBuf[6] = mapSeed & 0xFF;
  txBuf[7] = mapSeed >> 8;
  txBuf[8] = flagPocket;
  netSend(9);
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
    pickups[slot].type = 1 + rnd() % 3;
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
  pickups[slot].type = 0;
  pickups[slot].timer = PICKUP_RESPAWN;
  sendPickup(slot);
  sendWeapon(id);
}

// the flag game, host side: only the host decides who holds the flag, so everybody agrees
bool nearFlag(Player& p, int r)
{
  return abs(p.x + 8 - flagX) < r && abs(p.y + 10 - flagY) < r;
}

bool nearHome(byte id, byte r)
{
  Player& p = players[id];
  return abs(p.x - (int)pgm_read_word(&spawnX[id])) < r && abs(p.y - (int)pgm_read_word(&spawnY[id])) < r;
}

void hostPickup(byte id)
{
  flagHolder = id;
  sendFlag();
}

void hostCapture(byte id)
{
  score[id]++;
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
  if (kind == 0 && flagHolder == 255 && players[id].alive && nearFlag(players[id], 18)) hostPickup(id);
  else if (kind == 1 && flagHolder == id && nearHome(id, 26)) hostCapture(id);
}

bool nearPickup(Player& p, byte slot)
{
  return slot < PICKUP_MAX && abs(p.x + 8 - pickups[slot].x) < 20 && abs(p.y + 10 - pickups[slot].y) < 20;
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
    if (sender != 0) return;                     // only the host decides
    if (type == 'S' && gameState != STATE_LOBBY) return;     // a late settings message must not change a game that is running
    if (rxLen >= 7 && rxBuf[2] < MODE_COUNT && rxBuf[4] < 3 && rxBuf[5] < MAX_PLAYERS && rxBuf[6] < MAX_PLAYERS)
    {
      gameMode = rxBuf[2];
      winTarget = constrain(rxBuf[3], 1, 9);
      teamSet = rxBuf[4];
      teamChar[0] = rxBuf[5];
      teamChar[1] = rxBuf[6];
    }
    if (type == 'S' && rxLen >= 8 && rxBuf[7] < 3) menuStage = rxBuf[7];
    if (type == 'G' && rxLen >= 10 && rxBuf[9] < 6)
    {
      mapSeed = rxBuf[7] | ((unsigned int)rxBuf[8] << 8);
      flagPocket = rxBuf[9];
    }
    if (type == 'G') startGame();
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
    if (rxBuf[2] < MAX_PLAYERS) players[sender].ch = rxBuf[2];
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
    fireShot(netInt(2), netInt(4), rxBuf[6] & 7, sender, (rxBuf[6] >> 3) & 3);
  }
  else if (type == 'U' && sender == 0 && rxBuf[2] < PICKUP_MAX && rxBuf[3] <= W_FLAME)
  {
    pickups[rxBuf[2]].type = rxBuf[3];
    pickups[rxBuf[2]].x = netInt(4);
    pickups[rxBuf[2]].y = netInt(6);
  }
  else if (type == 'W' && sender == 0 && rxBuf[2] < MAX_PLAYERS && rxBuf[3] <= W_FLAME)
  {
    players[rxBuf[2]].weapon = rxBuf[3];
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
    shotDelay = (me.weapon == W_ROCKET) ? ROCKET_DELAY : (me.weapon == W_FLAME ? FLAME_DELAY : SHOT_DELAY);
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
      p.x += constrain(dx, -2, 2);
      p.y += constrain(dy, -2, 2);
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
    if (b.type != W_ROCKET || (b.life & 1) == 0)     // rockets fly at half speed
    {
      b.x += b.vx;
      b.y += b.vy;
    }

    if (pixelSolid(b.x, b.y) || pixelSolid(b.x + 1, b.y + 1) || (b.life == 0 && b.type == W_ROCKET))
    {
      b.life = 0;
      if (b.type == W_ROCKET) explode(b.x, b.y, b.owner);
      continue;
    }

    if (b.owner != myId)
    {
      if (me.alive && me.flash == 0 && !sameTeam(b.owner, myId) && bulletHits(b, me))
      {
        b.life = 0;
        if (b.type == W_ROCKET) explode(b.x, b.y, b.owner);
        else
        {
          byte left = me.health - 1;
          applyHit(myId, b.owner, left);
          if (left > 0) me.flash = b.type == W_FLAME ? INVULN_FLAME : INVULN_HIT;
          sendHit(b.owner, left);
        }
      }
    }
    else
    {
      for (byte k = 0; k < MAX_PLAYERS; k++)
      {
        if (k == myId || !(joinedMask & (1 << k)) || sameTeam(k, myId)) continue;
        Player& p = players[k];
        if (p.alive && p.flash == 0 && bulletHits(b, p))
        {
          b.life = 0;
          if (b.type == W_ROCKET) explode(b.x, b.y, b.owner);
          else addSpark(b.x, b.y);
          break;
        }
      }
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
  if (flagHolder == 255 && nearFlag(me, 10))
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

void drawSmallFlag(int x, int y)
{
  sprites.drawPlusMask(x, y, smallFlag_plus_mask, (arduboy.frameCount >> 3) & 3);
}

// drawMarker
// something far away: at the edge of the screen the little flag, or a player's number, points to it
void drawMarker(int wx, int wy, char label = 0)
{
  int x = wx - camX;
  int y = wy - camY;
  if (x >= 0 && x < WIDTH && y >= HUD_H && y < HEIGHT) return;
  if (label)
  {
    char t[2] = { label, 0 };
    x = constrain(x - 3, 0, WIDTH - 8);
    y = constrain(y - 3, HUD_H, HEIGHT - 7);
    arduboy.fillRect(x, y, 7, 7, BLACK);
    teamarg_fat_small_drawText(x, y, t);
    return;
  }
  drawSmallFlag(constrain(x - 3, 0, WIDTH - 7), constrain(y - 4, HUD_H, HEIGHT - 8));
}

// the compass pointer of the flag mode (the flag sprite, or the home dot while we carry it) as it sits on the edge now:
// returns false when it is not on the edge; px,py = top left, pw,ph = size
bool flagPointer(int &px, int &py, int &pw, int &ph)
{
  if (gameMode != MODE_FLAG) return false;
  bool dot = (flagHolder == myId);
  int wx = flagX, wy = flagY;
  if (dot) { wx = pgm_read_word(&spawnX[myId]) + 8; wy = pgm_read_word(&spawnY[myId]) + 8; }
  else if (flagHolder != 255) { wx = players[flagHolder].x + 8; wy = players[flagHolder].y + 10; }
  int x = wx - camX, y = wy - camY;
  if (x >= 0 && x < WIDTH && y >= HUD_H && y < HEIGHT) return false;
  if (dot)                                             // home dot: on a ring around us, pointing to the home base
  {
    Player& me = players[myId];
    int dx = wx - (me.x + 8), dy = wy - (me.y + 8);
    int ax = abs(dx), ay = abs(dy);
    int len = (ax > ay ? ax : ay) + (ax > ay ? ay : ax) / 2;       // about the real distance
    if (len == 0) len = 1;
    px = constrain(me.x - camX + 8 + dx * 20 / len - 2, 0, WIDTH - 5);
    py = constrain(me.y - camY + 8 + dy * 20 / len - 2, HUD_H, HEIGHT - 5);
    pw = 5; ph = 5;
  }
  else { px = constrain(x - 3, 0, WIDTH - 7); py = constrain(y - 4, HUD_H, HEIGHT - 8); pw = 7; ph = 8; }
  return true;
}

// drawCompass
// DUEL: a number at the edge of the screen for every other fighter that is out of sight
void drawCompass()
{
  if (gameMode == MODE_FLAG)
  {
    if (flagHolder == myId)                                                                                            // carry it home: a dot around us
    {
      int ax, ay, aw, ah;
      if (flagPointer(ax, ay, aw, ah))
      {
        Player& me = players[myId];
        int dx = pgm_read_word(&spawnX[myId]) - me.x, dy = pgm_read_word(&spawnY[myId]) - me.y;
        int adx = abs(dx), ady = abs(dy);
        byte f;                                                        // frames: 0 N, 1 NE, 2 E, 3 SE, 4 S, 5 SW, 6 W, 7 NW
        if (ady * 2 < adx) f = dx > 0 ? 2 : 6;
        else if (adx * 2 < ady) f = dy > 0 ? 4 : 0;
        else f = dx > 0 ? (dy > 0 ? 3 : 1) : (dy > 0 ? 5 : 7);
        sprites.drawPlusMask(ax, ay, arrow_plus_mask, f);
      }
    }
    else if (flagHolder == 255) drawMarker(flagX, flagY);                                                              // the flag on the floor
    else drawMarker(players[flagHolder].x + 8, players[flagHolder].y + 10);                                            // somebody else has it: follow him
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

// the weapon icons (8 x 8): double gun = two bars, rocket. Index = weapon - 1. Placeholders, draw your own!
const unsigned char PROGMEM weaponIcon[3][8] =
{
  { 0x00, 0x6C, 0x6C, 0x6C, 0x6C, 0x6C, 0x6C, 0x00 },
  { 0x66, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x18, 0x08 },
  { 0x00, 0x0C, 0x7E, 0xFF, 0x7E, 0x3C, 0x18, 0x00 },        // flame placeholder
};

// drawPickups
// the weapons that lie in the arena: a black plate with the icon, blinking now and then.
// One that is out of sight is pointed out by its icon, blinking at the edge of the screen.
// the double gun pickup (16x16, 2 frames, with mask)
const unsigned char PROGMEM doublegunpickup_plus_mask[] =
{
  16, 16,
  0xe0, 0xf8, 0x08, 0xfc, 0xf4, 0xfe, 0xfa, 0xff, 0x5a, 0xff, 0x3b, 0xff, 0x1b, 0xff, 0x9b, 0xff,
  0x9b, 0xff, 0x99, 0xff, 0x9d, 0xff, 0x98, 0xff, 0x82, 0xff, 0x9c, 0xfe, 0xd8, 0xfc, 0x80, 0xf8,
  0x07, 0x1f, 0x1c, 0x3f, 0x3d, 0x7f, 0x7d, 0xff, 0x7c, 0xff, 0xc0, 0xff, 0xdf, 0xff, 0xdf, 0xff,
  0xc5, 0xff, 0xf3, 0xff, 0xf9, 0xff, 0x7d, 0xff, 0x7d, 0xff, 0x3d, 0x7f, 0x1d, 0x3f, 0x05, 0x1f,
  0xe0, 0xf8, 0x18, 0xfc, 0xec, 0xfe, 0xf6, 0xff, 0xb6, 0xff, 0x77, 0xff, 0x37, 0xff, 0x37, 0xff,
  0x37, 0xff, 0x33, 0xff, 0x3b, 0xff, 0x32, 0xff, 0x06, 0xff, 0x3c, 0xfe, 0xb8, 0xfc, 0x20, 0xf8,
  0x07, 0x1f, 0x18, 0x3f, 0x3b, 0x7f, 0x7b, 0xff, 0x78, 0xff, 0x80, 0xff, 0xbe, 0xff, 0xbf, 0xff,
  0x8b, 0xff, 0xe7, 0xff, 0xf3, 0xff, 0x7b, 0xff, 0x7b, 0xff, 0x3b, 0x7f, 0x1b, 0x3f, 0x03, 0x1f
};

// on the field: the pickup itself. out of sight: the same 4 pixel dot for every weapon, blinking at the edge
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
    bool onScreen = (x >= 8 && x < WIDTH - 8 && y >= HUD_H + 8 && y < HEIGHT - 8);
    int ax, ay, aw;                                  // the box this one takes
    if (onScreen) { ax = x - 8; ay = y - 8; aw = 16; }
    else { ax = constrain(x - 2, 0, WIDTH - 4); ay = constrain(y - 2, HUD_H, HEIGHT - 4); aw = 4; }
    if (hasFlag && ax < fx + fw && ax + aw > fx && ay < fy + fh && ay + aw > fy) continue;

    if (onScreen)
    {
      if (type == W_DOUBLE) sprites.drawPlusMask(x - 8, y - 8, doublegunpickup_plus_mask, (arduboy.frameCount >> 4) & 1);
      else
      {
        arduboy.fillRect(x - 5, y - 5, 10, 10, BLACK);
        arduboy.drawBitmap(x - 4, y - 4, weaponIcon[type - 1], 8, 8, WHITE);
        if (arduboy.frameCount & 16) arduboy.drawRect(x - 5, y - 5, 10, 10, WHITE);
      }
    }
    else if (!(arduboy.frameCount & 8))
    {
      arduboy.fillRect(ax + 1, ay, 2, 4, WHITE);        // a round 4 pixel dot
      arduboy.fillRect(ax, ay + 1, 4, 2, WHITE);
    }
  }
}

// drawBooms
// a rocket explosion: a ring that grows to the size of the blast
void drawBooms()
{
  for (byte i = 0; i < BOOM_MAX; i++)
  {
    if (booms[i].t == 0) continue;
    byte r = 3 + (BOOM_LIFE - booms[i].t) * 2;
    arduboy.fillCircle(booms[i].x - camX, booms[i].y - camY, r / 2, WHITE);
    arduboy.drawCircle(booms[i].x - camX, booms[i].y - camY, r, WHITE);
  }
}

void drawBullets()
{
  for (byte i = 0; i < BULLET_MAX; i++)
  {
    Bullet& b = bullets[i];
    if (b.life == 0) continue;
    int x = b.x - camX;
    int y = b.y - camY;
    if (x >= 0 && y >= HUD_H && x < 127 && y < 63) {
      byte sz = b.type == W_FLAME ? 1 + (FLAME_LIFE - b.life) / 5 : (b.type ? 3 : 2);      // a flame grows while it flies
      arduboy.fillRect(x, y, sz, sz, WHITE);
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
    arduboy.drawPixel(x, y, WHITE);
    arduboy.drawPixel(x - r, y - r, WHITE);
    arduboy.drawPixel(x + r, y - r, WHITE);
    arduboy.drawPixel(x - r, y + r, WHITE);
    arduboy.drawPixel(x + r, y + r, WHITE);
    arduboy.drawPixel(x, y - r - 1, WHITE);
    arduboy.drawPixel(x, y + r + 1, WHITE);
    arduboy.drawPixel(x - r - 1, y, WHITE);
    arduboy.drawPixel(x + r + 1, y, WHITE);
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

// colon: two dots, 1 pixel wide, 1 pixel gap after it
int drawSmallColon(int x, int y)
{
  arduboy.drawPixel(x, y + 1, WHITE);
  arduboy.drawPixel(x, y + 3, WHITE);
  return x + 2;
}

// "P2:1" label (13 pixels wide)
void drawSmall(int x, int y, byte player, byte sc)
{
  x = drawSmallGlyph(x, y, 10);
  x = drawSmallGlyph(x, y, player + 1);
  x = drawSmallColon(x, y);
  drawSmallGlyph(x, y, sc % 10);
}

// "A:3" team label (9 pixels wide)
void drawSmallTeam(int x, int y, byte team, byte sc)
{
  x = drawSmallGlyph(x, y, 11 + team);
  x = drawSmallColon(x, y);
  drawSmallGlyph(x, y, sc % 10);
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
      drawSmallTeam(x, 1, t, score[t]);
      if (teamOf(myId) == t) arduboy.drawFastHLine(x, 7, 9, WHITE);
    }
  }
  else
  {
    for (byte i = 0; i < MAX_PLAYERS; i++)
    {
      if (!(joinedMask & (1 << i))) continue;
      int x = 30 + i * 25;
      drawSmall(x, 1, i, score[i]);
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

// drawBold
// a line of text in the TEAMarg fat font (text.h), centred at the top of the screen
void drawBold(byte, const __FlashStringHelper *text)
{
  byte n = strlen_P((const char *)text);
  teamarg_fat_small_drawText((128 - (n * 6 + 2)) / 2, 1, (const char *)text, true);
}

// drawNum
// a one or two digit number in the text font
void drawNum(int x, int y, byte n)
{
  char b[4];
  byte k = 0;
  if (n >= 10) b[k++] = '0' + (n / 10) % 10;
  b[k++] = '0' + n % 10;
  b[k] = 0;
  teamarg_fat_small_drawText(x, y, b);
}

// drawWord
// a word centred on x
void drawWord(int cx, int y, const __FlashStringHelper *text)
{
  byte n = strlen_P((const char *)text);
  teamarg_fat_small_drawText(cx - (n * 6 + 2) / 2, y, (const char *)text, true);
}

// the arrows around a character: 1 : 3 : 5 pixels high, pointing outwards
void drawArrowLeft(int x)
{
  arduboy.drawPixel(x - 5, 22, WHITE);
  arduboy.drawFastVLine(x - 4, 21, 3, WHITE);
  arduboy.drawFastVLine(x - 3, 20, 5, WHITE);
}

void drawArrowRight(int x)
{
  arduboy.drawPixel(x + 20, 22, WHITE);
  arduboy.drawFastVLine(x + 19, 21, 3, WHITE);
  arduboy.drawFastVLine(x + 18, 20, 5, WHITE);
}

// drawGameCard
// one of the three game type cards: animated characters, then the frame (thick when selected).
// The characters and where they stand are exactly the mock-up; they only walk in the direction they are shown.
void drawGameCard(byte c, bool selected)
{
  int x = 8 + c * 40;
  const int y = 12;
  byte f = arduboy.frameCount >> 3;

  if (c == MODE_DUEL)
  {
    drawAmy(x + 14, y + 5, 1, f & 3, 0);             // Amy walks southwest, Cid southeast, Dee northwest
    drawAmy(x - 1, y + 12, 7, (f + 1) & 3, 3);
    drawAmy(x + 15, y + 22, 3, (f + 2) & 3, 1);
  }
  else if (c == MODE_FLAG)
  {
    drawAmy(x + 1, y + 4, 7, f & 3, 2);              // Bob walks to the flag
    sprites.drawPlusMask(x + 15, y + 20, Flag_plus_mask, f & 3);
  }
  else
  {
    // two teams of two: Bob on the left (southeast), Dee on the right (southwest)
    drawAmy(x, y + 4, 7, f & 3, 2);
    drawAmy(x, y + 21, 7, (f + 2) & 3, 2);
    drawAmy(x + 15, y + 3, 1, (f + 1) & 3, 1);
    drawAmy(x + 15, y + 20, 1, (f + 3) & 3, 1);
  }

  arduboy.drawRoundRect(x, y, 32, 40, 3, WHITE);
  if (selected)
  {
    arduboy.drawRoundRect(x + 1, y + 1, 30, 38, 2, WHITE);
    arduboy.drawRoundRect(x + 2, y + 2, 28, 36, 1, WHITE);
  }
  else arduboy.drawRoundRect(x + 2, y + 2, 28, 36, 1, WHITE);
}

// the lobby is three screens. The host moves through them with A (B goes back), the guests see the same screen:
//   0 game type, 1 amount of wins, 2 characters (everybody picks their own; in a team game the host picks the 2 looks)
void stateLobby()
{
  static byte charTimer = 0;
  static byte settingsTimer = 0;
  static byte teamSel = 0;                         // team game: the team the host is changing
  static unsigned int walkTick = 0;                // they walk on the spot: 3 walk cycles per direction, all 8 directions
  walkTick++;
  byte f = arduboy.frameCount >> 3;
  bool host = (myId == 0);
  bool left = arduboy.justPressed(LEFT_BUTTON);
  bool right = arduboy.justPressed(RIGHT_BUTTON);
  char d = right ? 1 : (left ? -1 : 0);
  bool changed = false;

  if (menuStage == 0)
  {
    // game type
    if (host && d) { gameMode = (gameMode + MODE_COUNT + d) % MODE_COUNT; changed = true; }
    for (byte c = 0; c < MODE_COUNT; c++) drawGameCard(c, c == gameMode);
    if (host) drawBold(0, F("PICK GAME TYPE"));
    else drawBold(0, F("HOST CHOOSES"));
    drawWord(24, 55, F("DUEL"));
    drawWord(64, 55, F("FLAG"));
    drawWord(104, 55, F("TEAM"));
    if (notice > 0) notice--;
  }
  else if (menuStage == 1)
  {
    // amount of wins: 3 5 7 9
    if (host && d) { winTarget = (winTarget >= 9 && d > 0) ? 3 : ((winTarget <= 3 && d < 0) ? 9 : winTarget + 2 * d); changed = true; }
    for (byte i = 0; i < 4; i++)
    {
      int x = 6 + i * 30;
      bool sel = (winTarget == 3 + 2 * i);
      arduboy.drawRoundRect(x, 16, 26, 38, 3, WHITE);
      if (sel)
      {
        arduboy.drawRoundRect(x + 1, 17, 24, 36, 2, WHITE);
        arduboy.drawRoundRect(x + 2, 18, 22, 34, 1, WHITE);
      }
      else arduboy.drawRoundRect(x + 2, 18, 22, 34, 1, WHITE);
      drawNum(x + 9, 32, 3 + 2 * i);
    }
    if (host) drawBold(0, F("HOW MANY VICTORIES?"));
    else drawBold(28, F("HOST CHOOSES"));
  }
  else
  {
    // characters
    bool team = (gameMode == MODE_TEAM);
    byte walkStep = f & 3;                          // 8 frames per step, 4 steps per cycle
    byte walkDir = ((walkTick >> 3) / 12) & 7;      // 12 steps = 3 cycles, then the next direction

    if (team)
    {
      if (host)
      {
        if (arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(DOWN_BUTTON)) teamSel ^= 1;
        if (d)
        {
          byte c = teamChar[teamSel];
          for (byte n = 0; n < MAX_PLAYERS; n++)
          {
            c = (c + MAX_PLAYERS + d) % MAX_PLAYERS;
            if (c != teamChar[teamSel ^ 1]) { teamChar[teamSel] = c; break; }
          }
          changed = true;
        }
      }
    }
    else if (d)
    {
      byte c = players[myId].ch;
      for (byte n = 0; n < MAX_PLAYERS; n++)
      {
        c = (c + MAX_PLAYERS + d) % MAX_PLAYERS;
        if (!charTaken(c)) { myChar = c; break; }
      }
      charTimer = 255;                              // tell the others right now
    }
    if (myChar != 255) players[myId].ch = myChar;
    if (++charTimer >= 30) { charTimer = 0; sendChar(); }   // and now and then, for players who just joined

    for (byte i = 0; i < MAX_PLAYERS; i++)
    {
      int x = 8 + i * 30;
      bool here = rosterMask & (1 << i);
      if (here) drawAmy(x, 14, walkDir, walkStep, fighterChar(i));
      else arduboy.drawRect(x + 4, 18, 8, 8, WHITE);
      if (!team && i == myId && here)
      {
        drawArrowLeft(x);
        drawArrowRight(x);
      }
      if (i == myId && here) arduboy.drawFastHLine(x, 40, 16, WHITE);      // that's us
      char pl[3] = { 'P', (char)('1' + i), 0 };
      teamarg_fat_small_drawText(x - 1, 32, pl);
    }
    if (team && host)
    {
      drawArrowLeft(8 + teamSel * 60);                 // the pair of players that is being changed
      drawArrowRight(8 + teamSel * 60 + 30);
    }

    if (team && !host) drawBold(28, F("HOST CHOOSES"));
    else drawBold(22, F("PICK CHARACTER"));

    // the host's choices
    {
      char b[16] = "GAME TYPE: ";
      strcpy_P(b + 11, gameMode == MODE_DUEL ? PSTR("DUEL") : gameMode == MODE_FLAG ? PSTR("FLAG") : PSTR("TEAM"));
      teamarg_fat_small_drawText(18, 47, b);
      char v[14] = "VICTORIES: ";
      v[11] = '0' + winTarget % 10;
      v[12] = 0;
      teamarg_fat_small_drawText(18, 55, v);
    }
    if (notice > 0)                                  // a team game needs 4 players: the pairs blink
    {
      notice--;
      if (notice & 4) arduboy.drawRect(0, 12, 128, 24, WHITE);
    }
  }

  if (host)
  {
    if (changed || settingsTimer == 0) sendSettings();      // so the guests see the host's choices
    if (changed) settingsTimer = 1;
    else if (++settingsTimer >= 30) settingsTimer = 0;
    if (arduboy.justPressed(B_BUTTON))
    {
      if (menuStage < 2) { menuStage++; sendSettings(); }
      else if (gameMode == MODE_TEAM && linked && rosterMask != 0x0F) notice = 90;
      else
      {
        sendGo();
        startGame();
      }
    }
    else if (arduboy.justPressed(A_BUTTON) && menuStage > 0) { menuStage--; sendSettings(); }
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
  camX = constrain(me.x - 56, 0, ARENA_W - WIDTH);
  camY = constrain(me.y - 28, 0, ARENA_H - HEIGHT);

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
  else drawWord(64, 56, F("HOST CHOOSES"));

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
