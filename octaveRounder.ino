const int midi_serial_rate = 31250;
const int midi_note_count = 128;
const int byte_queue_length = 16;
const int pitch_wheel_centered = 8192;
const byte cmd_last_never = 255;
const int split_point = 60;
const int oct_notes = 12;
const int ceiling7bit = 128;
const byte no_idx = 255;
const int max_fingers = 12;

byte running_status = 0;
byte cmd_channel = 0;
byte cmd_state = 0;
byte cmd_args[2];
byte cmd_needs = 0;
byte cmd_has = 0;
byte cmd_id = 0;
byte cmd_last = cmd_last_never;
byte rpn_lsb = 0x7F;
byte rpn_msb = 0x7F;
byte rpn_msb_data = 0;
byte rpn_lsb_data = 0;

int pitch_wheel_in = pitch_wheel_centered;
int pitch_wheel_sent = pitch_wheel_centered;
int pitch_wheel_adjust = 0;
int pitch_wheel_semis = 2;
int note_adjust = 0;
int note_state_count = 0;
int clock_tick = 0;

struct note_state {
  byte idx;
  byte id;
  byte sent_note;
  byte sent_vol;
} _notes[max_fingers];

static int findNoteStateIdx(int idx) {
  for(int i=0; i<note_state_count; i++) {
    if(idx == _notes[i].idx) {
      return i;
    }
  }
  return no_idx;
}

static int createNoteStateIdx(int idx) {
   _notes[note_state_count].idx = idx;
   _notes[note_state_count].id = 0;
   _notes[note_state_count].sent_vol = 0;
   _notes[note_state_count].sent_note = 0;
   note_state_count++;
   return note_state_count - 1;  
}

static int findCreateNoteStateIdx(int idx) {
  int i = findNoteStateIdx(idx);
  if(i == no_idx) {
    return createNoteStateIdx(idx);
  }
  return i;
}

static void freeNoteStateIdx(int idx) {
  if( note_state_count > 0 ) {
    //Overwrite the hole with the last entry and shrink the list by 1
    int i = findNoteStateIdx(idx);
    int j = note_state_count - 1;
    _notes[i].idx = _notes[j].idx;
    _notes[i].id = _notes[j].id;
    _notes[i].sent_note = _notes[j].sent_note;
    _notes[i].sent_vol = _notes[j].sent_vol;
    note_state_count--;
  }
}

#define setNoteStateItem(idx,val,member) \
  int i = findNoteStateIdx(idx); \
  if(i == no_idx) { \
    if(val == 0) { \
      return; \
    } \
    i = createNoteStateIdx(idx); \
  } \
  _notes[i].member = val;

#define getNoteStateItem(idx, member) \
  int i = findNoteStateIdx(idx); \
  if(i == no_idx) { \
    return 0; \
  } \
  return _notes[i].member;

    
static void setNoteStateId(int idx, byte id) {
  setNoteStateItem(idx, id, id);
}

static byte getNoteStateId(int idx) {
  getNoteStateItem(idx, id);
}

static byte getNoteStateSent(int idx) {
  getNoteStateItem(idx, sent_note);
}

static void setNoteStateSent(int idx, byte n) {
  setNoteStateItem(idx, n, sent_note);
}

static byte getNoteStateVol(int idx) {
  getNoteStateItem(idx, sent_vol);
}

static void setNoteStateVol(int idx, byte vol) {
  setNoteStateItem(idx, vol, sent_vol);
}

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
  rpn_lsb = 0x7F;
  rpn_msb = 0x7F;
  rpn_msb_data = 0;
  rpn_lsb_data = 0;
  note_state_count = 0;
  pinMode(13, OUTPUT);
  Serial.begin(midi_serial_rate);
}

