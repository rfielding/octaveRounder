const int midi_serial_rate = 31250;
const int midi_note_count = 128;
const int byte_queue_length = 16;
const int pitch_wheel_centered = 8192;
const byte cmd_last_never = 255;
const int split_point = 60;
const int oct_notes = 12;
const int ceiling7bit = 128;

byte cmd_channel = 0;
byte cmd_state = 0;
byte cmd_args[2];
byte cmd_needs = 0;
byte cmd_has = 0;
byte cmd_id = 0;
byte cmd_last = cmd_last_never;
byte rpn_lsb = 0;
byte rpn_msb = 0;
byte rpn_msb_data = 0;
 
int pitch_wheel_in = pitch_wheel_centered;
int pitch_wheel_sent = pitch_wheel_centered;
int pitch_wheel_adjust = 0;
int pitch_wheel_semis = 2;
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
  pitch_wheel_semis = 2;
  note_adjust = 0;
  rpn_lsb = 0;
  rpn_msb = 0;
  rpn_msb_data = 0;
  for(int i=0; i<midi_note_count; i++) {
    notes[i].id = 0;
    notes[i].sent_note = 0;
    notes[i].sent_vol = 0;
  }
  Serial.begin(midi_serial_rate);
}

static int cmd_arg_count(byte c) {
  switch(c) {
    case 0x90: //note on
    case 0x80: //note off
    case 0xA0: //aftertouch
    case 0xB0: //control parm
    case 0xE0: //bend 2 semitones default
      return 2;
    case 0xD0: //channel pressure ... like channel global aftertouch
    case 0xC0: //program change
      return 1;
    default:
      return -1;
  }
}

static void forward_xmit() {
  //TODO: if we get program change, or pitch bend sensitivity, we should have responded
  Serial.write( cmd_state | cmd_channel );
  for(int i=0; i<cmd_has; i++) {
    Serial.write( cmd_args[i] );
  }
}

static void pitch_wheel_xmit() {
  int adjusted = pitch_wheel_in + pitch_wheel_adjust;
  if( adjusted < 0 ) {
    adjusted = 0;
  }
  if( adjusted+1 >= pitch_wheel_centered*2 ) {
    adjusted = pitch_wheel_centered*2 - 1;
  }
  if( adjusted != pitch_wheel_sent ) {
    Serial.write( 0xE0 | cmd_channel );
    Serial.write( adjusted % ceiling7bit );
    Serial.write( adjusted / ceiling7bit );
    pitch_wheel_sent = adjusted;
  }
}

static void note_message_re_xmit(int n, int v) {
  Serial.write( cmd_state | cmd_channel );
  Serial.write( n );
  Serial.write( v );
}

static void note_message_xmit() {
  Serial.write( cmd_state | cmd_channel );
  Serial.write( notes[cmd_args[0]].sent_note );
  Serial.write( notes[cmd_args[0]].sent_vol );
}

static int in_quartertone_zone(byte n) {
  return n < split_point;
}

static void quartertone_adjust(byte n) {
  pitch_wheel_adjust = 0;
  if(in_quartertone_zone(n)) {
    pitch_wheel_adjust -= (pitch_wheel_centered/(2*pitch_wheel_semis));
  }
}

static byte oct_rounding() {
    int nSend = cmd_args[0]+note_adjust;
    //Might need a switch so that fullJump is always true.  
    int fullJump = (in_quartertone_zone(cmd_last) != in_quartertone_zone(cmd_args[0]));
    //TODO: can all of the while loops be dispensed with for a constant-time calculation?
    if(cmd_last != cmd_last_never) {
      int diff = (nSend - note_adjust) - cmd_last;
      if(diff > oct_notes/2 && nSend >= oct_notes) {
        do {
          nSend -= oct_notes;
          note_adjust -= oct_notes;
          diff -= oct_notes;
        }while(diff > oct_notes/2 && nSend >= oct_notes && fullJump);
      } 
      if(diff < -oct_notes/2 && nSend < (midi_note_count-oct_notes)) {
        do {
          nSend += oct_notes;
          note_adjust += oct_notes;
          diff += oct_notes;
        }while(diff < -oct_notes/2 && nSend < (midi_note_count-oct_notes) && fullJump);
      } 
    }
    while(nSend < 0) {
      nSend += oct_notes;
      note_adjust += oct_notes;
    }
    while(nSend >= midi_note_count) {
      nSend -= oct_notes;
      note_adjust -= oct_notes;
    }
    return (byte)nSend;
}

