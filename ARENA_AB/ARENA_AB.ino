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
#include "text.h"
#include "level.h"
#include "net.h"
#include "game.h"
#include "targ.h"
#include "arduboy_intro.h"                  // the ARDUBOY letters land on their notes (60 fps defaults), then the small T-arg logo

#define INTRO_LOGO_FRAMES        52         // how long the T-arg logo stays

void stateIntro()
{
  if (introCounter < INTRO_FRAMES) arduboyIntro(introCounter);   // the ARDUBOY letters land on the notes ...
  else sprites.drawSelfMasked(49, 20, T_arg, 0);                 // ... then the small T-arg logo
  introCounter++;
  if (introCounter > INTRO_FRAMES + INTRO_LOGO_FRAMES || arduboy.justPressed(A_BUTTON | B_BUTTON))   // (A or B skips it)
  {
    ATM.stop();                                                  // the tune is over: switch the sound interrupt off
    gameState = STATE_LOBBY;
  }
}

void setup()
{
  arduboy.boot();                           // (no Arduboy2 logo: the ARDUBOY intro of the game takes its place)
  arduboy.audio.begin();
  arduboy.setFrameRate(60);
  Serial.begin(115200);                     // USB serial: the speed is ignored, but the sketch must open it
  for (byte i = 0; i < MAX_PLAYERS; i++) { spawnPlayer(i); players[i].ch = i; }
}

void loop()
{
  if (!(arduboy.nextFrame())) return;
  arduboy.pollButtons();
  netPoll();
  arduboy.clear();

  switch (gameState)
  {
    case STATE_INTRO: stateIntro(); break;
    case STATE_LOBBY: stateLobby(); break;
    case STATE_PLAY:  statePlay();  break;
    case STATE_OVER:  stateOver();  break;
  }

  arduboy.display();
}
