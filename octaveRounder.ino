/**
  This is ONLY in the C++ test harness!
 */
#ifndef logit
  #define logit(msg,...)
#endif


const int midi_serial_rate = 31250;
const int midi_note_count = 128;
const int byte_queue_length = 16;
const int pitch_wheel_centered = 8192;
const byte cmd_last_never = 255;

byte cmd_channel = 0;
byte cmd_state = 0;
byte cmd_args[2];
byte cmd_needs = 0;
byte cmd_has = 0;
byte cmd_id = 0;
byte cmd_last = cmd_last_never;

int pitch_wheel_in = pitch_wheel_centered;
int pitch_wheel_sent = pitch_wheel_centered;
int pitch_wheel_adjust = 0;
int note_adjust = 0;

struct note_state {
  byte id;
  byte sent_note;
  byte sent_vol;
} notes[midi_note_count];

/**
  Reset all mutable state, because test harness needs to clean up
 */
void setup() {
  cmd_channel = 0;
  cmd_state = 0;
  cmd_args[0] = 0;
  cmd_args[1] = 0;
  cmd_needs = 0;
  cmd_has = 0;
  cmd_id = 0;
  cmd_last = cmd_last_never;
  pitch_wheel_in = pitch_wheel_centered;
  pitch_wheel_sent = pitch_wheel_centered;
  pitch_wheel_adjust = 0;
  for(int i=0; i<midi_note_count; i++) {
    notes[i].id = 0;
    notes[i].sent_note = 0;
    notes[i].sent_vol = 0;
  }
  Serial.begin(midi_serial_rate);
}

static int cmd_arg_count(byte c) {
  switch(c) {
    case 0x90:
    case 0x80:
    case 0xA0:
    case 0xB0:
    case 0xC0:
    case 0xE0:
      return 2;
    case 0xD0:
      return 1;
    case 0xF0:
      return 0;
  }
}

static void forward_xmit() {
  Serial.write( cmd_state | cmd_channel );
  for(int i=0; i<cmd_has; i++) {
    Serial.write( cmd_args[i] );
  }
}

static void pitch_wheel_xmit() {
  int adjusted = pitch_wheel_in + pitch_wheel_adjust;
  if( adjusted != pitch_wheel_sent ) {
    Serial.write( 0xE0 | cmd_channel );
    Serial.write( adjusted % 128 );
    Serial.write( adjusted / 128 );
    pitch_wheel_sent = adjusted;
  }
}

static void note_message_xmit() {
  Serial.write( cmd_state | cmd_channel );
  Serial.write( notes[cmd_args[0]].sent_note );
  Serial.write( notes[cmd_args[0]].sent_vol );
}

static void quartertone_adjust(byte n) {
  pitch_wheel_adjust = 0;
  if(n < 60) {
    pitch_wheel_adjust -= 2048;
  }
}

static void note_message() {
  byte n = cmd_args[0];
  byte v = cmd_args[1];
  if(v > 0) {
    byte nSend = cmd_args[0]+note_adjust;
    if(cmd_last != cmd_last_never) {
      int diff = n - cmd_last;
      if(diff > 6) {
        nSend -= 12;
        note_adjust -= 12;
      } 
      if(diff < -6) {
        nSend += 12;
        note_adjust += 12;
      } 
    }
    cmd_id++;
    notes[n].id = cmd_id;
    notes[n].sent_note = nSend;
    notes[n].sent_vol = cmd_args[1];
    int copies = 0;
    byte n2 = notes[n].sent_note;
    for(int i=0; i<midi_note_count; i++) {
      if( notes[i].sent_note == n2 ) {
        copies++;
      }
    }
    if(copies > 1) {
      Serial.write( cmd_state | cmd_channel );
      Serial.write( n2 );
      Serial.write( 0 );
    }
    quartertone_adjust(n);
    pitch_wheel_xmit();
    note_message_xmit();
    cmd_last = cmd_args[0];
  } else {
    notes[n].sent_vol = 0;
    note_message_xmit();
    //Find the leader, and set the pitch wheel back to his setting
    byte lead_id = 0;
    byte lead_idx = n;
    for(byte i=0; i<midi_note_count; i++) {
      byte relative_id = notes[i].id - notes[n].id;
      if( notes[i].sent_vol > 0 && relative_id > lead_id ) {
        lead_id  = relative_id;
        lead_idx = i;
      }
    }
    quartertone_adjust(lead_idx);
    pitch_wheel_xmit();
  }
}

static void handle_message() {
  switch(cmd_state) {
    case 0x80:
      cmd_state = 0x90;
      cmd_args[1] = 0x00;
    case 0x90:
      note_message();
      break;
    case 0xE0:
      pitch_wheel_in = cmd_args[0] + 128*cmd_args[1];
      pitch_wheel_xmit();
      break;  
    case 0xA0:
    case 0xB0:
    case 0xC0:
    case 0xD0:
      forward_xmit();
      break;  
    case 0xF0:
      forward_xmit();
      break;  
  }
}

void byte_enqueue(byte b) {
  if(b & 0x80) {
    cmd_state   = (b & 0xF0);
    cmd_channel = (b & 0x0F);
    cmd_needs = cmd_arg_count(cmd_state); 
    cmd_has = 0;
  } else {
    if( cmd_needs > 0 ) {
      cmd_args[ cmd_has ] = b;
      cmd_has++;
    }
    if( cmd_has == cmd_needs ) {
      handle_message();
      cmd_needs = cmd_arg_count(cmd_state);
      cmd_has = 0;
    }
  }
}

void loop() {
  while(Serial.available()) {
    byte_enqueue(Serial.read());
  }
}
