
enum MidiState {
  midi_needStatus,
  midi_gotStatus,
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

//Here to toggle light as we get on/off events.
int toggle = 1;

void setup() {
  Serial.begin(31250);
  pinMode(13,OUTPUT);
  //unnecessary
  pinMode(2,OUTPUT);
  //necessary?
  pinMode(1,OUTPUT);
  //necessary?
  pinMode(0,INPUT);
  //unnecessary
  digitalWrite(2,HIGH);
  midi_state = midi_needStatus;
  for(int i=0; i < midi_noteCount; i++) {
    shifted_noteDown[i] = i;
  }
}

void doFilterOnOff() {
  int diff = 0;
            //arg1_byte == 0 for 0x90 is similar to 0x80
            if(cmd_byte == 0x90 && arg1_byte != 0) {
              //If there was a previous note down, then compare and possibly shift....
              if(last_noteDown != never_noteDown) {
                diff = (int)arg2_byte - (int)last_noteDown;
                if(diff > 6 && ((int)arg2_byte + (octave_shift-1)*12) >= 0) {
                  octave_shift--;
                }
                if(diff < -6 && ((int)arg2_byte + (octave_shift+1)*12) < midi_noteCount) {
                  octave_shift++;
                }
              }
              //Record how the note went down, and use that to plug in the note as it comes back up.
              //We can only handle 1 channel and one note down per note!!!
              shifted_noteDown[arg2_byte] = (((int)arg2_byte) + octave_shift * 12);
              shifted_noteDown[arg2_byte] %= midi_noteCount; //This is the note that we translate to on note up
              last_noteDown = arg2_byte; //can no longer be never_noteDown
              arg2_byte = shifted_noteDown[arg2_byte]; //This is the byte we send (octave translated).
            }
            if(cmd_byte == 0x80 || arg1_byte == 0) {
              arg2_byte = shifted_noteDown[arg2_byte];
            }
}

//When we are in send state
void doSend() {
  if(midi_state == midi_send) {
        switch(cmd_byte) {
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
  if(midi_state == midi_needStatus) {
      if(Serial.available() > 0) {
        status_byte = Serial.read();
        cmd_byte = status_byte & 0xF0;
        chan_byte = status_byte & 0x0F;
        switch(cmd_byte) {
          case 0x90:
          case 0x80:
            digitalWrite(13,toggle%2);
            toggle++;
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
            break;
        }
      }
  }
}

//Get second to last arg before send
void need2Args() {
  if(midi_state == midi_2Args) {
    if(Serial.available() > 0) {
      arg2_byte = Serial.read();
      midi_state = midi_1Args;
    }
  }
}

//Get the last arg before we must send
void need1Args() {
  if(midi_state == midi_1Args) {
    if(Serial.available() > 0) {
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
