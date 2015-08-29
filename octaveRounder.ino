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

/**
  This is ONLY in the C++ test harness!
 */
#ifndef logit
  #define logit(msg,...)
#endif

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
byte last_noteUp;
int octave_shift;
//count how many times this note is on for the client.  most synths don't handle note overlap correctly.
int count_noteDown[midi_noteCount];
//index is the *sent* midi note.  the value is the (last!) volume for this note's note down
byte vol_noteDown[midi_noteCount];
//index is the *physical* midi note.  the value is the *sent* midi note.
byte shifted_noteDown[midi_noteCount];

byte quarter_noteDown[midi_noteCount];

int lastWheel;


const int quartertoneSplit = 60;
const int blinky = 12;
const int toggle = 10;

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
  last_noteUp = never_noteDown;
  octave_shift = 0;
  lastWheel = 8192;
  
  pinMode(blinky, OUTPUT);
  digitalWrite(blinky, HIGH);

  pinMode(toggle, INPUT);

  midi_state = midi_needStatus;
  for (int i = 0; i < midi_noteCount; i++) {
    shifted_noteDown[i] = i;
    vol_noteDown[i] = 0;
    count_noteDown[i] = 0;
    quarter_noteDown[i] = 0; //Per note wheel adjustment
  }
}

void setup() {
  resetMidi();
}

static inline int isNoteOn() {
  return cmd_byte == 0x90 && arg1_byte != 0;
}

static inline int isNoteOff() {
  return (cmd_byte == 0x90 && arg1_byte == 0) || (cmd_byte == 0x80);
}

static inline void doFilterOnOff() {
  int diff = 0;
  int shiftedNote = 0;

  //arg1_byte == 0 for 0x90 is similar to 0x80
  if (isNoteOn()) {

    //assume that it is currently in range (it should be)
    shiftedNote = ((int)arg2_byte)  + octave_shift * 12;

    //If there was a previous note down, then compare and possibly shift....
    if ( digitalRead(toggle) == LOW ) {
      if (last_noteDown != never_noteDown) {
        diff = ((int)arg2_byte) - (int)last_noteDown;
        while (diff > 6 && (shiftedNote - 12) >= 0) {
          octave_shift--;
          shiftedNote -= 12;
          diff -= 12;
        }
        while (diff < -6 && (shiftedNote + 12) < midi_noteCount) {
          octave_shift++;
          shiftedNote += 12;
          diff += 12;
        }
      }
    }

    //Record how the note went down, and use that to plug in the note as it comes back up.
    last_noteDown = arg2_byte; //can no longer be never_noteDown

    //We can only handle 1 channel and one note down per note!!!
    while (shiftedNote > midi_noteCount) {
      shiftedNote -= 12;
    }
    while (shiftedNote < 0) {
      shiftedNote += 12;
    }

    arg2Send_byte = ((byte)shiftedNote) & 0x7F; //Should be impossible to be out of range, but I have experienced wraparound.

    shifted_noteDown[arg2_byte] = arg2Send_byte;
    quarter_noteDown[arg2_byte] = last_noteDown < quartertoneSplit; //Remember if this was a quartertone note

    //Remember enough to handle same note overlaps
    count_noteDown[arg2Send_byte]++; //Remember that we have an outstanding note that must be turned off.
    vol_noteDown[arg2Send_byte] = arg1Send_byte; //Remember the last volume for this *sent* note. (not the physical key)

  }

  if (isNoteOff()) {
    arg2Send_byte = shifted_noteDown[arg2_byte];
    count_noteDown[arg2Send_byte]--; //Should never go below zero.  Is zero when all of this note is off.
    last_noteUp = arg2_byte;
  }
}

static inline int isDuplicatingNote() {
  return count_noteDown[arg2Send_byte] > 1;
}

static inline int isNoteGone() {
  return count_noteDown[arg2Send_byte] == 0;
}

static inline void wheelAdjust(int isQuarterTone)
{
  int adjustment = 8192;
  if(isQuarterTone)
  {
    adjustment -= 8192/4;
  }
  Serial.write(0xE0 | chan_byte);
  Serial.write(adjustment % 128);
  Serial.write(adjustment / 128);
}

static inline void do2ArgSend() {
  if (isNoteOn() && isDuplicatingNote()) {
    byte arg1Send_bytePre = 0x00;
    //temporarily turn the note off (just before we turn it on again!)
    Serial.write(statusSend_byte);
    Serial.write(arg2Send_byte);
    Serial.write(arg1Send_bytePre);
  }

  //If we are bending, then remember what we last bent to.
  int isBendingNote = cmd_byte==0xE0;
  if(isBendingNote)
  {
    //Remember the bend from incoming pitch wheel
    lastWheel = arg2Send_byte + arg1Send_byte*128;
  }
  
  //If we are turning notes on, then add together the bend withe the quartertone adjustment
  if(isNoteOn())
  {
    wheelAdjust(quarter_noteDown[arg2_byte]);
  }
  
  //Send the message (whatever it is)
  Serial.write(statusSend_byte);
  Serial.write(arg2Send_byte);
  Serial.write(arg1Send_byte);

  //when undoing a retriggered note, unbury the note underneath it that should be on
  if (isNoteOff()) {
    //if the count is still 1 or more after doFilterOnOff, then turn the other instance of the note back on.
    if ( ! isNoteGone() ) {
      //Something is really wrong
      if ( count_noteDown[arg2Send_byte] > 10 || count_noteDown[arg2Send_byte] < 0) {
        //panic!!!!
        resetMidi();
      } else {
        //Retrigger the note
        byte statusSend_bytePost = 0x90 | chan_byte;
        byte arg1Send_bytePost = vol_noteDown[arg2Send_byte];
        Serial.write(statusSend_bytePost);
        Serial.write(arg2Send_byte);
        Serial.write(arg1Send_bytePost);
      }
    }
  }
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
          //Serial.write(status_byte); //No idea what it is. F0 and any byte without high bit set
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
  do {
    needStatus();
    need2Args();
    need1Args();
    doSend();
  } while (Serial.available());
  //Show light when octave rounding is on
  digitalWrite(blinky, !digitalRead(toggle));
  //digitalWrite(blinky, HIGH);
}