static int cmd_arg_count(const byte c) {
  switch (c) {
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

static void status_xmit(byte cmd, byte channel) {
  if (cmd == 0xB0 && running_status == cmd) {
    //Do running status on these messages, because it's very common to expect it.
  } else {
    //Technically, everything could be running status, but there seems to be a lot of devices that don't handle it correctly.
    Serial.write( cmd | channel );
  }
  running_status = cmd;
}

static void forward_xmit() {
  status_xmit(cmd_state, cmd_channel);
  for (int i = 0; i < cmd_has; i++) {
    Serial.write( cmd_args[i] );
  }
}

static void pitch_wheel_xmit() {
  int adjusted = pitch_wheel_in + pitch_wheel_adjust;
  if ( adjusted < 0 ) {
    adjusted = 0;
  }
  if ( adjusted + 1 >= pitch_wheel_centered * 2 ) {
    adjusted = pitch_wheel_centered * 2 - 1;
  }
  if ( adjusted != pitch_wheel_sent ) {
    status_xmit(0xE0, cmd_channel);
    Serial.write( adjusted % ceiling7bit );
    Serial.write( adjusted / ceiling7bit );
    pitch_wheel_sent = adjusted;
  }
}

static void note_message_re_xmit(const int n, const int v) {
  status_xmit(cmd_state, cmd_channel);
  Serial.write( n );
  Serial.write( v );
}

static void note_message_xmit() {
  status_xmit(cmd_state, cmd_channel);
  Serial.write( getNoteStateSent(cmd_args[0]) );
  Serial.write( getNoteStateVol(cmd_args[0]) );
}

static int in_quartertone_zone(const byte n) {
  return n < split_point;
}

static void quartertone_adjust(const byte n) {
  pitch_wheel_adjust = 0;
  if (in_quartertone_zone(n)) {
    pitch_wheel_adjust -= (pitch_wheel_centered / (2 * pitch_wheel_semis));
  }
}

static byte oct_rounding() {
  int nSend = cmd_args[0] + note_adjust;
  //Might need a switch so that fullJump is always true.
  const int fullJump = (in_quartertone_zone(cmd_last) != in_quartertone_zone(cmd_args[0]));
  //TODO: can all of the while loops be dispensed with for a constant-time calculation?
  if (cmd_last != cmd_last_never) {
    int diff = (nSend - note_adjust) - cmd_last;
    if (diff > oct_notes / 2 && nSend >= oct_notes) {
      do {
        nSend -= oct_notes;
        note_adjust -= oct_notes;
        diff -= oct_notes;
      } while (diff > oct_notes / 2 && nSend >= oct_notes && fullJump);
    }
    if (diff < -oct_notes / 2 && nSend < (midi_note_count - oct_notes)) {
      do {
        nSend += oct_notes;
        note_adjust += oct_notes;
        diff += oct_notes;
      } while (diff < -oct_notes / 2 && nSend < (midi_note_count - oct_notes) && fullJump);
    }
  }
  while (nSend < 0) {
    nSend += oct_notes;
    note_adjust += oct_notes;
  }
  while (nSend >= midi_note_count) {
    nSend -= oct_notes;
    note_adjust -= oct_notes;
  }
  return (byte)nSend;
}

static void find_leader(byte* ptr_lead_idx, int* ptr_lead_same) {
  const byte n = cmd_args[0];
  int lead_id = 0;
  int notes_on = 0;
  for (byte i = 0; i < midi_note_count; i++) {
    if ( getNoteStateVol(i) > 0 ) {
      notes_on++;
      if ( lead_id <= getNoteStateId(i) ) {
        lead_id  = getNoteStateId(i);
        *ptr_lead_idx = i;
      }
    }
  }
  if (notes_on == 0) {
    for (byte i = 0; i < midi_note_count; i++) {
      setNoteStateId(i, 0);
      cmd_id = 0;
    }
  }
  *ptr_lead_same = ((*ptr_lead_idx != n) && (getNoteStateSent(n) == getNoteStateSent(*ptr_lead_idx)));
}

static void note_turnoff() {
  status_xmit(cmd_state, cmd_channel);
  Serial.write( getNoteStateSent(cmd_args[0]) );
  Serial.write( 0 );
}

static int count_duplicates() {
  int count = 0;
  const byte n = cmd_args[0];
  const byte nSend = getNoteStateSent(n);
  for (byte i = 0; i < midi_note_count; i++) {
    if (getNoteStateSent(i) == nSend && getNoteStateVol(i) > 0) {
      count++;
    }
  }
  return count;
}

static void note_message() {
  const byte n = cmd_args[0];
  const byte v = cmd_args[1];
  byte lead_idx = n;
  int lead_same = 0;
  if (v > 0) {
    const byte nSend = oct_rounding();
    find_leader(&lead_idx, &lead_same);
    cmd_id++;
    setNoteStateId(n, cmd_id);
    setNoteStateSent(n, nSend); //!overwritten so that lead_same is different when recomputed now!
    setNoteStateVol(n, cmd_args[1]);
    const int count = count_duplicates();
    if (lead_same || count > 1) {
      note_turnoff(); //Done so that total on and off for a note always end up as 0
    }
    if (n == lead_idx || getNoteStateSent(n) != getNoteStateSent(lead_idx)) {
      quartertone_adjust(n);
      pitch_wheel_xmit();
    }
    note_message_xmit();
    cmd_last = cmd_args[0];
  } else {
    const int old_vol = getNoteStateVol(n);
    setNoteStateVol(n, 0);
    setNoteStateId(n, 0);
    note_turnoff();
    //Find the leader, and set the pitch wheel back to his setting
    find_leader(&lead_idx, &lead_same);
    quartertone_adjust(lead_idx);
    pitch_wheel_xmit();
    //If we just unburied the same note, then turn it back on (midi mono won't but should)
    if (lead_same) {
      note_message_re_xmit(getNoteStateSent(n), old_vol);
    }
    freeNoteStateIdx(n);
  }
}

//As relevant control info passes through, sync with it if we have to
static void handle_controls() {
  if ( cmd_args[0] == 0x64 ) {
    rpn_lsb = cmd_args[1];
  }
  if ( cmd_args[0] == 0x65 ) {
    rpn_msb = cmd_args[1];
  }
  if ( cmd_args[0] == 0x06 ) {
    rpn_msb_data = cmd_args[1];
    if ( rpn_msb == 0 ) {
      //TODO: we assume that the pitch wheel is centered when we get this message
      //pitch_wheel_semis = rpn_msb_data;
    }
  }
  if ( cmd_args[0] == 0x26 ) {
    rpn_lsb_data = cmd_args[1];
    if ( rpn_lsb == 0 ) {
      //TODO: not even going to bother with cents until semis work
      //pitch_wheel_cents = rpn_lsb_data;
    }
  }
}

static void handle_message() {
  switch (cmd_state) {
    case 0x80:
      cmd_state = 0x90;
      cmd_args[1] = 0x00;
    case 0x90:
      note_message();
      break;
    case 0xE0:
      pitch_wheel_in = cmd_args[0] + ceiling7bit * cmd_args[1];
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
  if ((b & 0xF0) == 0xF0) {
    cmd_state = 0xF0;
    Serial.write(b);
    if(b == 0xFA) {
      clock_tick = 0;
    }
    if(b == 0xF8) {
      if((clock_tick % 24) == 0) {
        digitalWrite(13,HIGH);
      }
      if((clock_tick % 24) == 12) {
        digitalWrite(13,LOW);
      }
      clock_tick++;
    }
    if (b == 0xFF) {
      setup(); //Let's reset if we see a MIDI reset going by.
    }
    return;
  }
  if ((b & 0x80) == 0 && cmd_state == 0xF0) {
    Serial.write(b);
    return;
  }

  if (b & 0x80) {
    cmd_state   = (b & 0xF0);
    cmd_channel = (b & 0x0F);
    cmd_needs = cmd_arg_count(cmd_state);
    cmd_has = 0;
  } else {
    if ( cmd_needs > 0 ) {
      cmd_args[ cmd_has ] = b;
      cmd_has++;
    }
    if ( cmd_has == cmd_needs ) {
      handle_message();
      cmd_needs = cmd_arg_count(cmd_state);
      cmd_has = 0;
    }
  }
}


//No calls to available or read should happen elsewhere
void loop() {
  while (Serial.available()) {
    byte_enqueue(Serial.read());
  }
}
