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
*/


enum MidiState {
  midi_needStatus,
  midi_2Args,
  midi_1Args,
  midi_send
};


MidiState midi_state = midi_needStatus;
byte status_byte = 0;
byte cmd_byte = 0;
byte chan_byte = 0;
//Number args down to 1 because of state machine handling.  ie:   cmd_byte arg2_byte arg1_byte
byte arg2_byte = 0;
byte arg1_byte = 0;

byte never_noteDown = 255; //Impossible MIDI note
byte last_noteDown = never_noteDown;
int octave_shift = 0;
const int midi_noteCount = 128;
byte shifted_noteDown[midi_noteCount];

void setup() {
  Serial.begin(31250);
  
  for(int i=0; i<12; i++) {
    pinMode( i + 2, OUTPUT );
    digitalWrite( i + 2, LOW );
  }
  
  midi_state = midi_needStatus;
  for (int i = 0; i < midi_noteCount; i++) {
    shifted_noteDown[i] = i;
  }
}

void showLastNoteDown() {
  byte n = (arg2_byte % 12);
  for(byte i=0; i<12; i++) {
    digitalWrite( i + 2, n==i);
  }
}

void doFilterOnOff() {
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
    //We can only handle 1 channel and one note down per note!!!
    shifted_noteDown[arg2_byte] = (((int)arg2_byte) + octave_shift * 12);
    shifted_noteDown[arg2_byte] %= midi_noteCount; //This is the note that we translate to on note up
    last_noteDown = arg2_byte; //can no longer be never_noteDown
    arg2_byte = shifted_noteDown[arg2_byte]; //This is the byte we send (octave translated).
    showLastNoteDown();
  }
  if (cmd_byte == 0x80 || arg1_byte == 0) {
    arg2_byte = shifted_noteDown[arg2_byte];
  }
}

//When we are in send state
void doSend() {
  if (midi_state == midi_send) {
    switch (cmd_byte) {
      case 0x90:
      case 0x80:
        doFilterOnOff();
      case 0xA0:
      case 0xB0:
      case 0xC0:
      case 0xE0:
        Serial.write(status_byte);
        Serial.write(arg2_byte);
        Serial.write(arg1_byte);
        break;
      case 0xD0:
        Serial.write(status_byte);
        Serial.write(arg1_byte);
        break;
      default:
        break;
    }
    midi_state = midi_needStatus;
  }
}

//Get a status byte and set the state depending on it
void needStatus() {
  if (midi_state == midi_needStatus) {
    if (Serial.available() > 0) {
      status_byte = Serial.read();
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
          Serial.write(status_byte); //No idea what it is
          break;
      }
    }
  }
}

//Get second to last arg before send
void need2Args() {
  if (midi_state == midi_2Args) {
    if (Serial.available() > 0) {
      arg2_byte = Serial.read();
      midi_state = midi_1Args;
    }
  }
}

//Get the last arg before we must send
void need1Args() {
  if (midi_state == midi_1Args) {
    if (Serial.available() > 0) {
      arg1_byte = Serial.read();
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
