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

// drawMasked
// draws a 16x16 image with its mask (from VIRUS LQP-79)
void drawMasked(int x, int y, const unsigned char *mask, byte maskFrame, const unsigned char *image, byte frame)
{
  sprites.drawErase(x, y, mask, maskFrame);
  sprites.drawSelfMasked(x, y, image, frame);
}

// drawFlipped
// draws a 16x16 image and its mask mirrored left to right (from VIRUS LQP-79). Sprites can't mirror, so this
// erases with the mirrored mask and ORs the mirrored image straight into the screen buffer.
void drawFlipped(int x, int y, const unsigned char *img, const unsigned char *mask, byte k)
{
  byte *buf = arduboy.getBuffer();
  byte shift = y & 7;
  for (byte c = 0; c <= k; c++)
  {
    int sx = x + c;
    if (sx < 0 || sx > 127) continue;
    for (byte p = 0; p < 2; p++)
    {
      byte i = (p << 4) + k - c;
      unsigned int im = (unsigned int)pgm_read_byte(img + i) << shift;
      unsigned int mk = (unsigned int)pgm_read_byte(mask + i) << shift;
      for (byte h = 0; h < 2; h++)
      {
        int page = (y >> 3) + p + h;
        if (page < 0 || page > 7) continue;
        byte *s = buf + page * 128 + sx;
        *s = (*s & ~(byte)(mk >> (h << 3))) | (byte)(im >> (h << 3));
      }
    }
  }
}

// drawAmy
// the four fighters: 0 Amy, 1 Dee, 2 Bob, 3 Cid (same feet, other upper body). Every frame has its own mask.
// The two pointer tables live in RAM (16 bytes); the sprites they point to are in PROGMEM.
const unsigned char *const charImg[MAX_PLAYERS]  = { playerAmy, playerDee, playerBob, playerCid };
const unsigned char *const charMask[MAX_PLAYERS] = { playerAmyMask, playerDeeMask, playerBobMask, playerCidMask };

void drawAmy(int x, int y, byte dir, byte frame, byte who)
{
  const unsigned char *img = charImg[who];
  const unsigned char *msk = charMask[who];
  if (dir <= 4) drawMasked(x, y, msk, frame + 4 * dir, img, frame + 4 * dir);
  else drawFlipped(x, y, img + 2 + (frame + 4 * (8 - dir)) * 32, msk + 2 + (frame + 4 * (8 - dir)) * 32, 15);
}

// game logic ////////////////////////////////////////////////////////////////

void spawnPlayer(byte id)
{
  Player& p = players[id];
  p.x = p.tx = pgm_read_byte(&spawnX[id]);
  p.y = p.ty = pgm_read_byte(&spawnY[id]);
  p.dir = (id & 1) ? 2 : 6;                       // face the middle of the arena
  p.frame = 0;
  p.walking = 0;
  p.alive = 1;
  p.health = PLAYER_START_HEALTH;
  p.flash = INVULN_SPAWN;
  p.respawn = 0;
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

void addBullet(int x, int y, byte dir, byte owner)
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
      b.life = BULLET_LIFE;
      return;
    }
  }
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
  for (byte i = 0; i < MAX_PLAYERS; i++) score[i] = 0;
  flagHolder = 255;
  flagX = FLAG_X;
  flagY = FLAG_Y;
  flagTimer = 0;
  joinedMask = rosterMask;
  winner = 255;
  shotDelay = 0;
  sendTimer = 0;
  for (byte i = 0; i < BULLET_MAX; i++) bullets[i].life = 0;
  for (byte i = 0; i < SPARK_MAX; i++) sparks[i].t = 0;
  for (byte i = 0; i < MAX_PLAYERS; i++)
  {
    players[i].frags = 0;
    spawnPlayer(i);
  }
  gameState = STATE_PLAY;
}

void sendFlag();                                 // defined below

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

void sendFire(int x, int y, byte dir)
{
  txBuf[0] = 'F';
  netPutInt(1, x);
  netPutInt(3, y);
  txBuf[5] = dir;
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
  netSend(4);
}

