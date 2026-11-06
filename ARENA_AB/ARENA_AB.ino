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
    gameState = STATE_TITLE;
  }
}

// the title screen: the arena tiles with the four fighters in the dark, a bar on top and one at the bottom
// (tile numbers of arenaMapTiles, 8 x 4 tiles)
const byte PROGMEM titleMap[32] =
{
  4, 5, 4, 5, 4, 5, 4, 5,
  5, 8, 9, 8, 9, 8, 9, 4,
  4, 7, 0, 0, 0, 0, 6, 5,
  5, 4, 7, 6, 7, 6, 5, 4
};

// B selects, A goes back. Left / right chooses INFO, PLAY or CONF.
void stateTitle()
{
  static byte sel = 1;                                 // 0 INFO, 1 PLAY, 2 CONF
  static byte page = 0;                                // 0 title, 1 info, 2 config

  if (page == 0)
  {
    for (byte j = 0; j < 4; j++)
      for (byte i = 0; i < 8; i++)
        sprites.drawOverwrite(i * 16, j * 16, arenaMapTiles, pgm_read_byte(titleMap + j * 8 + i));
    for (byte c = 0; c < 4; c++) drawAmy(32 + c * 16, 32, 0, ((arduboy.frameCount >> 3) + c) & 3, c);
    arduboy.fillRect(0, 4, 128, 9, BLACK);
    arduboy.fillRect(0, 51, 128, 9, BLACK);
    drawWord(64, 5, F("ARENA"));
    drawWord(28, 52, F("INFO"));
    drawWord(64, 52, F("PLAY"));
    drawWord(100, 52, F("CONF"));
    drawSmallFlag(7 + sel * 36, 51);                    // the little waving flag points at the choice
    if (arduboy.justPressed(LEFT_BUTTON) && sel > 0) sel--;
    if (arduboy.justPressed(RIGHT_BUTTON) && sel < 2) sel++;
    if (arduboy.justPressed(B_BUTTON))
    {
      if (sel == 1) { gameState = STATE_LOBBY; menuStage = 0; }
      else page = sel ? 2 : 1;
    }
  }
  else if (page == 1)
  {
    drawBold(0, F("HOW TO PLAY"));
    drawWord(64, 12, F("ARROWS MOVE"));
    drawWord(64, 21, F("A RUN AND SHOOT"));
    drawWord(64, 30, F("B STAND AND SHOOT"));
    drawWord(64, 39, F("GRAB GUNS ON THE MAP"));
    drawWord(64, 48, F("FLAG TAKE IT HOME"));
    drawWord(64, 57, F("A BACK"));
    if (arduboy.justPressed(A_BUTTON)) page = 0;
  }
  else
  {
    drawBold(0, F("CONFIG"));
    if (arduboy.audio.enabled()) drawWord(64, 26, F("SOUND ON"));
    else drawWord(64, 26, F("SOUND OFF"));
    drawWord(64, 44, F("B CHANGE"));
    drawWord(64, 54, F("A BACK"));
    if (arduboy.justPressed(B_BUTTON)) { arduboy.audio.toggle(); arduboy.audio.saveOnOff(); }
    if (arduboy.justPressed(A_BUTTON)) page = 0;
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
    case STATE_TITLE: stateTitle(); break;
    case STATE_LOBBY: stateLobby(); break;
    case STATE_PLAY:  statePlay();  break;
    case STATE_OVER:  stateOver();  break;
  }

  arduboy.display();
}
