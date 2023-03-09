// xflyremote_arduino
// Controller logic for the Arduino/Nextion combo
// License: GPLv3
// https://github.com/mydogspies/xflyremote-hardware
// The python main interface to xplane can be found at;
// https://github.com/mydogspies/xflyremote-main

#include <SoftwareSerial.h>
SoftwareSerial nextion (2, 3);

// SERIAL COMS
//
String from_nextion = "";
String from_python = "";
int wait = 1;
int updated = 0;

// PAGE LOGIC
//
const int page_sets[2][5] = {
  {3, 2, 1, 5, 4},
  {0, 0, 0, 0, 0}}
  ;
int page_button_state[5][10] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
  };
int current_page_set = 0;

// ROTZARY ENCODER LOGIC
//
#define rotaryCLK 6
#define rotaryDT 7
#define rotarySW 5
int counter = 0;
int aState;
int aLastState;
int rotary_switch_state = 1;

// RADIO PAGE LOGIC
//
int select_state[4][2] = {
  {0,0},
  {0,0},
  {0,0},
  {0,0}
};
// radio state array as follows;
// only one select at any time
// 0 - no select
// 1 - select left
// 2 - select right
int active_state_radios[3][2] = {
  {0,0},
  {0,0},
  {0,0}
};
// transponder state array
// only one single digit selected at one time
// 0 - not active
// 1 - active
int active_state_transp[] = {0, 0, 0, 0};
// current frequencies in each position
// note second item in the responder list is currently not used
String radio_values[4][2] = {
  {"121.050","136.990"},
  {"118.100","136.005"},
  {"0200","1750"},
  {"1234","0"}
};
// id of img used for different states
const String IMG_ID_NOSEL = "4"; // state 0
const String IMG_ID_SELLEFT = "12"; // state 1
const String IMG_ID_SELRIGHT = "14"; // state 2
// select by postion (from left to right)
const String IMG_ID_TRNSP_0 = "4"; // nothing selected
const String IMG_ID_TRNSP_1 = "12"; // first digit
const String IMG_ID_TRNSP_2 = "13"; // second digit
const String IMG_ID_TRNSP_3 = "14"; // third digit
const String IMG_ID_TRNSP_4 = "8"; // fourth digit


void setup() {

  Serial.begin(115200);
  nextion.begin(38400);

  // for testing only
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // rotary enncoder pins
  pinMode(rotaryCLK, INPUT);
  pinMode(rotaryDT, INPUT);
  pinMode(rotarySW, INPUT);
  aLastState = digitalRead(rotaryCLK);

  // Query python for which page set to use
  Serial.println("s?");
  while(wait != 0){
    while(Serial.available()) {
      from_python += Serial.readString();
    }
    if(from_python.substring(0, 1) == "s") {
      current_page_set = from_python.substring(1, 2).toInt();
      wait = 0;
    }
    from_python = "";
    Serial.flush();
    delay(100);
  }
}


void loop() {

  readnextion();
  readrotary();

}

// send from radio_values[][] to display
void setradiovalues() {

  String freq;
  String first;
  String second;
  String adf;
  String trnsp;
  String cmd = "";

  int v = 1; // start address of the first text box
  for (int i=0; i<2; i++) {

    for (int j=0; j<2; j++) {

      // get com & nav values and split them into integral and fractional
      freq = radio_values[i][j];
      for (int k=0; k<3; k++) {
        first = first + freq[k];
      }
      for (int m=0; m<3; m++) {
        second = second + freq[m+4];
      }

      cmd = "r" + String(v) + ".val=" + first;
      nextion.print(cmd);
      nextion.write(0xff);
      nextion.write(0xff);
      nextion.write(0xff);
      nextion.flush();
      cmd = "r" + String(v+1) + ".val=" + second;
      nextion.print(cmd);
      nextion.write(0xff);
      nextion.write(0xff);
      nextion.write(0xff);
      nextion.flush();
      v = v+2;
      first = "";
      second = "";
      cmd = "";
    }
  }

  // send adf valuies to nextion
  v = 9; // start address for text box on the adf
  for (int n=0; n<2; n++) {

    adf = radio_values[2][n];
    
    for (int p=0; p<4; p++) {
      char digit = adf[p];
      cmd = "r" + String(v) + ".val=" + digit;
      nextion.print(cmd);
      nextion.write(0xff);
      nextion.write(0xff);
      nextion.write(0xff);
      nextion.flush();
      v ++;
    }
    adf = "";
    cmd = "";
  }

  v = 17; // start address of the text box for trnsp
  // int trnsp = int(radio_values[3][0]);
  trnsp = radio_values[3][0];
    
  for (int q=0; q<4; q++) {
    char digit = trnsp[q];
    cmd = "r" + String(v) + ".val=" + digit;
    nextion.print(cmd);
    nextion.write(0xff);
    nextion.write(0xff);
    nextion.write(0xff);
    nextion.flush();
    v ++;
    }
    adf = "";
    cmd = "";  
}


