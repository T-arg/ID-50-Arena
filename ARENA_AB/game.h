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
// every player is Amy
void drawAmy(int x, int y, byte dir, byte frame)
{
  if (dir <= 4) drawMasked(x, y, playerAmyMask, dir, playerAmy, frame + 4 * dir);
  else drawFlipped(x, y, playerAmy + 2 + (frame + 4 * (8 - dir)) * 32, playerAmyMask + 2 + (8 - dir) * 32, 15);
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

void startGame()
{
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
    if (shooter < MAX_PLAYERS && shooter != victim)
    {
      players[shooter].frags++;
      if (players[shooter].frags >= FRAG_LIMIT)
      {
        winner = shooter;
        gameState = STATE_OVER;
      }
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
  txBuf[6] = (me.walking ? 1 : 0) | (me.alive ? 2 : 0) | (me.flash ? 4 : 0);
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

void sendHit(byte shooter, byte health)
{
  txBuf[0] = 'H';
  txBuf[1] = shooter;
  txBuf[2] = health;
  netSend(3);
}

void sendGo()
{
  txBuf[0] = 'G';
  netSend(1);
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
    }
    return;
  }

  if (sender >= MAX_PLAYERS || sender == myId) return;

  if (type == 'G')
  {
    if (sender == 0) startGame();                // only the host starts a round
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
  }
  else if (type == 'F')
  {
    addBullet(netInt(2), netInt(4), rxBuf[6] & 7, sender);
  }
  else if (type == 'H')
  {
    applyHit(sender, rxBuf[2], rxBuf[3]);
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
      if (me.alive && me.flash == 0 && bulletHits(b, me))
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
        if (k == myId || !(joinedMask & (1 << k))) continue;
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
    if (!(p.flash && (arduboy.frameCount & 4))) drawAmy(x, y, p.dir, p.frame);

    // hearts left: a little bar under her feet, 2 pixels per heart
    arduboy.fillRect(x + 3, y + 16, p.health * 2, 2, WHITE);

    // everyone is Amy, so every Amy wears her player number (ours is inverted)
    arduboy.setCursor(x + 5, y - 7);
    if (i == myId)
    {
      arduboy.setTextColor(BLACK);
      arduboy.setTextBackground(WHITE);
    }
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

  for (byte i = 0; i < MAX_PLAYERS; i++)
  {
    if (!(joinedMask & (1 << i))) continue;
    int x = 28 + i * 25;
    arduboy.setCursor(x, 0);
    arduboy.print(i + 1);
    arduboy.print(':');
    arduboy.print(players[i].frags);
    if (i == myId) arduboy.drawFastHLine(x, 7, 24, WHITE);
  }
}

void stateLobby()
{
  arduboy.setCursor(34, 0);
  arduboy.print(F("ID-50 ARENA"));

  for (byte i = 0; i < MAX_PLAYERS; i++)
  {
    int x = 8 + i * 30;
    if (rosterMask & (1 << i)) drawAmy(x, 14, 0, 0);
    else arduboy.drawRect(x + 4, 18, 8, 8, WHITE);
    arduboy.setCursor(x + 2, 32);
    arduboy.print('P');
    arduboy.print(i + 1);
    if (i == myId && (rosterMask & (1 << i)))
    {
      arduboy.setCursor(x - 1, 41);
      arduboy.print(F("YOU"));
    }
  }

  arduboy.setCursor(0, 56);
  if (!linked) arduboy.print(F("NO BROWSER: SOLO TEST"));
  else if (myId == 0) arduboy.print(F("A: START THE GAME"));
  else arduboy.print(F("WAITING FOR HOST..."));

  // the host (or a solo test) starts the round
  if (myId == 0 && arduboy.justPressed(A_BUTTON))
  {
    sendGo();
    startGame();
  }
}

void statePlay()
{
  updateMe();
  updateRemotes();
  updateBullets();

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
  arduboy.setCursor(30, 0);
  arduboy.print(F("PLAYER "));
  arduboy.print(winner + 1);
  arduboy.print(F(" WINS!"));

  byte row = 0;
  for (byte i = 0; i < MAX_PLAYERS; i++)
  {
    if (!(joinedMask & (1 << i))) continue;
    arduboy.setCursor(24, 16 + row * 10);
    arduboy.print('P');
    arduboy.print(i + 1);
    arduboy.print(F("  FRAGS "));
    arduboy.print(players[i].frags);
    if (i == myId) arduboy.print(F(" <"));
    row++;
  }

  arduboy.setCursor(0, 57);
  if (myId == 0) arduboy.print(F("A: PLAY AGAIN"));
  else arduboy.print(F("WAITING FOR HOST..."));

  if (myId == 0 && arduboy.justPressed(A_BUTTON))
  {
    sendGo();
    startGame();
  }
}

#endif
