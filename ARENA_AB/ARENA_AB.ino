/*
  ID-50 ARENA: multiplayer shooter for 2 to 4 Arduboys, played over the internet.

  Everybody opens index.html in Chrome / Edge, plugs in the Arduboy with USB and connects.
  One player hosts and shares the code, the others join with it. See README.md.

  Amy, the walking and the shooting come from VIRUS LQP-79 (ID-40).

  MADE by TEAM a.r.g. : https://t-arg.github.io

  Game License: MIT : https://opensource.org/licenses/MIT

*/

//determine the game
#define GAME_ID                  50

#include "globals.h"
#include "bitmaps.h"
#include "level.h"
#include "net.h"
#include "game.h"

void setup()
{
  arduboy.begin();
  arduboy.setFrameRate(60);
  Serial.begin(115200);                     // USB serial: the speed is ignored, but the sketch must open it
  for (byte i = 0; i < MAX_PLAYERS; i++) spawnPlayer(i);
}

void loop()
{
  if (!(arduboy.nextFrame())) return;
  arduboy.pollButtons();
  netPoll();
  arduboy.clear();

  switch (gameState)
  {
    case STATE_LOBBY: stateLobby(); break;
    case STATE_PLAY:  statePlay();  break;
    case STATE_OVER:  stateOver();  break;
  }

  arduboy.display();
}
