#include <Arduino.h>

// xflyremote_arduino
// Controller logic for the Arduino/Nextion combo
// License: GPLv3
// https://github.com/mydogspies/xflyremote-hardware
// The python main interface to xplane can be found at;
// https://github.com/mydogspies/xflyremote-main

#include <SoftwareSerial.h>
SoftwareSerial nextion (2, 3);


//
// * DECLARATIONS
//
void sendtonextion(char * data);
void sendtopython(char * data);
void nextionlisten();
void pagetonextion(long page);
void updatebuttonstate(long page);
long getbuttonstate(char page [2], char button [2]);
void setbuttonstate(char page [2], char button [2]);
void buttontopython(char page [2], char button [2]);
String formatbuttonstring(char page [2], char button [2], long state);
char * longtochar(long value); // def
void readrotary();

// SERIAL COMS
//

String from_nextion = "";
String from_python = "";
int wait = 1;
int updated = 0;

// PAGE LOGIC
//
// [page][button]
long page_button_state[6][10] = {
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
// * SETUP
//
void setup() {

    Serial.begin(57600);
    nextion.begin(9600);

    // SET START PAGE AND TEXT
    //
    char pg[8] = "page 0\0";
    sendtonextion(pg);

    // rotary enncoder pins
    pinMode(rotaryCLK, INPUT);
    pinMode(rotaryDT, INPUT);
    pinMode(rotarySW, INPUT);
    rotLastState = digitalRead(rotaryCLK);

    // tell the world we are ready
    char con_color[15] = "con.pco=59387\0";
    char con_text[22] = "con.txt=\"Connected!\"\0";
    sendtonextion(con_text);
    sendtonextion(con_color);

    Serial.println("Arduino connected and says hi!");
}


//
// * MAIN PROGRAM LOOP
//
void loop() {

    nextionlisten();
    // readrotary();

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
            char cmdnext[9] = {"\0"};
            if(found){
                int inl;

                // read rest of the incoming chars
                for(inl=0; inl < len; inl++) {
                    int asc = nextion.read();
                    char stuff = asc;
                    cmdnext[inl] += stuff;
                }

                // CHANGE PAGE
                //
                if(cmdnext[0] == 'm') {
                    int i;
                    char page[2];

                    // get the rest of the characters
                    for(i=0;i<2;i++) {
                        page[i] = cmdnext[i+1];
                    }

                    pagetonextion(strtol(reinterpret_cast<const char *>(page), (char **) "\0", 10));
                }

                // BUTTON PRESSED
                //
                if(cmdnext[0] == 'b') {

                    char pg[3];
                    sprintf(pg, "%c%c", cmdnext[1], cmdnext[2]);

                    char bt[3];
                    sprintf(bt, "%c%c", cmdnext[3], cmdnext[4]);

                    setbuttonstate(pg, bt);
                    buttontopython(pg, bt);
                }

                /* ! FIX ME
                // NUMBER DISPLAY PRESSED
                //
                if(cmdnext[0] == "v") {
                  radioselecttonextion(cmdnext.substring(0,len));
                }
                */ // ! END FIX

            }

        }
    }
} // END nextionlisten()


//
// * SERIAL FUNCTIONS
//

// SEND TO NEXTION
// simply sending preformatted string to Nextion
void sendtonextion(char *data) {

    nextion.print(data);
    nextion.write(0xFF);
    nextion.write(0xFF);
    nextion.write(0xFF);
} // END sendtonextion()

void sendtopython(char *data) {

    Serial.print(data);
} // END sendtopython()


//
// * HARDWARE FUNCTIONS
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
            // setradiovalues(page);
            Serial.println("pos");

        } else { // subtracting values
            Serial.println("neg");

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

    char pgtxt[6] = "page ";
    char * con = strcat(pgtxt, pg);

    sendtonextion(con); // change page
    updatebuttonstate(page);

    /* ! FIX
    if(page == 2) {
        // setradiostate(page);
        setradiovalues(page);
    }
    */
} // END pagettonextion()

// UPDATES BUTTONS ON NEXTION ACCORDING TO STATE ARRAY
// checks for current state on given page and sets them thereafter
void updatebuttonstate(long page) {
    int i;
    long state;

    // int to char
    char buffer [2];
    snprintf(buffer, sizeof(buffer), "%ld", page);
    char * pg = buffer;

    for(i=0; i<10; i++) {
        state = page_button_state[page][i];
        char cmd_state [18];

        // int to char
        char in [2];
        snprintf(in, sizeof(in), "%d", i);
        char * ii = in;

        if(state == 0) {
            sprintf(cmd_state, "page%s.b0%s0%s.val=0", pg, pg, ii);
        } else {
            sprintf(cmd_state, "page%s.b0%s0%s.val=1", pg, pg, ii);
        }

        sendtonextion(cmd_state);
    }
} // END updatebuttonstate()

// SET A BUTTON's STATE
// checks for current state on a button and flips it
void setbuttonstate(char page [2], char button [2]) {

    long state;
    int pagenum = atoi(page);
    int buttonnum = atoi(button);
    state = page_button_state[pagenum][buttonnum];

    if(state == 0) {
        page_button_state[pagenum][buttonnum] = 1;
    } else {
        page_button_state[pagenum][buttonnum] = 0;
    }
} // END setbuttonstate()

// GET CURRENT BUTTON's STATE
// looks up the state array and returns the current state
long getbuttonstate(char page [2], char button [2]) {

    int i;
    long cstate;
    int pgg = atoi(page);
    int btt = atoi(button);
    cstate = page_button_state[pgg][btt];

    return cstate;
} // END getbuttonstate()


//
// * PYTHON FUNCTIONALITY METHODS
//

// GET BUTTON AND SEND TO PYTHON
// receives the button command and formats it with state for python
void buttontopython(char page [2], char button [2]) {

    long state = getbuttonstate(page, button);
    char cmd [6] = "b";

    sprintf(cmd, "b%s%s%ld", page, button, state);

    sendtopython(cmd);
} // END buttontopython()


//
// * FORMATTING
//
