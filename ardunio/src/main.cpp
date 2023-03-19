#include <Arduino.h>

// xflyremote_arduino
// Controller logic for the Arduino/Nextion combo
// License: GPLv3
// https://github.com/mydogspies/xflyremote-hardware
// The python main interface to xplane can be found at;
// https://github.com/mydogspies/xflyremote-main

#include <SoftwareSerial.h>
SoftwareSerial nextion (2, 3);

// char *concat(const char *a, const char *b);

// SERIAL COMS
//

String from_nextion = "";
String from_python = "";
int wait = 1;
int updated = 0;

// PAGE LOGIC
//
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
int rotCounter = 0;
int rotCurrentState;
int rotLastState;
int rotSwitchState = 0;
String rotDirection;

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
String selectedbox = "0";
// current radio values
// row 0 - the current values, row 1 - min value. row 2 - max value, row 3 - increments allowed
String radioValues[4][20] = {
        {"115","000","130","500","110","000","115","100","0","1","9","0","1","2","5","0","5","5","5","5"},
        {"118","000","118","000","108","000","108","000","0","2","0","0","0","2","0","0","0","0","0","0"},
        {"136","990","136","990","117","950","117","950","1","7","5","0","1","7","5","0","7","7","7","7"},
        {"1","5","1","5","1","5","1","5","1","1","1","1","1","1","1","1","1","1","1","1"}
};

// id of img used for different states
const String IMG_ID_NOSEL = "4"; // state 0
const String IMG_ID_SEL = "9"; // state 0


//
// DECLARATIONS
//
void sendtonextion(const char * data);
void nextionlisten();
void readrotary();
void pagetonextion(long page);
char inttochar(int value);
void setradiovalues(int page);
void setradiostate(int page);
//
// SETUP
//
void setup() {

    Serial.begin(115200);
    nextion.begin(38400);

    // SET START PAGE AND TEXT
    //
    const char pg[7] = "page 0";
    sendtonextion(pg);

    // rotary enncoder pins
    pinMode(rotaryCLK, INPUT);
    pinMode(rotaryDT, INPUT);
    pinMode(rotarySW, INPUT);
    rotLastState = digitalRead(rotaryCLK);

    // tell the world we are ready
    const char con_color[14] = "con.pco=59387";
    const char con_text[21] = "con.txt=\"Connected!\"";
    sendtonextion(con_text);
    sendtonextion(con_color);
}

//
// MAIN PROGRAM LOOP
//
void loop() {

    nextionlisten();

//    if(selectedbox != "0") {
//        readrotary();
//    }


}

//
// * MAIN NEXTION LOOP
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

        unsigned char start_char = nextion.read();

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
            unsigned char cmdnext[9] = "";
            if(found){

                int inl;

                for(inl=0; inl < len; inl++) {
                    unsigned char stuff = nextion.read();
                    cmdnext[inl] += stuff;
                }

                // CHANGE PAGE
                //
                if(cmdnext[0] == 'm') {

                    int i;
                    unsigned char page[3];

                    for(i=0;i<2;i++) {
                        page[i] = cmdnext[i+1];
                    }

                    page[i+1] = '\0';
                    pagetonextion(strtol(reinterpret_cast<const char *>(page), (char **) "\0", 10));
                }

                /*
                // BUTTON PRESSED
                //
                if(cmdnext[0] == "b") {
                  buttontopython(cmdnext.substring(0,len));
                }

                // NUMBER DISPLAY PRESSED
                //
                if(cmdnext[0] == "v") {
                  radioselecttonextion(cmdnext.substring(0,len));
                }
                */

            }

        }
    }
}


