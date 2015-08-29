/**
  This is ONLY in the C++ test harness!
 */
#ifndef logit
  #define logit(msg,...)
#endif


const int midi_serial_rate = 31250;
const int midi_noteCount = 128;
const int byte_queue_length = 16;

byte byte_queue[byte_queue_length]; //We can queue this much before we have loss
int byte_queue_head = 0;
int byte_queue_tail = 0;
int byte_queue_count = 0;

byte cmd_state = 0;

void setup() {
  Serial.begin(midi_serial_rate);
}

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

void byte_enqueue(byte b) {
  //byte_queue[byte_queue_tail] = b;
  //byte_queue_tail++;
  //byte_queue_tail %= byte_queue_tail;
  Serial.write(b);
}

void loop() {
  while(Serial.available()) {
    byte_enqueue(Serial.read());
  }
}
