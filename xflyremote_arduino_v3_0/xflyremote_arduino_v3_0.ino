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
// [page][button]
int page_button_state[6][10] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
  };
int current_page_set = 0;

// ROTARY ENCODER LOGIC
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
// [page][number_box]
// this one is purely for the internal logic of the select logic
int radio_select_state[6][20] = {
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
};
// This global keeps track of which actual value box is currently selected
String selectedbox;

// transponder state array
// only one single digit selected at one time
// 0 - not active
// 1 - active
int active_state_transp[] = {0, 0, 0, 0};
// current frequencies in each position
// note second item in the responder list is currently not used
// String radio_values[4][2] = {
//  {"118.000","136.990"},
//  {"108.000","117.950"},
//  {"0200","1750"},
//  {"7000","0"}
// };

String radiovalues[5][4] = {
  {"120","000","130","500"},
  {"110","000","115","100"},
  {"0","1","9","0"},
  {"1","2","5","0"},
  {"7","7","7","7"}  
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

//
// SETUP
//
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

//
// MAIN PROGRAM LOOP
//
void loop() {

  nextionlisten();
  readrotary();

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

          // NUMBER DISPLAY PRESSED
          //
          if(cmd.substring(0,1) == "v") {
            radioselecttonextion(cmd.substring(0,len));
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
  if(page == 2) {
    setradiostate(page);
    setradiovalues(page);
  }
}

// SET BUTTONS ACCORDING TO STATE ARRAY
// checks for current state on given page and sets them thereafter
void setbuttonstate(int page) {
  int i;
  int state;
  // String button = "b";
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

// CHANGE RADIO VALUE BOX STATE
// Changes the select state of a radio value box
void radioselecttonextion(String cmd) {
  int i;
  int page = cmd.substring(1,3).toInt();
  int imgbox = cmd.substring(3,5).toInt();
  int newvalue;
  String vbox = cmd.substring(0,5);

  // check current value and remember new
  if(radio_select_state[page][imgbox] == 0) {
    newvalue = 1;
    selectedbox = vbox;
  } else {
    newvalue = 0;
    selectedbox = "0";
  }

  // reset array
  for(i=0;i<20;i++) {
    radio_select_state[page][i] = 0;
  }

  // set to new value
  radio_select_state[page][imgbox] = newvalue;
  setradiostate(page);
  Serial.println(selectedbox);
}


// SET RADIO NUMBER STATES
// checks for current number box state for given page and sets a new value
void setradiostate(int page) {
  int i;
  int state;
  String id = "";
  String cmd;

  for(i=0;i<20;i++) {
    state = radio_select_state[page][i];

    if(i<10) {
        id = "0";
    } else {
      id = "";
    }

    if(state == 0) {
      cmd = "page" + String(page) + ".i0" + String(page) + id + String(i) + ".picc=8";
     } else {
      cmd = "page" + String(page) + ".i0" + String(page) + id + String(i) + ".picc=9"; 
    }
    sendtonextion(cmd);
  }
}

// SET RADIO VALUES
// reads the values array and set the radio frequency boxes
void setradiovalues(int page) {
  int i;
  int k;
  int id = 0;
  String box;
  String valb = "v0" + String(page);
  String cmd;

  // iterate through the radiovalues[][] array
  for(i=0;i<5;i++) {
    for(k=0;k<4;k++) {

      // format with leading zero
      if(id<10) {
        box = valb + "0" + String(id);
      } else {
        box = valb + String(id);
      }
      // send value to nextion
      cmd = "page" + String(page) + "." + box + ".val=" + radiovalues[i][k];
      sendtonextion(cmd);
      id++;
    }
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

//
// HARDWARE FUNCTIONS
//

// READ ROTARY
// reads the rotary control
void readrotary() {
  int currentval;
  int newval;
  int boxid = selectedbox.substring(3,5).toInt();  
  int page = selectedbox.substring(1,3).toInt();

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
    
    if (digitalRead(rotaryDT) != aState) { // this is the positive side
      Serial.println("pos");

      // if there is selected box then find it in the array
      if(selectedbox != "0") {
        if(boxid < 4) {
          newval = radiovalues[0][boxid].toInt() + 1;
          // radiovalues[0][boxid] = String(newval);
          
        }    
      }

      counter ++;

    } else { // this is the negative side
      Serial.println("neg");


      if(selectedbox != "0") {
        if(boxid < 3) {
          newval = radiovalues[0][boxid].toInt() - 1;
          // radiovalues[0][boxid] = String(newval);
          
        }    
      }
      counter --;

    }
    // setradiovalues(page);

  }
  aLastState = aState;
  // 
} // END readrotary()