#define LEQ(a,b) ((a) <= (b))

static void find_leader(byte* ptr_lead_idx, int* ptr_lead_same) {
    byte n = cmd_args[0];
    int lead_id = 0;
    int notes_on = 0;
    for(byte i=0; i<midi_note_count; i++) {
      if( notes[i].sent_vol > 0 ) {
        notes_on++;
        if( LEQ(lead_id, notes[i].id) ) {
          lead_id  = notes[i].id;
          *ptr_lead_idx = i;
        }
      }
    }
    if(notes_on == 0) {
      for(byte i=0; i<midi_note_count; i++) {
        notes[i].id = 0;
        cmd_id = 0;
      }
    }
    *ptr_lead_same = ((*ptr_lead_idx != n) && (notes[n].sent_note == notes[*ptr_lead_idx].sent_note));
}

static void note_turnoff() {
      Serial.write( cmd_state | cmd_channel );
      Serial.write( notes[cmd_args[0]].sent_note );
      Serial.write( 0 );
}

static void note_message() {
  const byte n = cmd_args[0];
  const byte v = cmd_args[1];
  byte lead_idx = n;
  int lead_same = 0;
  if(v > 0) {
    const byte nSend = oct_rounding();
    find_leader(&lead_idx, &lead_same);
    cmd_id++;
    notes[n].id = cmd_id; //overwrites existing if it's still on
    notes[n].sent_note = nSend; //!overwritten so that lead_same is different when recomputed now!
    notes[n].sent_vol = cmd_args[1];
    if(lead_same) {
      note_turnoff(); //Done so that total on and off for a note always end up as 0
    } 
    if(n==lead_idx || notes[n].sent_note != notes[lead_idx].sent_note) {
      quartertone_adjust(n);
      pitch_wheel_xmit();
    }
    note_message_xmit();
    cmd_last = cmd_args[0];
  } else {
    int old_vol = notes[n].sent_vol;
    notes[n].sent_vol = 0;
    notes[n].id = 0;
    note_turnoff();
    //Find the leader, and set the pitch wheel back to his setting
    find_leader(&lead_idx, &lead_same);
    quartertone_adjust(lead_idx);
    pitch_wheel_xmit();
    //If we just unburied the same note, then turn it back on (midi mono won't but should)
    if(lead_same) {
      note_message_re_xmit(notes[n].sent_note, old_vol); 
    }
  }
}

//As relevant control info passes through, sync with it if we have to
static void handle_controls() {
  if( cmd_args[0] == 64 ) {
    rpn_lsb = cmd_args[1];
  }
  if( cmd_args[0] == 65 ) {
    rpn_msb = cmd_args[1];
  }  
  if( cmd_args[0] == 6 ) {
    rpn_msb_data = cmd_args[1];
    if( rpn_msb == 0 ) {
      //TODO: we assume that the pitch wheel is centered when we get this message
      //pitch_wheel_semis = rpn_msb_data;
    }
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
      pitch_wheel_in = cmd_args[0] + ceiling7bit*cmd_args[1];
      pitch_wheel_xmit();
      break;
    case 0xB0:
      handle_controls();
      forward_xmit();
      break;  
    default:
      forward_xmit();
      break;  
  }
}

static void byte_enqueue(byte b) {
  //Pass 0xF0 messages literally and immediately.
  if((b & 0xF0) == 0xF0) {
    cmd_state = 0xF0;
    Serial.write(b);
    if(b == 0xFF) {
      setup(); //Let's reset if we see a MIDI reset going by.
    }
    return;
  }
  if((b & 0x80)==0 && cmd_state == 0xF0) {
    Serial.write(b);
    return;
  }

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

//No calls to available or read should happen elsewhere
void loop() {
  while(Serial.available()) {
    byte_enqueue(Serial.read());
  }
}
