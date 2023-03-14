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
const unsigned int MAX_MESSAGE_LENGTH = 12;
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
int page_button_state[6][10] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
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
  {"108.000","117.950"},
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
  nextion.begin(38400);

  // SET START PAGE AND TEXT
  //
  String pg = "page 0";
  sendtonextion(pg);

  // rotary enncoder pins
  pinMode(rotaryCLK, INPUT);
  pinMode(rotaryDT, INPUT);
  pinMode(rotarySW, INPUT);
  aLastState = digitalRead(rotaryCLK);

  /*
  // Query python for which page set to use
  Serial.print("s?");
  while(wait != 0){
    while(Serial.available()) {
      from_python += Serial.readString();
    }
    if(from_python.substring(0, 3) == "set") {
      current_page_set = from_python.substring(3, 4).toInt();
      wait = 0;
    }
    from_python = "";
    delay(500);    
  }
  */

  // for testing ONLY
  // sendradiotoxplane();

  // tell the world we are ready
  String con_color = "con.pco=59387";
  String con_text = "con.txt=\"Connected!\"";
  sendtonextion(con_text);
  sendtonextion(con_color);
}


void loop() {

  nextionlisten();
  // readrotary();

}

//
// MAIN NEXTION LOOP
//
// incoming message format for buttons/touchspots is -> #LCppccdddd (for example '#7b05014848' means -> string (#) with (7) bytes of data attached (button 01 on page 05 with 4848 as data attached). 
// # - means string of data follows
// L - number of bytes
// C - command (b - button, m - page)
// pp - page
// cc - number
// dddd - any data that follows
// NOTE! the 'objname' in nextion tool attribute is the Cppnn part - so for example a button would be 'b0501' (button 01 on page 05)
//
void nextionlisten() {

    if(nextion.available() > 2){

      char start_char = nextion.read();

      // COMMANDS
      // look for the flag '#' meaning this is an incoming command
      if(start_char == '#'){

        uint8_t len = nextion.read() - '0'; // turn ascii into number
        unsigned long wait1 = millis();
        boolean found = true;

        // wait for the remaining string and time out if needed
        while(nextion.available() < len){          
          if((millis() - wait1) > 100){
            found = false;
            break;                            
          }                                     
          delay(1);
        }                                   

        // get the actual command string
        String cmd = "";                            
        if(found == true){
          int i;

          for(i=0; i<len; i++) {
            char stuff = nextion.read();
            cmd += stuff;
          }  

          // CHANGE PAGE
          //
          if(cmd.substring(0,1) == "m") {
            pagetonextion(cmd.substring(1,3).toInt());
          }

          // BUTTON PRESSED
          //
          if(cmd.substring(0,1) == "b") {
            buttontopython(cmd.substring(0,len));
          }

        }

      } 
    }    
}


//
// MAIN PYTHON LOOP
//
// incoming message format for buttons/touchspots is -> *LCppccdddd (for example '#7b05014848' means -> string (#) with (7) bytes of data attached (button 01 on page 05 with 4848 as data attached). 
// * - means string of data follows
// L - number of bytes
// C - command (t - text box, s - state for buttons)
// pp - page
// cc - number
// dddd - any data that follows
// NOTE! the 'objname' in nextion tool attribute is the Cppnn part - so for example a button would be 'b0501' (button 01 on page 05)
//
void pythonlisten() {

  if(Serial.available() > 2){

    char pstart_char = Serial.read();

    // COMMANDS
    // look for the flag '#' meaning this is an incoming command
    if(pstart_char == '*'){

      uint8_t plen = Serial.read() - '0'; // turn ascii into number
      unsigned long pwait1 = millis();
      boolean pfound = true;

      // wait for the remaining string and time out if needed
      while(Serial.available() < plen){          
        if((millis() - pwait1) > 100){
          pfound = false;
          break;                            
        }                                     
        delay(1);
      }                                   

      // get the actual command string
      String pcmd = "";                            
      if(pfound == true){
        int i;

        for(i=0; i<plen; i++) {
          char pstuff = Serial.read();
          pcmd += pstuff;
        }  

        // TEST! go to page example
        if(pcmd.substring(0,1) == "c") {
          
        }      

      }
    } 
  }  
}

//
// SERIAL FUNCTIONS
//

// SEND TO NEXTION
// simply sending preformatted string to Nextion
void sendtonextion(String data) {
  nextion.print(data);
  nextion.write(0xFF);
  nextion.write(0xFF);
  nextion.write(0xFF);
}

// SEND TO PYTHON
// sending preformatted string to python
void sendtopython(String data) {
  Serial.print(data);  
}

//
// NEXTION FUNCTIONALITY METHODS
//

// FORMAT PAGE FOR NEXTION
// formats page number as a Nextion command
// and sets off to check the current state of buttons
void pagetonextion(int page) {
  String pg = "page " + String(page);
  sendtonextion(pg);
  setbuttonstate(page);
}

// SET BUTTONS ACCORDING TO STATE ARRAY
// checks for current state on given page and sets them thereafter
void setbuttonstate(int page) {
  int i;
  int state;
  String button = "b";
  String cmd;

  for(i=0; i<10; i++) {
    cmd = "page";
    state = page_button_state[page][i];
    if(state == 0) {
      cmd += String(page) + ".b0" + String(page) + "0" + String(i) + ".val=0";
     } else {
       cmd += String(page) + ".b0" + String(page) + "0" + String(i) + ".val=1";       
    }
    sendtonextion(cmd);
  }
}

