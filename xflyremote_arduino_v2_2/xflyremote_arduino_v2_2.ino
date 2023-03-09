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
int radio_select_state[4][4] = {
  {0,0,0,0},
  {0,0,0,0},
  {0,0,0,0},
  {0,0,0,0}
};
// transponder state array
// only one single digit selected at one time
// 0 - not active
// 1 - active
int active_state_transp[] = {0, 0, 0, 0};
// current frequencies in each position
// note second item in the responder list is currently not used
String radio_values[4][2] = {
  {"118.000","136.990"},
  {"118.100","136.990"},
  {"0200","1750"},
  {"7000","0"}
};
// id of img used for different states
const String IMG_ID_NOSEL = "4"; // state 0
const String IMG_ID_SEL = "9"; // state 0
// select by postion (from left to right)
const String IMG_ID_TRNSP_0 = "4"; // nothing selected
const String IMG_ID_TRNSP_1 = "12"; // first digit
const String IMG_ID_TRNSP_2 = "13"; // second digit
const String IMG_ID_TRNSP_3 = "14"; // third digit
const String IMG_ID_TRNSP_4 = "8"; // fourth digit

void setup() {

  Serial.begin(115200);
  nextion.begin(115200);

  // SET START PAGE AND TEXT
  //
  String pg = "page 0";
  nextion.print(pg);
  nextion.write(0xFF);
  nextion.write(0xFF);
  nextion.write(0xFF);

  String con_color = "65048";
  String con_text = "Connecting...";
  nextion.print(con_text);
  nextion.write(0xff);
  nextion.write(0xff);
  nextion.write(0xff);
  nextion.print(con_color);
  nextion.write(0xff);
  nextion.write(0xff);
  nextion.write(0xff);
  nextion.flush();
  delay(1000);

  // rotary enncoder pins
  pinMode(rotaryCLK, INPUT);
  pinMode(rotaryDT, INPUT);
  pinMode(rotarySW, INPUT);
  aLastState = digitalRead(rotaryCLK);

  // Query python for which page set to use
  Serial.print("s?");
  while(wait != 0){
    while(Serial.available()) {
      from_python += Serial.readString();
    }
    if(from_python.substring(0, 1) == "s") {
      current_page_set = from_python.substring(1, 2).toInt();
      wait = 0;
    }
    from_python = "";
    delay(500);    
  }

  // for testing ONLY
  sendradiotoxplane();

  // tell the world we are ready
  con_color = "con.pco=59387";
  con_text = "con.txt=\"Connected!\"";
  nextion.print(con_text);
  nextion.write(0xff);
  nextion.write(0xff);
  nextion.write(0xff);
  nextion.print(con_color);
  nextion.write(0xff);
  nextion.write(0xff);
  nextion.write(0xff);
  nextion.flush();
}


void loop() {

  readnextion();
  readrotary();

}

// SEND RADIO FREQ TO PYTHON
// format to send to xplane is;
// rf XXX.XXX XXX.XXX YYY.YYY YYY.YYY AAAA AAAA TTTT
// where XXX.XXX is coms 1 & 2 (118.000 for example), YYY.YYY are the navs
// AAAA are the two ADFs and TTTT is transponder value
void sendradiotoxplane() {
  int i;
  int k;
  String sendstring = "rf";

  for(i=0; i<4; i++) {

    for(k=0; k<2; k++) {

      if(i==3 && k==1) {
        // do nothing, last value has no functio for now
      } else {
        sendstring = sendstring + " " + radio_values[i][k];
      }

    }

  }
  Serial.print(sendstring);

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
  int i;
  int k;
  int idx = 0;
  String cmd = "";

  for (i=0; i<4; i++) {

    for (k=0; k<4; k++) {

      idx ++;
      // jump for boxes with dual state but for independent digits
      if(idx == 10) {idx = 11;}
      if(idx == 12) {idx = 13;}
      if(idx == 14) {idx = 15;}
      if(idx == 16) {idx = 17;}

      int sel = radio_select_state[i][k];

      if(sel == 0) {
        cmd = "sel" + String(idx) + ".picc=" + IMG_ID_NOSEL;
      } else {
        cmd = "sel" + String(idx) + ".picc=" + IMG_ID_SEL;
      }

      nextion.print(cmd);
      nextion.write(0xff);
      nextion.write(0xff);
      nextion.write(0xff);
      nextion.flush();
    }
    
  } 

} // END setradiodisplay()


// MAIN NEXTION LOOP
//
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
      delay(10);
      nextion.flush();

      // if it is a radio page we need to recall the values since we use local vars in Nextion that reset between pages
      if (req = 2) {
        setradiodisplay();
        setradiovalues();
      }
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

    // RADIO SWAP BUTTONS
    // string from nextion has format "s1" to "s3"
    if(from_nextion.substring(0,1) == "s") {

      // read radio_values array and swap values
      int idx = from_nextion.substring(1,2).toInt();
      String val1 = radio_values[idx - 1][0];
      String val2 = radio_values[idx - 1][1];
      radio_values[idx - 1][0] = val2;
      radio_values[idx - 1][1] = val1;
      setradiovalues();
    }

    // RADIO SELECT FREQ BUTTONS
    // the string format for the number boxes is "rXXpXX" where rXX is the number box and pXX is its place in the radio_select_state array
    if(from_nextion.substring(0,1) == "r") {
      int k;
      int j;
      String cmd = "";

      int array_row = from_nextion.substring(4,5).toInt();
      int array_column = from_nextion.substring(5,6).toInt();
      
      // look up current value and switch state
      int state = radio_select_state[array_row][array_column];
      if(state == 0) {
        resetradiostate();
        delay(10);
        radio_select_state[array_row][array_column] = 1;        
      } else {
        resetradiostate();
        delay(10);
        radio_select_state[array_row][array_column] = 0;
      }
      delay(10);
      setradiodisplay();
    }
  }
}

// simply resets radio_select_state array to all off
void resetradiostate() {
  int k;
  int j;

  for(k=0; k<4; k++) {
    for(j=0; j<4; j++) {
      radio_select_state[k][j] = 0;
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