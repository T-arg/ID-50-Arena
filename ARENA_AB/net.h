#ifndef NET_H
#define NET_H

/*
  The Arduboy talks to the web page (index.html) over its USB serial port.

  Every message is a small frame:   0xA5, length, payload[length]

  Arduboy -> page   payload = type, data...
  page -> Arduboy   payload = sender, type, data...    (sender 0..3 = player number, 0xFF = the page itself)

  types:
    'I'  page -> Arduboy   data: yourId, rosterMask      who you are and who is connected (sent every second too)
    'P'  position          data: x(16), y(16), dir, flags   flags: bit0 walking, bit1 alive, bit2 blinking
    'F'  fire              data: x(16), y(16), dir+weapon   a shot: dir in bits 0-2, weapon in bits 3-4 (0 gun, 1 double gun, 2 rocket)
    'H'  hit               data: shooterId, healthLeft      sent by the player that got hit
    'G'  go                data: mode, target, teamSet, teamChars, seed(16), pocket   sent by the host: start a new round with these rules
    'S'  settings          data: mode, target, teamSet      the host's current choice, repeated in the lobby
    'R'  back to lobby     data:                            sent by the host after a round
    'L'  flag              data: holder, x(16), y(16)       flag game, sent by the host: who carries the flag (255 = nobody) or where it lies
    'Q'  scores            data: s0, s1, s2, s3             flag game, sent by the host after a capture
    'K'  claim             data: kind, slot                 sent to the host: 0 = I touched the flag, 1 = I am home with it, 2 = I take pickup number slot
    'U'  pickup            data: slot, type, x(16), y(16)   sent by the host: a weapon lies there (type 0 = the slot is empty)
    'W'  weapon            data: player, weapon             sent by the host: this player now carries this weapon
    'C'  character         data: character                  lobby: the character the sender picked

  16 bit numbers are little endian.
*/

#define NET_SYNC                 0xA5
#define NET_MAXLEN               12
#define NET_SYSTEM               0xFF

byte rxBuf[NET_MAXLEN];
byte rxPos = 0;
byte rxLen = 0;
byte rxState = 0;
byte txBuf[NET_MAXLEN];

void netHandle();                                // defined in game.h

// netSend
// sends the first len bytes of txBuf
void netSend(byte len)
{
  if (!linked) return;                           // nobody is listening: stay silent so a solo game never stalls
  Serial.write((byte)NET_SYNC);
  Serial.write(len);
  Serial.write(txBuf, len);
}

// netPoll
// reads everything the browser sent since the last frame
void netPoll()
{
  while (Serial.available() > 0)
  {
    byte b = Serial.read();
    if (rxState == 0)
    {
      if (b == NET_SYNC) rxState = 1;
    }
    else if (rxState == 1)
    {
      if (b >= 2 && b <= NET_MAXLEN)
      {
        rxLen = b;
        rxPos = 0;
        rxState = 2;
      }
      else rxState = 0;
    }
    else
    {
      rxBuf[rxPos++] = b;
      if (rxPos >= rxLen)
      {
        rxState = 0;
        lastRx = millis();
        linked = true;
        netHandle();
      }
    }
  }
  if (linked && (millis() - lastRx) > 3000) linked = false;    // the page sends a heartbeat every second
}

int netInt(byte i)
{
  return (int)((unsigned int)rxBuf[i] | ((unsigned int)rxBuf[i + 1] << 8));
}

void netPutInt(byte i, int v)
{
  txBuf[i] = v & 0xFF;
  txBuf[i + 1] = (v >> 8) & 0xFF;
}

#endif
