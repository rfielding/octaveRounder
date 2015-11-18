const int midi_serial_rate = 31250;
const int midi_note_count = 128;
const int ring_buffer_size = 512;
int midi_time=-1;
int spill_time=-1;
bool delay_real_time=false;
byte echo_status_byte = 0;
int echo_status_parm = 0;
int ring_buffer_head = 0;
int ring_buffer_tail = 0;
byte ring_buffer[ring_buffer_size];

/**
  Reset all mutable state, because test harness needs to clean up
 */
void setup() {
  //Ring Buffer is empty when head==tail
  ring_buffer_head = 0;
  ring_buffer_tail = 0;
  pinMode(13, OUTPUT);
  Serial.begin(midi_serial_rate);
}

static void showTime(byte b) {
    if(midi_time % 24 == 0) {
      digitalWrite(13, HIGH);
    } else {
      if(midi_time % 24 == 12) {
        digitalWrite(13, LOW);
      }
    }
}

static bool ring_buffer_empty() {
  return ring_buffer_head == ring_buffer_tail;  
}

static bool isDueWithin(int limit) {
  return (midi_time-spill_time) >= limit;
}

static void byte_buffer(byte bin) {
  //First, spill everything that is due for spilling, which (should) guarantee space to write data.
  while(!ring_buffer_empty() && isDueWithin(16)) {
    byte bout = ring_buffer[ring_buffer_tail];
    if(bout == 0xF8) {
      spill_time++;
    }
    if(bout & 0x80) {
      echo_status_byte = bout;
      echo_status_parm = 0;
    } else {
      echo_status_parm++;
    }
    if(bout & 0x80) {
      //We need to not propagate real-time messages a second time.
      //Watch for data bytes of real-time messages
      delay_real_time = ((bout & 0xF0)==0xF0);
    }
    if(!delay_real_time) {
      if( (echo_status_byte & 0xF0)==0x90 && echo_status_parm==1 ) {
        bout = (bout + 12 + 128) % 128;
      }
      if( (echo_status_byte & 0xF0)==0x90 && echo_status_parm==2 ) {
        bout = (bout * 3) / 4;
      }
      if( (echo_status_byte & 0xF0)==0x80 && echo_status_parm==1 ) {
        bout = (bout + 12 + 128) % 128;
      }
      Serial.write(bout);
    }
    ring_buffer_tail++;
    ring_buffer_tail %= ring_buffer_size;  
  }
  ring_buffer[ring_buffer_head] = bin;
  ring_buffer_head++;
  ring_buffer_head %= ring_buffer_size;
}


void byte_enqueue(byte b) {
  if(b == 0xF8) {
    showTime(b);
    midi_time++;
  }

  Serial.write(b);
  byte_buffer(b);  
}

//No calls to available or read should happen elsewhere
void loop() {
  while (Serial.available()) {
    byte_enqueue(Serial.read());
  }
}
