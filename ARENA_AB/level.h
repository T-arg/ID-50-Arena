#ifndef LEVEL_H
#define LEVEL_H

// the arena: # = wall, . = floor. 32 x 16 tiles of 8 x 8 pixels. Mirrored both ways so nobody has an advantage.
const char PROGMEM arenaMap[TILES_X * TILES_Y + 1] =
  "################################"
  "#..............................#"
  "#..............................#"
  "#.....##................##.....#"
  "#.........###......###.........#"
  "#.........#..........#.........#"
  "#..............................#"
  "#......##..............##......#"
  "#......##..............##......#"
  "#..............................#"
  "#.........#..........#.........#"
  "#.........###......###.........#"
  "#.....##................##.....#"
  "#..............................#"
  "#..............................#"
  "################################";

// where each player (re)spawns: the four corners
const byte PROGMEM spawnX[MAX_PLAYERS] = {   8, 232,   8, 232 };
const byte PROGMEM spawnY[MAX_PLAYERS] = {   8,   8, 104, 104 };

const unsigned char PROGMEM wallTile[] =
{
  8, 8,
  0xFF, 0x81, 0xBD, 0xA5, 0xA5, 0xBD, 0x81, 0xFF,
};

// tileSolid
// is this tile a wall? everything outside the arena counts as wall
bool tileSolid(int tx, int ty)
{
  if (tx < 0 || ty < 0 || tx >= TILES_X || ty >= TILES_Y) return true;
  return pgm_read_byte(&arenaMap[ty * TILES_X + tx]) == '#';
}

// pixelSolid
// is the arena pixel (x, y) inside a wall?
bool pixelSolid(int x, int y)
{
  if (x < 0 || y < 0) return true;
  return tileSolid(x >> 3, y >> 3);
}

// boxBlocked
// the feet of the 16x16 sprite at (x, y) are an 8 x 7 box, true when one corner is in a wall
bool boxBlocked(int x, int y)
{
  return pixelSolid(x + 4, y + 8) || pixelSolid(x + 11, y + 8) ||
         pixelSolid(x + 4, y + 14) || pixelSolid(x + 11, y + 14);
}

// drawLevel
// draws the part of the arena that is on screen, camY can be negative: the score bar sits on top of the arena
void drawLevel(int camX, int camY)
{
  int tx0 = camX >> 3;
  int ty0 = camY >> 3;
  for (int ty = ty0; ty <= ty0 + 8; ty++)
  {
    for (int tx = tx0; tx <= tx0 + 16; tx++)
    {
      if (tx < 0 || ty < 0 || tx >= TILES_X || ty >= TILES_Y) continue;
      int x = tx * TILE - camX;
      int y = ty * TILE - camY;
      if (tileSolid(tx, ty)) sprites.drawOverwrite(x, y, wallTile, 0);
      else if (((tx + ty) & 1) == 0) arduboy.drawPixel(x + 3, y + 3, WHITE);
    }
  }
}

#endif