//
// * MAIN PYTHON LOOP
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

        unsigned char pstart_char = Serial.read();

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
            if(pfound){
                int i;

                for(i=0; i<plen; i++) {
                    unsigned char pstuff = Serial.read();
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
// * SERIAL FUNCTIONS
//

// SEND TO NEXTION
// simply sending preformatted string to Nextion
void sendtonextion(const char * data) {

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
// * NEXTION FUNCTIONALITY METHODS
//

// FORMAT PAGE FOR NEXTION
// formats page number as a Nextion command
// and sets off to check the current state of buttons
void pagetonextion(long page) {

    // int to char
    char buffer [2];
    snprintf(buffer, sizeof(buffer), "%ld", page);
    char * pg = buffer;

    // send off to concat
    char pgtxt[6] = "page ";
    char * con = strcat(pgtxt, pg);
    sendtonextion(con);

    // setbuttonstate(page);

    /* ! FIX
    if(page == 2) {
        // setradiostate(page);
        setradiovalues(page);
    }
    */
}

// SET BUTTONS ACCORDING TO STATE ARRAY
// checks for current state on given page and sets them thereafter
/* ! FIX
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
 */ // ! FIX END

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
/* ! FIX
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
 */ // ! FIX END

// SET RADIO VALUES
// reads the values array and set the radio frequency boxes
/* ! FIX
void setradiovalues(int page) {

    int i;
    // int to char
    char *pg  = inttochar(page);
    // send of to concat to make first part of command string
    char *valb = strcat("v0", pg);
    char *cmd = strcat("page ", pg);

    // iterate through all radio boxes
    char *box[5] = {0};
    char valbox[5] = {0};
    for(i=0;i<20;i++) {

        strncpy(valbox, valb, 5);
        valbox[4] = '\0';
        char *ii  = inttochar(i);

        // format with leading zero
        if(i<10) {
            *box = strcat(valbox, "0");
            *box = strcat(*box, ii);
        } else {
            *box = concat(valb, ii);
        }

        // &cmd[0];

        Serial.println(*cmd);

        // reset some arrays
        memset(&box[0], '\0', sizeof(*box));
        memset(&valbox[0], '\0', sizeof(valbox));

        // char *cmd  = strcat("page ", pg);




        // char cmd[9] = "page" + pg + "." + box + ".val=" + radioValues[0][i];

        char *cmd = concat("page ", pg);
        *cmd = concat(cmd, ".");
        *cmd = concat(cmd, box);
        *cmd = concat(cmd, ".val=");

        char *rv = inttochar(radioValues[0][i].toInt());

        *cmd = concat(cmd, rv);


        // Serial.println(cmd);
        // sendtonextion(cmd);

    }


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
*/ // ! FIX END

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

    int newval;
    int boxid = selectedbox.substring(3,5).toInt();
    int page = selectedbox.substring(1,3).toInt();

    // ROTARY LOGIC
    //
    rotCurrentState = digitalRead(rotaryCLK);
    if (rotCurrentState != rotLastState  && rotCurrentState == 1){


        if (digitalRead(rotaryDT) != rotCurrentState) { // adding values

            newval = radioValues[0][boxid].toInt() + 1;
            radioValues[0][boxid] = String(newval);
            setradiovalues(page);

        } else { // subtracting values


        }


    }
    rotLastState = rotCurrentState;

    // SWITCH LOGIC
    //
    int btnState = digitalRead(rotarySW);

    if (btnState == LOW) {

        if (millis() - rotSwitchState > 100) {
            Serial.println("Button pressed!");
        }

        // Remember last button press event
        rotSwitchState = millis();
    }

    delay(1);

} // END readrotary()

//
// EXTRAS
//

/* ! FIX
char *concat(const char *a, const char *b){
    int lena = strlen(a);
    int lenb = strlen(b);
    char *con = malloc(lena+lenb+1);
    // copy & concat (including string termination)
    memcpy(con,a,lena);
    memcpy(con+lena,b,lenb+1);
    return con;
}

char *inttochar(int value) {
    int length = snprintf(NULL, 0, "%d", value);
    char* val = malloc( length + 1 );
    snprintf(val, length + 1, "%d", value);
    return val;
}
*/ // ! FIX END