//
// PYTHON FUNCTIONALITY METHODS
//

// GET BUTTON AND SEND TO PYTHON
// receives the button command and formats it with state for python
void buttontopython(String cmd) {
  String bcmd;

  // define page and button positions in array
  int page = cmd.substring(1,3).toInt();
  int button = cmd.substring(3,5).toInt();
  // strip data of last 4 digits (amend processed data later)
  bcmd = cmd.substring(0,5);

  // check state array and reverse depending on state
  // then amend last 4 bytes of data with current state added
  if(page_button_state[page][button] == 0) {
    page_button_state[page][button] = 1;
    bcmd = bcmd + "1000";
  } else {
    page_button_state[page][button] = 0;
    bcmd = bcmd + "0000";
  }
  sendtopython(bcmd);
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
      cmd = "r" + String(v+1) + ".val=" + second;
      nextion.print(cmd);
      nextion.write(0xff);
      nextion.write(0xff);
      nextion.write(0xff);
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
    }
    
  } 

} // END setradiodisplay()

//
// MAIN NEXTION LOOP
//
// message format for buttons/touchspots is -> #LCppccdddd (for example '#7b05014848' means -> string (#) with (7) bytes of data attached (button 01 on page 05 with 4848 as data attached). 
// # - means string of data follows
// L - number of bytes
// C - command (b - button, m - page
// pp - page
// cc - number
// dddd - any data that follows
// NOTE! the 'objname' in nextion tool attribute is the Cppnn part - so for example a button would be 'b0501' (button 01 on page 05)
//
void readnextion() {

  /*
  from_nextion = "";
  int page = 0;
  int button_id =0;
  String cmd = "";
  wait = 1;
  */

  // FROM NEXTION
  if (nextion.available() > 2) {

    char recv = nextion.read();

    // Serial.println(recv);

    // following code based on code from Athanasios Seitanis (CC Attrib 4.0 license)
    // https://seithan.com/projects/nextion-tutorial/
    //
    // check if it's and incomming command
    if(recv == "#") {
      
      uint8_t len = nextion.read(); // see how many bytes we expect
      unsigned long wait1 = millis();
      boolean found = true;

      Serial.println(len);

      // wait for buffer to fill out and time-out if not
      while(nextion.available() < len) {
        if((millis() - wait1) > 100) {
          found = false;
          break;
        } 
      }

      // if we got all then do stuff to it
      if(found == true) {
        
        int i;

        for(i=0; i<len; i++) {
          char stuff = nextion.read();
          Serial.print(stuff);
        }

      }
    }

    
    /*
    // MENU COMMANDS
    // incoming menu commands are as follows; m1 - ap, m2 - radio, m3 - lights, m4 - sys, m5 -utility
    if(from_nextion.substring(0, 1) == "m") {

      // check for sanity
      if(from_nextion.length() == 2) {

        int req = from_nextion.substring(1, 2).toInt();
        page = page_sets[current_page_set][req-1];
        String pg = "page " + String(page);
        nextion.print(pg);
        nextion.write(0xFF);
        nextion.write(0xFF);
        nextion.write(0xFF);

        // if it is a radio page we need to recall the values since we use local vars in Nextion that reset between pages
        if (req = 2) {
        setradiodisplay();
        setradiovalues();
        }
      }
    }
    */

    /*
    // BUTTON COMMANDS
    // string from nextion has format;
    // for buttons - b(page)(button_id) - example: b0100 (button, contains 01 and 00 (page 1, button_id 0) )
    if(from_nextion.substring(0, 1) == "b") {

      // check for sanity
      if(from_nextion.length() == 5) {

        page = from_nextion.substring(1, 3).toInt() - 1;
        button_id = from_nextion.substring(3, 5).toInt();  

        if (page_button_state[page][button_id] == 0) {
          page_button_state[page][button_id] = 1;
        } else {
          page_button_state[page][button_id] = 0;
        }
   
        String button_cmd = from_nextion + String(page_button_state[page][button_id]);
        Serial.println(button_cmd);
      }
      // Serial.print(from_nextion);
    }
    */

    /*
    // RADIO SWAP BUTTONS
    // string from nextion has format "s1" to "s3"
    if(from_nextion.substring(0,1) == "s") {

      // check for sanity
      if(from_nextion.length() == 2) {
        // read radio_values array and swap values
        int idx = from_nextion.substring(1,2).toInt();
        String val1 = radio_values[idx - 1][0];
        String val2 = radio_values[idx - 1][1];
        radio_values[idx - 1][0] = val2;
        radio_values[idx - 1][1] = val1;
        setradiovalues();

      }
      
    }
    */

    /*
    // RADIO SELECT FREQ BUTTONS
    // the string format for the number boxes is "rXXpXX" where rXX is the number box and pXX is its place in the radio_select_state array
    if(from_nextion.substring(0,1) == "r") {

      if(from_nextion.length() == 6) {

        int k;
        int j;
        String cmd = "";

        int array_row = from_nextion.substring(4,5).toInt();
        int array_column = from_nextion.substring(5,6).toInt();
      
        // look up current value and switch state
        int state = radio_select_state[array_row][array_column];
        if(state == 0) {
          resetradiostate();
          radio_select_state[array_row][array_column] = 1;        
        } else {
          resetradiostate();
          radio_select_state[array_row][array_column] = 0;
        }

      setradiodisplay();
      }
    }
    */

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

