#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduboy2.h>

// constants /////////////////////////////////////////////////////////////////

#define MAX_PLAYERS              4

//define game states
#define STATE_LOBBY              0
#define STATE_PLAY               1
#define STATE_OVER               2

// the arena is a fixed playground of 32 x 16 tiles
#define TILE                     8
#define TILES_X                  32
#define TILES_Y                  16
#define ARENA_W                  (TILES_X * TILE)
#define ARENA_H                  (TILES_Y * TILE)

#define HUD_H                    8               // the top 8 pixel rows are the score bar

#define PLAYER_START_HEALTH      5
#define FRAG_LIMIT               10              // first to this many frags wins
#define SHOT_DELAY               10              // frames between two shots
#define BULLET_MAX               12
#define BULLET_LIFE              45              // frames a bullet lives (range = life * speed)
#define INVULN_HIT               45              // frames of blinking after a hit
#define INVULN_SPAWN             90              // frames of blinking after (re)spawning
#define RESPAWN_TIME             150             // frames before a dead player comes back
#define SEND_EVERY               3               // send our position every 3 frames = 20 times a second

// structures ////////////////////////////////////////////////////////////////

struct Player
{
  int x;                   // top left of the 16x16 sprite, in arena pixels
  int y;
  int tx;                  // where the network says a remote player is, x and y chase it
  int ty;
  byte dir;                // 0 = south, then clockwise through 1 SW, 2 W, 3 NW, 4 N, 5 NE, 6 E, 7 SE
  byte frame;
  byte walking;
  byte alive;
  byte health;
  byte flash;              // blinking = can't be hit
  byte frags;
  byte respawn;
};

struct Bullet
{
  int x;
  int y;
  char vx;
  char vy;
  byte owner;
  byte life;               // 0 = free slot
};

// variables /////////////////////////////////////////////////////////////////

Arduboy2 arduboy;
Sprites sprites;

Player players[MAX_PLAYERS];
Bullet bullets[BULLET_MAX];

byte gameState = STATE_LOBBY;
byte myId = 0;             // 0 = host (master), 1..3 = guests (slaves). Told to us by the browser.
byte rosterMask = 1;       // which players the browser says are connected (bit per player)
byte joinedMask = 0;       // which players are in the round that is running
byte winner = 255;
byte shotDelay = 0;
byte sendTimer = 0;

bool linked = false;       // true while the browser page is talking to us
unsigned long lastRx = 0;

#endif
