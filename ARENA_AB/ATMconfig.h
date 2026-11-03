#ifndef ATMCONFIG_H
#define ATMCONFIG_H

// Only the ARDUBOY intro tune (one sound effect on channel 1: notes, volume, volume slide) is played. Everything else is left out.

// effects that are NOT used
#define ATM_FX_FREQ_SLIDE       0
#define ATM_FX_ARP              0
#define ATM_FX_NOISE            0
#define ATM_FX_TRANSPOSE        0
#define ATM_FX_TREMOLO          0
#define ATM_FX_VIBRATO          0
#define ATM_FX_GLISSANDO        0
#define ATM_FX_NOTE_CUT         0
#define ATM_FX_CUE              0
#define ATM_FX_TEMPO            0
#define ATM_FX_GOTO_ADV         0

// functions: ATM.playSfx() for the tune and ATM.stop() to switch the sound interrupt off afterwards (the game needs the processor)
#define ATM_FUNC_PLAYPAUSE      0
#define ATM_FUNC_PAUSE          0
#define ATM_FUNC_RESUME         0
#define ATM_FUNC_MUTE           0
#define ATM_FUNC_UNMUTE         0
#define ATM_FUNC_TEMPO          0
#define ATM_FUNC_CUE            0

// the tune only uses channel 1 and octave 2: the other channels are left out of the sound interrupt
#define ATM_WAVE_CH0            ATM_WAVE_OFF
#define ATM_WAVE_CH2            ATM_WAVE_OFF
#define ATM_WAVE_CH3            ATM_WAVE_OFF

// no GOTO / REPEAT: a call stack of 1 level per channel is enough
#define ATM_STACK_DEPTH         1
#define ATM_OCTAVE_3            0
#define ATM_OCTAVE_4            0
#define ATM_OCTAVE_5            0
#define ATM_OCTAVE_6            0
#define ATM_OCTAVE_7            0

#endif
