#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduboy2.h>
#include "ATMconfig.h"
#include <ATMlib.h>

// constants /////////////////////////////////////////////////////////////////

#define MAX_PLAYERS              4

//define game states
#define STATE_LOBBY              0
#define STATE_PLAY               1
#define STATE_OVER               2
#define STATE_SETUP              3               // the host picks the game type and the target score
#define STATE_INTRO              4               // the ARDUBOY letters and the T-arg logo at power on

// game types the host can pick
#define MODE_DUEL                0               // everybody against everybody, frags count
#define MODE_FLAG                1               // one flag in the middle, carry it to your own corner
#define MODE_TEAM                2               // 2 teams of 2, no friendly fire, team frags count
#define MODE_COUNT               3

#define FLAG_X                   128             // where the flag starts (the middle of the arena)
#define FLAG_Y                   64
#define FLAG_RETURN              900             // frames until a dropped flag flies back to the middle

// the arena is a fixed playground of 32 x 16 tiles
#define TILE                     8
#define TILES_X                  32
#define TILES_Y                  16
#define ARENA_W                  (TILES_X * TILE)
#define ARENA_H                  (TILES_Y * TILE)

#define HUD_H                    8               // the top 8 pixel rows are the score bar

#define PLAYER_START_HEALTH      5
#define SHOT_DELAY               10              // frames between two shots
#define BULLET_MAX               12
#define BULLET_LIFE              45              // frames a bullet lives (range = life * speed)
#define INVULN_HIT               45              // frames of blinking after a hit
#define INVULN_SPAWN             90              // frames of blinking after (re)spawning
#define RESPAWN_TIME             150             // frames before a dead player comes back
#define SPARK_MAX                4
#define SPARK_LIFE               10              // frames a hit spark is shown
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
  byte ch;                 // which character: 0 Amy, 1 Dee, 2 Bob, 3 Cid
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

struct Spark
{
  int x;
  int y;
  byte t;                  // frames left, 0 = free slot
};

// variables /////////////////////////////////////////////////////////////////

Arduboy2 arduboy;
Sprites sprites;
ATMsynth ATM;

Player players[MAX_PLAYERS];
Bullet bullets[BULLET_MAX];
Spark sparks[SPARK_MAX];

byte introCounter = 0;     // frame counter of the ARDUBOY intro + T-arg logo
byte gameState = STATE_INTRO;
byte myId = 0;             // 0 = host (master), 1..3 = guests (slaves). Told to us by the browser.
byte rosterMask = 1;       // which players the browser says are connected (bit per player)
byte joinedMask = 0;       // which players are in the round that is running
byte winner = 255;         // player number, or the team (0 / 1) in a team game

byte gameMode = MODE_DUEL; // set by the host, sent to everybody
byte winTarget = 5;        // first to reach this score wins: 3, 5, 7 or 9
byte teamSet = 0;          // team game: 0 = P1+P2 against P3+P4, 1 = P1+P3 / P2+P4, 2 = P1+P4 / P2+P3
byte score[MAX_PLAYERS];   // frags (duel), captures (flag), or per team in slot 0 and 1 (team game)
byte flagHolder = 255;     // who carries the flag, 255 = it lies on the floor at flagX, flagY
int flagX = FLAG_X;
int flagY = FLAG_Y;
unsigned int flagTimer = 0; // host only: frames until a dropped flag returns
byte menuStage = 0;        // the lobby screens: 0 game type, 1 amount of wins, 2 characters. The host moves on, the guests follow.
byte teamChar[2] = { 2, 1 }; // team game: the look of each team, picked by the host
byte notice = 0;           // frames a message on the setup screen stays
byte myChar = 255;         // the character we picked in the lobby (255 = the one that goes with our seat)
byte shotDelay = 0;
byte sendTimer = 0;

bool linked = false;       // true while the browser page is talking to us
unsigned long lastRx = 0;

#endif
