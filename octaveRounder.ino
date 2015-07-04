/*
  Schematic:

  Just stack an Arduino Uni (r3) with a Olimex MIDI shield.
  Plug in MIDI in to a keyboard, and MIDI out to a synth.
  Ensure that MIDI channel out is same as channel receiving in synth (this does not channel rewrite).

  This proxy works by:

  - was last note down lower than -6 semitones from last?
      Octave switch up, and remember what note we must come up on to turn note off
  - was last note up higher than 6 semitones from last?
      Octave switch up, and remember what note we must come up on to turn note off

  Any byte sequences unrecognized are passed through.  The only effect of this pedal should be to automatically octave switch.

  TODO:
    - add a footswitch such that:
      - foot comes up, this pedal just passes MIDI.  there is no octave switching
      - foot goes down, pedal does auto octave switching.
      
   This MISBEHAVES with more than one channel passing through!
*/


enum MidiState {
  midi_needStatus,
  midi_2Args,
  midi_1Args,
  midi_send
};

const int midi_serial_rate = 31250;
const int midi_noteCount = 128;

//
// Set ALL of these variables in reset.
//
MidiState midi_state;

//Number args down to 1 because of state machine handling.  ie:   cmd_byte arg2_byte arg1_byte
byte status_byte;
byte cmd_byte;
byte chan_byte;
byte arg2_byte;
byte arg1_byte;

byte statusSend_byte;
byte arg2Send_byte;
byte arg1Send_byte;

byte never_noteDown;
byte last_noteDown;
int octave_shift;
//count how many times this note is on for the client.  most synths don't handle note overlap correctly.
int count_noteDown[midi_noteCount];
//index is the *sent* midi note.  the value is the (last!) volume for this note's note down
byte vol_noteDown[midi_noteCount];
//index is the *physical* midi note.  the value is the *sent* midi note.
byte shifted_noteDown[midi_noteCount];



/*
  Call this to reset the state machine.  That means that all variables *must* be set here.
  This might be hooked up to a reset button at some point.
 */
void resetMidi() {
  Serial.begin(midi_serial_rate);
  midi_state = midi_needStatus;
  status_byte = 0;
  cmd_byte = 0;
  chan_byte = 0;
  arg2_byte = 0;
  arg1_byte = 0;
  statusSend_byte = 0;
  arg2Send_byte = 0;
  arg1Send_byte = 0;
  never_noteDown = 255; //Arbitrary impossible MIDI note
  last_noteDown = never_noteDown;
  octave_shift = 0;

  for (int i = 0; i < 12; i++) {
    pinMode( i + 2, OUTPUT );
    digitalWrite( i + 2, LOW );
  }

  midi_state = midi_needStatus;
  for (int i = 0; i < midi_noteCount; i++) {
    shifted_noteDown[i] = i;
    vol_noteDown[i] = 0;
    count_noteDown[i] = 0;
  }

  //todo: wipe all stuck notes?
}

void setup() {
  resetMidi();
}

static inline void showLastNoteDown() {
  byte n = (arg2_byte % 12);
  for (byte i = 0; i < 12; i++) {
    digitalWrite( (i + 4) % 12 + 2, n == i);
  }
}

static inline void doFilterOnOff() {
  int diff = 0;
  //arg1_byte == 0 for 0x90 is similar to 0x80
  if (cmd_byte == 0x90 && arg1_byte != 0) {
    
    //If there was a previous note down, then compare and possibly shift....
    if (last_noteDown != never_noteDown) {
      diff = (int)arg2_byte - (int)last_noteDown;
      if (diff > 6 && ((int)arg2_byte + (octave_shift - 1) * 12) >= 0) {
        octave_shift--;
      }
      if (diff < -6 && ((int)arg2_byte + (octave_shift + 1) * 12) < midi_noteCount) {
        octave_shift++;
      }
    }
    
    //Record how the note went down, and use that to plug in the note as it comes back up.
    last_noteDown = arg2_byte; //can no longer be never_noteDown
    
    //We can only handle 1 channel and one note down per note!!!
    arg2Send_byte = (byte)(((int)arg2_byte) + octave_shift * 12);
    
    shifted_noteDown[arg2_byte] = arg2Send_byte;

    //Remember enough to handle same note overlaps    
    count_noteDown[arg2Send_byte]++; //Remember that we have an outstanding note that must be turned off.
    vol_noteDown[arg2Send_byte] = arg1Send_byte; //Remember the last volume for this *sent* note. (not the physical key)
    
    //Do the blinky lights
    showLastNoteDown();
  }
  
  if (cmd_byte == 0x80 || arg1_byte == 0) {
    arg2Send_byte = shifted_noteDown[arg2_byte];
    count_noteDown[arg2_byte]--; //Should never go below zero
  }
}


