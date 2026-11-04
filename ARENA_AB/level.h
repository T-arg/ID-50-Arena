#ifndef LEVEL_H
#define LEVEL_H

#include "arenaMap.h"

// where each player (re)spawns: the four corners
const int PROGMEM spawnX[MAX_PLAYERS] = {  64, 944,  64, 944 };
const int PROGMEM spawnY[MAX_PLAYERS] = {  52,  52, 960, 960 };

// the pockets: the tree parts 8-11 of the tile map editor, each with its opening on another side
// (8 opens south, 9 east, 10 west, 11 north). Every round 3 to 6 of them are placed at random.
struct Pocket { byte px, py, type; };       // part position (1..6) and 0..3 = part 8..11
Pocket pockets[6];
byte pocketCount = 0;
const signed char PROGMEM faceDx[4] = {  0, 1, -1,  0 };
const signed char PROGMEM faceDy[4] = {  1, 0,  0, -1 };
const byte PROGMEM flagOffX[4] = { 56, 72, 40, 72 };   // where the flag lies inside the pocket (pixels from the part's corner)
const byte PROGMEM flagOffY[4] = { 72, 56, 72, 40 };

unsigned int rng;
byte rnd()
{
  rng ^= rng << 7;
  rng ^= rng >> 9;
  rng ^= rng << 8;
  return rng >> 8;
}

// buildPockets
// places the pockets for mapSeed: at least 3 parts apart (so no part can be walled in), and no opening that looks at another pocket
// (a pocket's front, side or back) or at the border. The same seed gives the same pockets everywhere.
void buildPockets()
{
  rng = mapSeed ^ 0xA5C3;
  if (rng == 0) rng = 1;
  byte want = 3 + rnd() % 4;
  pocketCount = 0;
  for (byte tries = 0; tries < 250 && pocketCount < want; tries++)
  {
    byte px = 1 + rnd() % 6, py = 1 + rnd() % 6, t = rnd() & 3;
    int fx = px + (signed char)pgm_read_byte(&faceDx[t]);
    int fy = py + (signed char)pgm_read_byte(&faceDy[t]);
    bool ok = fx >= 1 && fx <= 6 && fy >= 1 && fy <= 6;
    for (byte i = 0; ok && i < pocketCount; i++)
    {
      Pocket& q = pockets[i];
      if (abs((int)px - q.px) + abs((int)py - q.py) < 3) ok = false;                                   // too close: no sealed-off parts
      if (q.px == fx && q.py == fy) ok = false;                                                        // we look at it
      if (q.px + (signed char)pgm_read_byte(&faceDx[q.type]) == px &&
          q.py + (signed char)pgm_read_byte(&faceDy[q.type]) == py) ok = false;                        // it looks at us
    }
    if (ok) { pockets[pocketCount].px = px; pockets[pocketCount].py = py; pockets[pocketCount].type = t; pocketCount++; }
  }
}

int flagHomeX() { Pocket& p = pockets[flagPocket]; return p.px * 128 + pgm_read_byte(&flagOffX[p.type]); }
int flagHomeY() { Pocket& p = pockets[flagPocket]; return p.py * 128 + pgm_read_byte(&flagOffY[p.type]); }

// mapPart
// the map part at part position px, py. The border parts are as drawn in the tile map editor, the pockets
// are the ones from buildPockets, every other part is picked by a small hash of the seed, so all Arduboys
// build the same random arena
byte mapPart(byte px, byte py)
{
  byte p = arenaMapGetPart(px, py);
  if (!((p >= 4 && p <= 6) || (p >= 8 && p <= 11))) return p;
  for (byte i = 0; i < pocketCount; i++)
    if (pockets[i].px == px && pockets[i].py == py) return 8 + pockets[i].type;
  unsigned int h = (mapSeed ^ ((unsigned int)px * 0x0A5B + (unsigned int)py * 0x1F27)) * 0x9E37;
  h ^= h >> 7;
  h *= 0x85EB;
  h ^= h >> 9;
  byte r = (h >> 4) & 3;
  return r == 0 ? 4 : (r == 3 ? 6 : 5);            // a few trees, an open part, a block
}

// pixelSolid
// is the arena pixel (x, y) inside a wall? everything outside the arena counts as wall
bool pixelSolid(int x, int y)
{
  if (x < 0 || y < 0 || x >= ARENA_W || y >= ARENA_H) return true;
  byte tx = x >> 4, ty = y >> 4;
  return arenaMapIsSolid(arenaMapPartTile(mapPart(tx >> 3, ty >> 3), tx & 7, ty & 7));
}

// boxBlocked
// the feet of the 16x16 sprite at (x, y) are an 8 x 7 box, true when one corner is in a wall
bool boxBlocked(int x, int y)
{
  return pixelSolid(x + 4, y + 8) || pixelSolid(x + 11, y + 8) ||
         pixelSolid(x + 4, y + 14) || pixelSolid(x + 11, y + 14);
}

// drawLevel
// draws the part of the arena that is on screen (camX, camY are never negative)
void drawLevel(int camX, int camY)
{
  byte tx0 = camX >> 4, ty0 = camY >> 4;
  byte sx = camX & 15, sy = camY & 15;
  byte cols = 8 + (sx ? 1 : 0), rows = 4 + (sy ? 1 : 0);
  for (byte j = 0; j < rows; j++)
    for (byte i = 0; i < cols; i++)
    {
      byte tx = tx0 + i, ty = ty0 + j;
      byte tile = ARENAMAP_OUTSIDE_TILE;
      if (tx < ARENAMAP_WIDTH && ty < ARENAMAP_HEIGHT)
        tile = arenaMapPartTile(mapPart(tx >> 3, ty >> 3), tx & 7, ty & 7);
      sprites.drawOverwrite(i * 16 - sx, j * 16 - sy, arenaMapTiles, tile);
    }
}

#endif
