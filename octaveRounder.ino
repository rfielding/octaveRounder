#include <LiquidCrystal.h>

const int midi_serial_rate = 31250;
const int midi_note_count = 128;
const int byte_queue_length = 16;
const int pitch_wheel_centered = 8192;
const byte cmd_last_never = 255;
const int split_point = (60-12);
const int oct_notes = 12;
const int ceiling7bit = 128;

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
byte lcd_last = 0;
byte lcd_oct = 0;

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

void pinSetup() {
  for (int i = 0; i < oct_notes; i++) {
    pinMode(i + 2, OUTPUT);
    digitalWrite(i + 2, LOW);
  }
}

#define LCD_RS 53
#define LCD_ENABLE 51
#define LCD_D4 49
#define LCD_D5 47
#define LCD_D6 45
#define LCD_D7 43
#define BUT_DN 41
#define BUT_UP 39
#define BUT_TOG 37

LiquidCrystal lcd(LCD_RS, LCD_ENABLE, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

void lcdSetup() {
  pinMode(LCD_RS, OUTPUT);
  pinMode(LCD_ENABLE, OUTPUT);
  pinMode(LCD_D4, OUTPUT);
  pinMode(LCD_D5, OUTPUT);
  pinMode(LCD_D6, OUTPUT);
  pinMode(LCD_D7, OUTPUT);
  pinMode(BUT_DN, INPUT);
  pinMode(BUT_UP, INPUT);
  pinMode(BUT_TOG, INPUT);
  lcd.begin(16, 2);
  lcd.print("Jins 1.2");
}

/**
  Reset all mutable state, because test harness needs to clean up
 */
void setup() {
  lcdSetup();
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
  for (int i = 0; i < midi_note_count; i++) {
    notes[i].id = 0;
    notes[i].sent_note = 0;
    notes[i].sent_vol = 0;
  }
  Serial.begin(midi_serial_rate);
  //pinSetup(); !! DO NOT INCLUDE THIS IF BOARD USES ANY DIGITAL PINS 2-13 FOR READ
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
  Serial.write( notes[cmd_args[0]].sent_note );
  Serial.write( notes[cmd_args[0]].sent_vol );
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
    if ( notes[i].sent_vol > 0 ) {
      notes_on++;
      if ( lead_id <= notes[i].id ) {
        lead_id  = notes[i].id;
        *ptr_lead_idx = i;
      }
    }
  }
  if (notes_on == 0) {
    for (byte i = 0; i < midi_note_count; i++) {
      notes[i].id = 0;
      cmd_id = 0;
    }
  }
  *ptr_lead_same = ((*ptr_lead_idx != n) && (notes[n].sent_note == notes[*ptr_lead_idx].sent_note));
}

static void note_turnoff() {
  status_xmit(cmd_state, cmd_channel);
  Serial.write( notes[cmd_args[0]].sent_note );
  Serial.write( 0 );
}

static int count_duplicates() {
  int count = 0;
  const byte n = cmd_args[0];
  const byte nSend = notes[n].sent_note;
  for (byte i = 0; i < midi_note_count; i++) {
    if (notes[i].sent_note == nSend && notes[i].sent_vol > 0) {
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
    notes[n].id = cmd_id; //overwrites existing if it's still on
    notes[n].sent_note = nSend; //!overwritten so that lead_same is different when recomputed now!
    notes[n].sent_vol = cmd_args[1];
    const int count = count_duplicates();
    if (lead_same || count > 1) {
      note_turnoff(); //Done so that total on and off for a note always end up as 0
    }
    if (n == lead_idx || notes[n].sent_note != notes[lead_idx].sent_note) {
      quartertone_adjust(n);
      pitch_wheel_xmit();
    }
    note_message_xmit();
    cmd_last = cmd_args[0];
  } else {
    const int old_vol = notes[n].sent_vol;
    notes[n].sent_vol = 0;
    notes[n].id = 0;
    note_turnoff();
    //Find the leader, and set the pitch wheel back to his setting
    find_leader(&lead_idx, &lead_same);
    quartertone_adjust(lead_idx);
    pitch_wheel_xmit();
    //If we just unburied the same note, then turn it back on (midi mono won't but should)
    if (lead_same) {
      note_message_re_xmit(notes[n].sent_note, old_vol);
    }
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

byte ledStates[oct_notes];

static void blinkys() {
  for (int i = 0; i < oct_notes; i++) {
    ledStates[i] = LOW;
  }
  for (int i = 0; i < midi_note_count; i++) {
    ledStates[i % 12] |= (notes[i].sent_vol > 0);
  }
  for (int i = 0; i < oct_notes; i++) {
    digitalWrite(i + 2, ledStates[i]);
  }
}

char* note_names[] = {
  "C ",
  "C#",
  "D ",
  "D#",
  "E ",
  "F ",
  "F#",
  "G ",
  "G#",
  "A ",
  "A#",
  "B ",
};


int lastBtnHi = 1;
int lastBtnLo = 0;

void doLcd() {
  byte oct = (cmd_last + note_adjust) / 12;
  byte note = (cmd_last + note_adjust) % 12;

  //If data needs re-rendering
  if (note != lcd_last || oct != lcd_oct) {
    lcd.setCursor(0, 1);
    lcd.print(note_names[note]);

    if (oct < 10) {
      lcd.setCursor(6,1);
      lcd.print(" ");
      lcd.setCursor(7, 1);
      lcd.print(oct);
    } else {
      lcd.setCursor(6,1);
      lcd.print(oct);
    }
    
    lcd_last = cmd_last;
    lcd_oct = oct;
  }

  //Check buttons...debounced
  int bDown = digitalRead(BUT_DN);
  int bUp = digitalRead(BUT_UP);
  int bTog = digitalRead(BUT_TOG);
  int now = millis();

  if (bUp || bDown || bTog) {
    int timeDelta = now - lastBtnHi;
    lastBtnHi = now;
    if (timeDelta > 10 && lastBtnLo < lastBtnHi) {
      if (bDown) {
        if (note_adjust + cmd_last >= 12) {
          note_adjust -= 12;
        }
      }
      if (bUp) {
        if (note_adjust + cmd_last < (127 - 12)) {
          note_adjust += 12;
        }
      }
      if (bTog) {
        note_adjust = 0;
      }
    }
  } else {
    lastBtnLo = now;
  }
}

//No calls to available or read should happen elsewhere
void loop() {
  while (Serial.available()) {
    byte_enqueue(Serial.read());
  }
  doLcd();
}
