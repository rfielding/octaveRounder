const int midi_serial_rate = 31250;
const int midi_note_count = 128;
const int ring_buffer_size = 512;
int midi_time=0;
int spill_time=0;

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
  return (midi_time-spill_time) > 0;
}

static void byte_buffer(byte bin) {
  //First, spill everything that is due for spilling, which (should) guarantee space to write data.
  while(!ring_buffer_empty() && isDueWithin(24)) {
    byte bout = ring_buffer[ring_buffer_head];
    if(bout == 0xF8) {
      spill_time++;
    }
    Serial.write(bout);
    ring_buffer_head++;
    ring_buffer_head %= ring_buffer_size;  
  }
  //Assume ring buffer is not full: 
  // (ring_buffer_tail+1)%ring_buffer_size != ring_buffer_head
  ring_buffer_tail++;
  ring_buffer_tail %= ring_buffer_size;
  ring_buffer[ring_buffer_head] = bin;
}


void byte_enqueue(byte b) {
  if(b == 0xF8) {
    showTime(b);
    midi_time++;
  }
  
  byte_buffer(b);  
}

//No calls to available or read should happen elsewhere
void loop() {
  while (Serial.available()) {
    byte_enqueue(Serial.read());
  }
}
