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
// * FUNC DECLARATIONS
//
void sendtonextion(char * data);
void sendtopython(char * data);
void nextionlisten();
void pythonlisten();
void pagetonextion(long page);
void updatebuttonstate(long page);
long getbuttonstate(char page [2], char button [2]);
void setbuttonstate(char page [2], char button [2]);
void buttontopython(char page [2], char button [2]);
void radioselecttonextion(char page [2], char numbox [2]);
void setradiostate(long page);
void setradiovalues(long page);
void readrotary();

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

// ROTARY ENCODER LOGIC
//
#define rotaryCLK 6
#define rotaryDT 7
#define rotarySW 5
int rotCurrentState;
int rotLastState;
int rotSwitchState = 0;

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
// format: "vPPVV" where PP is page and VV is value box
char selectedbox[6];
// current radio values
// row 0 - the current values, row 1 - min value. row 2 - max value, row 3 - increments allowed
int radioValues[4][20] = {
        {120,000,130,500,110,000,115,100,0,1,0,0,1,2,5,0,5,5,5,5}, // current
        {118,000,118,000,108,000,108,000,0,2,0,0,0,0,0,0,0,0,0,0}, // min
        {136,990,136,990,117,950,117,950,1,2,9,9,1,7,9,9,7,7,7,7}, // max
        {1,5,1,5,1,5,1,5,1,1,1,1,1,1,1,1,1,1,1,1}// incr
};
// This is the global for the "multi" extended function key
// format: 0 - off, 1 - on
int multikey;


//
// * SETUP
//
void setup() {

    Serial.begin(115200);
    nextion.begin(57600);

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

    pythonlisten();
    nextionlisten();
    readrotary();
}


//
// * MAIN ARDUINO LOOP
//
// incoming message format for buttons/touchspots is -> #LCppccdddd (for example '#9b05014848' means -> string (#) with (9) bytes of data attached (button 01 on page 05 with 4848 as data attached).
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

                // TRAILDATA
                // we get the last trailing 4 characters of data for later use
                char traildata[5];
                sprintf(traildata, "%c%c%c%c", cmdnext[5], cmdnext[6], cmdnext[7], cmdnext[8]);

                // CHANGE PAGE
                //
                if(cmdnext[0] == 'm') {

                    int i;
                    char page[3];
                    sprintf(page, "%c%c", cmdnext[1], cmdnext[2]);
                    pagetonextion(atoi(page));
                }

                // BUTTON PRESSED
                //
                if(cmdnext[0] == 'b') {

                    // check if the multikey has been pressed and set state of it
                    // (any button can be defined as "multi" by its last for characters)
                    if (strcmp(traildata, "MULT") == 0) {
                        if (multikey == 1) {
                            multikey = 0;
                        } else {
                            multikey = 1;
                        }
                    }

                    char pg[3];
                    sprintf(pg, "%c%c", cmdnext[1], cmdnext[2]);

                    char bt[3];
                    sprintf(bt, "%c%c", cmdnext[3], cmdnext[4]);

                    setbuttonstate(pg, bt);
                    buttontopython(pg, bt);
                }

                // NUMBER DISPLAY PRESSED
                //
                if(cmdnext[0] == 'v') {

                    char vpage[3];
                    sprintf(vpage, "%c%c", cmdnext[1], cmdnext[2]);
                    char vbutton[3];
                    sprintf(vbutton, "%c%c", cmdnext[3], cmdnext[4]);
                    radioselecttonextion(vpage, vbutton);
                }
            }
        }
    }
} // END nextionlisten()

// incoming message format from python  -> *LCppccdddd (for example '*9b05014848' means -> string (*) with (9) bytes of data attached (button 01 on page 05 with 4848 as data attached).
// * - means string of data follows
// L - number of bytes
// C - command (b - button, d - dref, c - general command)
// pp - nextion page
// cc - nextion number
// dddd - any data that follows (can be any length)
//
void pythonlisten() {

    if(Serial.available() > 2){
        unsigned char start_char = Serial.read();

        // COMMANDS
        // look for the flag '#' meaning this is an incoming command
        if(start_char == '*'){

            uint8_t len = Serial.read() - '0'; // turn ascii into number
            unsigned long wait1 = millis();
            boolean found = true;

            // wait for the remaining string and time out if needed
            while(Serial.available() < len){
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
                    int asc = Serial.read();
                    char stuff = asc;
                    cmdnext[inl] += stuff;
                }

                // TRAILDATA
                // we get the last trailing 4 characters of data for later use
                char traildata[5];
                sprintf(traildata, "%c%c%c%c", cmdnext[5], cmdnext[6], cmdnext[7], cmdnext[8]);

                // GENERAL COMMANDS
                //
                if(cmdnext[0] == 'c') {

                    // Check if this is startup request from python
                    if (strcmp(traildata, "RVAL") == 0) {

                        // receive startup values from python

                    }


                }

            }
        }
    }
} // END pythonlisten()


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

