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
void sendtonextion(const char * data); // def
void nextionlisten(); // def
void pagetonextion(long page); // def
void setbuttonstate(long page);
void buttontopython(unsigned char page[2], unsigned char button[2]); // def
const char * formatbuttonstring(unsigned char * page, unsigned char * button, unsigned char * state); // def
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

    Serial.begin(115200);
    nextion.begin(38400);

    // SET START PAGE AND TEXT
    //
    const char pg[8] = "page 0\0";
    sendtonextion(pg);

    // rotary enncoder pins
    pinMode(rotaryCLK, INPUT);
    pinMode(rotaryDT, INPUT);
    pinMode(rotarySW, INPUT);
    rotLastState = digitalRead(rotaryCLK);

    // tell the world we are ready
    const char con_color[15] = "con.pco=59387\0";
    const char con_text[22] = "con.txt=\"Connected!\"\0";
    sendtonextion(con_text);
    sendtonextion(con_color);

    Serial.write("hello");
}



//
// MAIN PROGRAM LOOP
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
            unsigned char cmdnext[9] = "";
            if(found){
                int inl;

                // read rest of the incoming chars
                for(inl=0; inl < len; inl++) {
                    unsigned char stuff = nextion.read();
                    cmdnext[inl] += stuff;
                }

                // CHANGE PAGE
                //
                if(cmdnext[0] == 'm') {
                    int i;
                    unsigned char page[3];

                    // get the rest of the characters
                    for(i=0;i<2;i++) {
                        page[i] = cmdnext[i+1];
                    }

                    page[i+1] = '\0';
                    pagetonextion(strtol(reinterpret_cast<const char *>(page), (char **) "\0", 10));
                }

                // BUTTON PRESSED
                //
                if(cmdnext[0] == 'b') {

                    unsigned char pg[2];
                    pg[0] = cmdnext[1];
                    pg[1] = cmdnext[2];

                    unsigned char bt[2];
                    bt[0] = cmdnext[3];
                    bt[1] = cmdnext[4];

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
            // setradiovalues(page);

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
void setbuttonstate(long page) {
    int i;
    long state;

    // int to char
    char buffer [2];
    snprintf(buffer, sizeof(buffer), "%ld", page);
    char * pg = buffer;

    Serial.println(pg);

    for(i=0; i<10; i++) {
        state = page_button_state[page][i];
        unsigned char * cmd;
        // int to char
        char in [1];
        snprintf(buffer, sizeof(buffer), "%s", in);
        char * ii = buffer;

        if(state == 0) {
            // cmd += String(page) + ".b0" + String(page) + "0" + String(i) + ".val=0";
            strcpy(reinterpret_cast<char *>(*cmd), "page");
            strcpy(reinterpret_cast<char *>(*cmd), pg);
            strcpy(reinterpret_cast<char *>(*cmd), ".b0");
            strcpy(reinterpret_cast<char *>(*cmd), pg);
            strcpy(reinterpret_cast<char *>(*cmd), "0");
            strcpy(reinterpret_cast<char *>(*cmd), ii);
            strcpy(reinterpret_cast<char *>(*cmd), ".val=0");
        } else {
            // cmd += String(page) + ".b0" + String(page) + "0" + String(i) + ".val=1";
            strcpy(reinterpret_cast<char *>(*cmd), "page");
            strcpy(reinterpret_cast<char *>(*cmd), pg);
            strcpy(reinterpret_cast<char *>(*cmd), ".b0");
            strcpy(reinterpret_cast<char *>(*cmd), pg);
            strcpy(reinterpret_cast<char *>(*cmd), "0");
            strcpy(reinterpret_cast<char *>(*cmd), ii);
            strcpy(reinterpret_cast<char *>(*cmd), ".val=1");
        }

        // sendtonextion(cmd);
    }
}


//
// * PYTHON FUNCTIONALITY METHODS
//

// GET BUTTON AND SEND TO PYTHON
// receives the button command and formats it with state for python
void buttontopython(unsigned char page[2], unsigned char button[2]) {



    // format into a command string
    // const char * cmdb = formatbuttonstring(page, button, page_button_state[page][button]);



    Serial.println(page[0]);
    Serial.println(page[1]);



    const char * cmdb;
    // cmdb = formatbuttonstring(page, button, reinterpret_cast<unsigned char *>(page_button_state[pg][bt]));

    // Serial.println(cmdb);

    // sendtopython(bcmd);
}

//
// * FORMATTING
//

// FORMAT BUTTON DATA INTO COMMAND STRING
// formats button data into a command string for python
const char * formatbuttonstring(unsigned char * page, unsigned char * button, unsigned char * state) {

    char * pp;
    char * bb;
    char * sta;
    char command[8];
    strcpy(command, "b");

    // int to char
    pp = longtochar(page);
    bb = longtochar(button);
    sta = longtochar(state);

    Serial.println(pp);
    Serial.println(bb);
    Serial.println(sta);

    if(page<10) {
        strcat(command, "0");
        strcat(command, reinterpret_cast<const char *>(&pp));
    } else {
        strcat(command, reinterpret_cast<const char *>(&pp));
    }


    Serial.println(command);


//    if(button>9) {
//        *bb = longtochar(button);
//    } else {
//        *bbb = longtochar(button);
//        strcpy(*bb, "0");
//        strcpy(*bb, *ppp);
//    }

//    strcpy(*command, *pp);
//    strcpy(*command, *bb);
//
//    strcpy(*command, *sta);
//    strcpy(*command, "000");



    return nullptr;
}

// CONVERT LONG TO CHAR ARRAY
char * longtochar(long value) {

    char buffer [10];
    int ret = snprintf(buffer, sizeof(buffer), "%ld", value);
    char * num_string = buffer; //String terminator is added by snprintf

    return num_string;
}