void sendGo()
{
  txBuf[0] = 'G';
  txBuf[1] = gameMode;
  txBuf[2] = winTarget;
  txBuf[3] = teamSet;
  netSend(4);
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

void sendClaim(byte kind)
{
  txBuf[0] = 'K';
  txBuf[1] = kind;
  netSend(2);
}

// the flag game, host side: only the host decides who holds the flag, so everybody agrees
bool nearFlag(Player& p, int r)
{
  return abs(p.x + 8 - flagX) < r && abs(p.y + 10 - flagY) < r;
}

bool nearHome(byte id, byte r)
{
  Player& p = players[id];
  return abs(p.x - (int)pgm_read_byte(&spawnX[id])) < r && abs(p.y - (int)pgm_read_byte(&spawnY[id])) < r;
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
  flagX = FLAG_X;
  flagY = FLAG_Y;
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
        flagHolder = 255;                        // the carrier left: the flag goes back to the middle
        flagX = FLAG_X;
        flagY = FLAG_Y;
        sendFlag();
      }
    }
    return;
  }

  if (sender >= MAX_PLAYERS || sender == myId) return;

  if (type == 'G' || type == 'S')
  {
    if (sender != 0) return;                     // only the host decides
    if (rxLen >= 5 && rxBuf[2] < MODE_COUNT && rxBuf[4] < 3)
    {
      gameMode = rxBuf[2];
      winTarget = constrain(rxBuf[3], 1, 9);
      teamSet = rxBuf[4];
    }
    if (type == 'G') startGame();
    return;
  }

  if (type == 'R')
  {
    if (sender == 0) gameState = STATE_LOBBY;
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
    addBullet(netInt(2), netInt(4), rxBuf[6] & 7, sender);
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
  else if (type == 'K' && myId == 0 && gameMode == MODE_FLAG)
  {
    hostClaim(sender, rxBuf[2]);
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
    addBullet(bx, by, me.dir, myId);
    sendFire(bx, by, me.dir);
    shotDelay = SHOT_DELAY;
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

// updateBullets
// moves the bullets and looks for hits.
//  - Bullets from others against us: we decide it is a hit (we know best where we are), lower our health and tell everybody.
//  - Our own bullets against the others: only a prediction on our screen. The bullet stops and a spark shows it, but the
//    damage only counts when the other player confirms with a hit message.
void updateBullets()
{
  Player& me = players[myId];

  for (byte i = 0; i < BULLET_MAX; i++)
  {
    Bullet& b = bullets[i];
    if (b.life == 0) continue;

    b.life--;
    b.x += b.vx;
    b.y += b.vy;

    if (pixelSolid(b.x, b.y) || pixelSolid(b.x + 1, b.y + 1))
    {
      b.life = 0;
      continue;
    }

    if (b.owner != myId)
    {
      if (me.alive && me.flash == 0 && !sameTeam(b.owner, myId) && bulletHits(b, me))
      {
        b.life = 0;
        byte left = me.health - 1;
        applyHit(myId, b.owner, left);
        if (left > 0) me.flash = INVULN_HIT;
        sendHit(b.owner, left);
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
          addSpark(b.x, b.y);
          break;
        }
      }
    }
  }

  for (byte i = 0; i < SPARK_MAX; i++)
    if (sparks[i].t > 0) sparks[i].t--;
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
      flagX = FLAG_X;                              // nobody picked it up: back to the middle
      flagY = FLAG_Y;
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

void drawFlagAt(int x, int y)
{
  arduboy.fillRect(x, y, 8, 8, BLACK);
  arduboy.drawBitmap(x, y, flagBmp, 8, 8, WHITE);
}

// drawMarker
// a blinking dot at the edge of the screen that points to something far away
void drawMarker(int wx, int wy)
{
  int x = wx - camX;
  int y = wy - camY;
  if (x >= 0 && x < WIDTH && y >= HUD_H && y < HEIGHT) return;
  if (arduboy.frameCount & 8) return;
  arduboy.fillRect(constrain(x, 1, WIDTH - 4), constrain(y, HUD_H + 1, HEIGHT - 4), 3, 3, WHITE);
}

void drawFlag()
{
  if (gameMode != MODE_FLAG) return;
  if (flagHolder == 255)
  {
    drawFlagAt(flagX - 3 - camX, flagY - 7 - camY);
    drawMarker(flagX, flagY);
  }
  else if (flagHolder == myId)
  {
    drawMarker(pgm_read_byte(&spawnX[myId]) + 8, pgm_read_byte(&spawnY[myId]) + 8);   // carry it home
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
    if (x >= 0 && y >= HUD_H && x < 127 && y < 63) arduboy.fillRect(x, y, 2, 2, WHITE);
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
    if (!(p.flash && (arduboy.frameCount & 4))) drawAmy(x, y, p.dir, p.frame, p.ch);

    // hearts left: a little bar under her feet, 2 pixels per heart
    arduboy.fillRect(x + 3, y + 16, p.health * 2, 2, WHITE);

    // every fighter wears the player number (ours is inverted); in a team game the team letter goes in front
    arduboy.setCursor(gameMode == MODE_TEAM ? x + 2 : x + 5, y - 7);
    if (gameMode == MODE_FLAG && flagHolder == i) drawFlagAt(x + 11, y - 8);
    if (i == myId)
    {
      arduboy.setTextColor(BLACK);
      arduboy.setTextBackground(WHITE);
    }
    if (gameMode == MODE_TEAM) arduboy.print((char)('A' + teamOf(i)));
    arduboy.print(i + 1);
    arduboy.setTextColor(WHITE);
    arduboy.setTextBackground(BLACK);
  }
}

void drawHud()
{
  Player& me = players[myId];
  arduboy.fillRect(0, 0, WIDTH, HUD_H, BLACK);
  for (byte i = 0; i < me.health; i++) arduboy.fillRect(i * 5, 1, 4, 5, WHITE);

  if (gameMode == MODE_TEAM)
  {
    // team scores: A:3  B:2, our own team underlined
    for (byte t = 0; t < 2; t++)
    {
      int x = 40 + t * 40;
      arduboy.setCursor(x, 0);
      arduboy.print((char)('A' + t));
      arduboy.print(':');
      arduboy.print(score[t]);
      if (teamOf(myId) == t) arduboy.drawFastHLine(x, 7, 24, WHITE);
    }
  }
  else
  {
    for (byte i = 0; i < MAX_PLAYERS; i++)
    {
      if (!(joinedMask & (1 << i))) continue;
      int x = 28 + i * 25;
      arduboy.setCursor(x, 0);
      arduboy.print(i + 1);
      arduboy.print(':');
      arduboy.print(score[i]);
      if (i == myId) arduboy.drawFastHLine(x, 7, 24, WHITE);
    }
  }
}

void printMode(byte m)
{
  if (m == MODE_DUEL) arduboy.print(F("DUEL"));
  else if (m == MODE_FLAG) arduboy.print(F("FLAG"));
  else arduboy.print(F("TEAM"));
}

// is this character already taken by another player in the lobby?
bool charTaken(byte c)
{
  for (byte i = 0; i < MAX_PLAYERS; i++)
    if (i != myId && (rosterMask & (1 << i)) && players[i].ch == c) return true;
  return false;
}

void stateLobby()
{
  static byte charTimer = 0;
  static byte settingsTimer = 0;
  static unsigned int walkTick = 0;                // they walk on the spot: 3 walk cycles per direction, all 8 directions
  walkTick++;
  byte walkStep = (walkTick >> 3) & 3;               // 8 frames per step, 4 steps per cycle
  byte walkDir = ((walkTick >> 3) / 12) & 7;        // 12 steps = 3 cycles, then the next direction

  arduboy.setCursor(34, 0);
  arduboy.print(F("ID-50 ARENA"));

  // LEFT / RIGHT picks the next free character
  byte step = 0;
  if (arduboy.justPressed(RIGHT_BUTTON)) step = 1;
  if (arduboy.justPressed(LEFT_BUTTON)) step = MAX_PLAYERS - 1;
  if (step)
  {
    byte c = players[myId].ch;
    for (byte n = 0; n < MAX_PLAYERS; n++)
    {
      c = (c + step) % MAX_PLAYERS;
      if (!charTaken(c)) { myChar = c; break; }
    }
    charTimer = 255;                              // tell the others right now
  }
  if (myChar != 255) players[myId].ch = myChar;
  if (++charTimer >= 30) { charTimer = 0; sendChar(); }   // and now and then, for players who just joined

  for (byte i = 0; i < MAX_PLAYERS; i++)
  {
    int x = 8 + i * 30;
    if (rosterMask & (1 << i)) drawAmy(x, 14, walkDir, walkStep, players[i].ch);
    else arduboy.drawRect(x + 4, 18, 8, 8, WHITE);
    if (i == myId && (rosterMask & (1 << i)))
    {
      // arrows that point outwards, 1 : 3 : 5 pixels high
      arduboy.drawPixel(x - 5, 22, WHITE);
      arduboy.drawFastVLine(x - 4, 21, 3, WHITE);
      arduboy.drawFastVLine(x - 3, 20, 5, WHITE);
      arduboy.drawPixel(x + 20, 22, WHITE);
      arduboy.drawFastVLine(x + 19, 21, 3, WHITE);
      arduboy.drawFastVLine(x + 18, 20, 5, WHITE);
    }
    arduboy.setCursor(x + 2, 32);
    arduboy.print('P');
    arduboy.print(i + 1);
    if (i == myId && (rosterMask & (1 << i)))
    {
      arduboy.setCursor(x - 1, 41);
      arduboy.print(F("YOU"));
    }
  }

  arduboy.setCursor(9, 48);
  arduboy.print(F("<> PICK  "));
  printMode(gameMode);
  arduboy.print(F(" TO "));
  arduboy.print(winTarget);

  arduboy.setCursor(0, 56);
  if (!linked) arduboy.print(F("SOLO TEST. A: SETUP"));
  else if (myId == 0) arduboy.print(F("A: GAME SETUP"));
  else arduboy.print(F("WAITING FOR HOST..."));

  // the host (or a solo test) picks the game and starts the round
  if (myId == 0)
  {
    if (settingsTimer == 0) sendSettings();                 // so the guests see the host's choice
    if (++settingsTimer >= 30) settingsTimer = 0;
    if (arduboy.justPressed(A_BUTTON))
    {
      setupRow = 0;
      notice = 0;
      gameState = STATE_SETUP;
    }
  }
}

// stateSetup
// the host picks the game type and the target score: UP / DOWN select a line, LEFT / RIGHT change it, A starts
void stateSetup()
{
  static byte tick = 0;
  byte rows = (gameMode == MODE_TEAM) ? 3 : 2;
  if (setupRow >= rows) setupRow = rows - 1;
  bool changed = false;

  if (arduboy.justPressed(DOWN_BUTTON)) setupRow = (setupRow + 1) % rows;
  if (arduboy.justPressed(UP_BUTTON)) setupRow = (setupRow + rows - 1) % rows;

  char d = 0;
  if (arduboy.justPressed(RIGHT_BUTTON)) d = 1;
  if (arduboy.justPressed(LEFT_BUTTON)) d = -1;
  if (d)
  {
    changed = true;
    if (setupRow == 0) gameMode = (gameMode + MODE_COUNT + d) % MODE_COUNT;
    else if (setupRow == 1) winTarget = (winTarget == 9 && d > 0) ? 3 : ((winTarget == 3 && d < 0) ? 9 : winTarget + 2 * d);
    else teamSet = (teamSet + 3 + d) % 3;
  }
  if (changed || ++tick >= 30) { tick = 0; sendSettings(); }

  arduboy.setCursor(34, 0);
  arduboy.print(F("GAME SETUP"));

  for (byte r = 0; r < rows; r++)
  {
    byte y = 14 + r * 11;
    arduboy.setCursor(6, y);
    arduboy.print(r == 0 ? F("GAME") : (r == 1 ? F("FIRST TO") : F("TEAMS")));
    arduboy.setCursor(72, y);
    bool sel = (r == setupRow);
    if (sel) arduboy.print('<');
    else arduboy.print(' ');
    if (r == 0) printMode(gameMode);
    else if (r == 1) { arduboy.print(' '); arduboy.print(winTarget); arduboy.print(' '); }
    else arduboy.print(teamSet == 0 ? F("1+2") : (teamSet == 1 ? F("1+3") : F("1+4")));
    if (sel) arduboy.print('>');
    if (sel) arduboy.print(' ');
  }
  arduboy.setCursor(0, 14 + setupRow * 11);
  arduboy.print('>');

  arduboy.setCursor(0, 47);
  if (gameMode == MODE_DUEL) arduboy.print(F("SHOOT EACH OTHER"));
  else if (gameMode == MODE_FLAG) arduboy.print(F("CARRY THE FLAG HOME"));
  else arduboy.print(F("2 V 2. NO FRIENDLY FIRE"));

  arduboy.setCursor(0, 57);
  if (notice > 0) { notice--; arduboy.print(F("TEAM GAME NEEDS 4 PLAYERS")); }
  else arduboy.print(F("A: START    B: BACK"));

  if (arduboy.justPressed(B_BUTTON)) gameState = STATE_LOBBY;
  else if (arduboy.justPressed(A_BUTTON))
  {
    if (gameMode == MODE_TEAM && linked && rosterMask != 0x0F) notice = 90;
    else
    {
      sendGo();
      startGame();
    }
  }
}

void statePlay()
{
  updateMe();
  updateRemotes();
  updateBullets();
  updateFlag();

  if (gameState != STATE_PLAY) return;             // that last hit ended the round

  if (++sendTimer >= SEND_EVERY)
  {
    sendTimer = 0;
    sendPos();
  }

  // camera: Amy in the middle of the play field (below the score bar), clamped to the arena
  Player& me = players[myId];
  camX = constrain(me.x - 56, 0, ARENA_W - WIDTH);
  camY = constrain(me.y - 28, -HUD_H, ARENA_H - HEIGHT);

  drawLevel(camX, camY);
  drawFlag();
  drawBullets();
  drawPlayers();
  drawSparks();
  drawHud();

  if (!me.alive)
  {
    arduboy.fillRect(34, 26, 60, 11, BLACK);
    arduboy.setCursor(37, 28);
    arduboy.print(F("RESPAWN "));
    arduboy.print(1 + me.respawn / 60);
  }
}

void stateOver()
{
  arduboy.setCursor(28, 0);
  if (gameMode == MODE_TEAM)
  {
    arduboy.print(F("TEAM "));
    arduboy.print((char)('A' + winner));
  }
  else
  {
    arduboy.print(F("PLAYER "));
    arduboy.print(winner + 1);
  }
  arduboy.print(F(" WINS!"));

  byte y = 12;
  if (gameMode == MODE_TEAM)
  {
    arduboy.setCursor(34, y);
    arduboy.print(F("A "));
    arduboy.print(score[0]);
    arduboy.print(F("  -  B "));
    arduboy.print(score[1]);
    y += 10;
  }

  for (byte i = 0; i < MAX_PLAYERS; i++)
  {
    if (!(joinedMask & (1 << i))) continue;
    arduboy.setCursor(24, y);
    arduboy.print('P');
    arduboy.print(i + 1);
    if (gameMode == MODE_TEAM)
    {
      arduboy.print(' ');
      arduboy.print((char)('A' + teamOf(i)));
    }
    arduboy.print(gameMode == MODE_FLAG ? F("  FLAGS ") : F("  FRAGS "));
    arduboy.print(gameMode == MODE_FLAG ? score[i] : players[i].frags);
    if (i == myId) arduboy.print(F(" <"));
    y += 9;
  }

  arduboy.setCursor(0, 57);
  if (myId == 0) arduboy.print(F("A: AGAIN   B: MENU"));
  else arduboy.print(F("WAITING FOR HOST..."));

  if (myId == 0)
  {
    if (arduboy.justPressed(A_BUTTON))
    {
      sendGo();
      startGame();
    }
    else if (arduboy.justPressed(B_BUTTON))
    {
      sendBack();
      gameState = STATE_LOBBY;
    }
  }
}

#endif