// SEND TO PYTHON
// sending preformatted string to Python
void sendtopython(char *data) {

    Serial.print(data);
} // END sendtopython()


//
// * HARDWARE FUNCTIONS
//

// READ ROTARY
// reads the rotary control
void readrotary() {

    // ROTARY LOGIC
    //
    rotCurrentState = digitalRead(rotaryCLK);
    if (rotCurrentState != rotLastState  && rotCurrentState == 1){

        int newval;
        char boxid [3];
        char page [3];
        int boxnum;
        long pagenum;
        int incr;
        sprintf(boxid, "%c%c", selectedbox[3], selectedbox[4]);
        sprintf(page, "%c%c", selectedbox[1], selectedbox[2]);
        boxnum = atoi(boxid);
        pagenum = atoi(page);

        // if radio page
        if (pagenum == 2) {
            if (digitalRead(rotaryDT) != rotCurrentState) { // adding values

                incr = radioValues[3][boxnum];

                // incr values depending on multi key
                if (multikey == 1) {
                    newval = radioValues[0][boxnum] + 100;
                } else {
                    newval = radioValues[0][boxnum] + incr;
                }

                // check for limits
                if (newval >= radioValues[1][boxnum] && newval <= radioValues[2][boxnum]) {
                    radioValues[0][boxnum] = newval;
                    setradiovalues(pagenum);
                }

            } else { // subtracting values

                // decr values depending on multi key
                incr = radioValues[3][boxnum];
                if (multikey == 1) {
                    newval = radioValues[0][boxnum] - 100;
                } else {
                    newval = radioValues[0][boxnum] - incr;
                }

                // check for limits
                if (newval >= radioValues[1][boxnum] && newval <= radioValues[2][boxnum]) {
                    radioValues[0][boxnum] = newval;
                    setradiovalues(pagenum);
                }
            }
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
        delay(5); // to stop bounce on switch
    }

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

    if(page == 2) {
        setradiostate(page);
        setradiovalues(page);
    }

} // END pagettonextion()

// UPDATES BUTTONS ON NEXTION ACCORDING TO STATE ARRAY
// checks for current state on given page and sets them thereafter
// format: "pageX.bXXBB.val=1" whare X is page and B is button. Value 1 for on and 0 for off state.
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


// RADIO VALUES SPECIFIC
//

// CHANGE RADIO VALUE BOX STATE
// Changes the select state of a radio value box
// format: "vPPBB" where PP is page and BB is box number
void radioselecttonextion(char page [2], char numbox [2]) {

    int vpage = atoi(page);
    int imgbox = atoi(numbox);
    char vbox[6];
    sprintf(vbox, "v%s%s", page, numbox);

    // check current value and remember new
    int newvalue;
    if(radio_select_state[vpage][imgbox] == 0) {
        newvalue = 1;
        selectedbox[0] = 'v';
        for (int j=1; j<6; j++) {
            selectedbox[j] = vbox[j];
        }
    } else {
        newvalue = 0;
        for (int j; j<5; j++) {
            selectedbox[j] = 0;
        }
    }

    // reset array
    int i;
    for(i=0;i<20;i++) {
        radio_select_state[vpage][i] = 0;
    }

    // set to new value
    radio_select_state[vpage][imgbox] = newvalue;
    setradiostate(atoi(page));
}

// SET RADIO NUMBER STATES
// checks for current number box state for given page and sets a new value
// format: "pageX.iXXBB.picc=8" where X is page and B is box number.
void setradiostate(long page) {

    int i;
    int state;
    char cmd [20];

    // int to char
    char buffer [2];
    snprintf(buffer, sizeof(buffer), "%ld", page);
    char * pg = buffer;

    char id[3];
    sprintf(id, "0%ld", page);

    for(i=0;i<20;i++) {

        state = radio_select_state[page][i];

        if(state == 0) {
            sprintf(cmd, "page%s.i%s%02d.picc=8", pg, id, i);
        } else {
            sprintf(cmd, "page%s.i%s%02d.picc=9", pg, id, i);
        }
        sendtonextion(cmd);
    }
}

// SET RADIO VALUES
// reads the values array and set the radio frequency boxes
// format: "pageX.vXXBB.val=123" where X is the page, BB is the box number and value is the box value.
void setradiovalues(long page) {

    int i;

    char cmd[22];
    char pgf[3];
    sprintf(pgf, "0%ld", page);

    // int to char
    char buffer [2];
    snprintf(buffer, sizeof(buffer), "%ld", page);
    char * pg = buffer;

    int val;
    for(i=0;i<20;i++) {

        val = radioValues[0][i]; // sets current values
        sprintf(cmd, "page%s.v%s%02d.val=%d", pg, pgf, i, val);
        sendtonextion(cmd);
    }
}


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