// SET RADIO PAGE
// sets the radio active and selection graphics depending on current states array active_state_radios[][]
void setradiodisplay() {

  String cmd = "";
  String cmdt = "";
  int k;
  int i;
  int j;
  int img_num = 1;

  // SET PAGE - DEFAULT SET
  //
  if (current_page_set == 0) {

    // RADIOS
    // Com 1&2, Nav 1&2 and ADF 1&2
    for (i = 0; i<3; ++i) {

      for (k = 0; k<2; k++) {
        
        switch (active_state_radios[i][k]) {

          case 0:
            cmd = "im" + String(img_num) + ".picc=" + IMG_ID_NOSEL;
            break;
            
          case 1:
            cmd = "im" + String(img_num) + ".picc=" + IMG_ID_SELLEFT;
            break;

          case 2:
            cmd = "im" + String(img_num) + ".picc=" + IMG_ID_SELRIGHT;
            break;

          default:
            break;
        }
        
        nextion.print(cmd);
        nextion.write(0xff);
        nextion.write(0xff);
        nextion.write(0xff);
        nextion.flush();
        img_num ++;
        
      }
    }

    // TRANSPONDER
    // only 1 digit can be selected at anytime
    for (j=0; j<4; j++) {

      cmdt = "im7.picc=" + IMG_ID_TRNSP_0;

      if (active_state_transp[j] == 1) {

        switch (j) {

          case 0:
            cmdt = "im7.picc=" + IMG_ID_TRNSP_1;
            break;
            
          case 1:
            cmdt = "im7.picc=" + IMG_ID_TRNSP_2;
            break;

          case 2:
            cmdt = "im7.picc=" + IMG_ID_TRNSP_3;
            break;

          case 3:
            cmdt = "im7.picc=" + IMG_ID_TRNSP_4;
            break;

          default:
            break;
        }
        
      }
      nextion.print(cmdt);
      nextion.write(0xff);
      nextion.write(0xff);
      nextion.write(0xff);
      nextion.flush();

    }
  } // END set default page
    
} // END setradiodisplay()


void readnextion() {

  int page = 0;
  int button_id =0;
  String cmd = "";
  wait = 1;

  // FROM NEXTION
  if (nextion.available() > 0) {
    from_nextion = "";

    while(nextion.available()) {
      from_nextion += char(nextion.read());
      delay(50);
    }

    // MENU COMMANDS
    // incoming menu commands are as follows; m1 - ap, m2 - radio, m3 - lights, m4 - sys, m5 -utility
    if(from_nextion.substring(0, 1) == "m") {
      int req = from_nextion.substring(1, 2).toInt();
      page = page_sets[current_page_set][req-1];
      String pg = "page " + String(page);
      nextion.print(pg);
      nextion.write(0xFF);
      nextion.write(0xFF);
      nextion.write(0xFF);
      if (req = 2) {
        setradiodisplay();
        setradiovalues();
      }
      nextion.flush();
    }

    // BUTTON COMMANDS
    // string from nextion has format;
    // for buttons - b(page)(button_id) - example: b0100 (button, contains 01 and 00 (page 1, button_id 0) )
    if(from_nextion.substring(0, 1) == "b") {
      page = from_nextion.substring(1, 3).toInt();
      button_id = from_nextion.substring(3, 5).toInt();      
      if (page_button_state[page][button_id] == 0) {
        page_button_state[page][button_id] = 1;
      } else {
        page_button_state[page][button_id] = 0;
      }
      delay(50);    
      String button_cmd = from_nextion + String(page_button_state[page][button_id]);
      Serial.println(button_cmd);
      Serial.flush();
    }
  }
}

void readrotary() {

  // SWITCH LOGIC
  int sw = digitalRead(rotarySW);
  if (sw == 0) {

    if (rotary_switch_state == 1) {
      rotary_switch_state = 0;
    } else {
      rotary_switch_state = 1;
    }
    Serial.println(rotary_switch_state);
    delay(400);
  }

  // ROTARY LOGIC
  aState = digitalRead(rotaryCLK);
  if (aState != aLastState) {
    if (digitalRead(rotaryDT) != aState) {
      counter ++;
    } else {
      counter --;
    }
    Serial.print("Counter: ");
    Serial.println(counter);
  }
  aLastState = aState;
} // END readrotary()