static inline void do2ArgSend() {
  ////This is IMPOSSIBLE on a normal keyboard because there are no note overlaps, so expect that synths DO NOT handle overlaps right!
  //turn off notes before retriggering them
  /*
  if (cmd_byte == 0x90 && arg1Send_byte != 0) {
    if (count_noteDown[arg2Send_byte] > 1) {
      byte arg1Send_bytePre = 0x00;
      //temporarily turn the note off (just before we turn it on again!)
      Serial.write(statusSend_byte);
      Serial.write(arg2Send_byte);
      Serial.write(arg1Send_bytePre);
    }
  }
  */
  
  Serial.write(statusSend_byte);
  Serial.write(arg2Send_byte);
  Serial.write(arg1Send_byte);
  
  /*
  //when undoing a retriggered note, unbury the note underneath it that should be on
  if ((cmd_byte == 0x90 && arg1Send_byte == 0) || cmd_byte == 0x80) {
    //if the count is still 1 or more after doFilterOnOff, then turn the other instance of the note back on.
    if (count_noteDown[arg2Send_byte] != 0) {
      byte statusSend_bytePost = 0x90 | chan_byte;
      byte arg1Send_bytePost = vol_noteDown[arg2Send_byte];
      Serial.write(statusSend_bytePost);
      Serial.write(arg2Send_byte);
      Serial.write(arg1Send_bytePost);
    }
  }
  */
}

static inline void do1ArgSend() {
  Serial.write(statusSend_byte);
  Serial.write(arg1Send_byte);
}

//When we are in send state
static inline void doSend() {
  if (midi_state == midi_send) {
    switch (cmd_byte) {
      case 0x90:
      case 0x80:
        doFilterOnOff();
      case 0xA0:
      case 0xB0:
      case 0xC0:
      case 0xE0:
        do2ArgSend();
        break;
      case 0xD0:
        do1ArgSend();
        break;
      default:
        break;
    }
    midi_state = midi_needStatus;
  }
}

//Get a status byte and set the state depending on it
static inline void needStatus() {
  if (midi_state == midi_needStatus) {
    if (Serial.available() > 0) {
      status_byte = Serial.read();
      statusSend_byte = status_byte;
      cmd_byte = status_byte & 0xF0;
      chan_byte = status_byte & 0x0F;
      switch (cmd_byte) {
        case 0x90:
        case 0x80:
        case 0xA0:
        case 0xB0:
        case 0xC0:
        case 0xE0:
          midi_state = midi_2Args;
          break;
        case 0xD0:
          midi_state = midi_1Args;
          break;
        default:
          Serial.write(status_byte); //No idea what it is. F0 and any byte without high bit set
          break;
      }
    }
  }
}

//Get second to last arg before send
static inline void need2Args() {
  if (midi_state == midi_2Args) {
    if (Serial.available() > 0) {
      arg2_byte = Serial.read();
      arg2Send_byte = arg2_byte;
      midi_state = midi_1Args;
    }
  }
}

//Get the last arg before we must send
static inline void need1Args() {
  if (midi_state == midi_1Args) {
    if (Serial.available() > 0) {
      arg1_byte = Serial.read();
      arg1Send_byte = arg1_byte;
      midi_state = midi_send;
    }
  }
}

//Handle reading 1 byte at a time
void loop() {
  needStatus();
  need2Args();
  need1Args();
  doSend();
}
