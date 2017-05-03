//LED 4X7SEG TEST 050515
#include <TimerOne.h>
#include <LiquidCrystal.h>

//#define B_DCF 3
#define B_ALARM1 14 //... das Relais anzustellen
#define B_ALARM2 15 //... den Alarm (Buzzer / Relais) an- / auszustellen
#define B_SNOZ 16  //... Taster der die funktion hat ... den Alarm auszuschalten
#define B_NAP 17    // mittagsschlaf
#define B_SET 8 // drehencoder button <====================================================================================================================================
//#define D_A 7 //drehencoder a
//#define D_B 2 //drehencoder b

#define encoder0PinA  2
#define encoder0PinB  4

#define BUZZER 10 //Buzzer

LiquidCrystal lcd(12, 11, 6, 5, 7, 3); // initialize the library with the numbers of the interface pins

int min = 0;  // Current Time (Minutes)
int hour = 0; // Current Time (Hours)
int date = 23;
int month = 4;
int day = 7;

int tTAmin = 5;
int tTAhour = 91;

int to_m = 30;     // Time offset Min
int to_h = 12;     // Time offset Hour

int al_m1 = 30;     // Alarm1 time Min
int al_h1 = 6;     // Alarm1 time Hour
int al_day1 = 3;    //redo alarm 1 : E) 1mal[1] I) immer[2] A) von montag bis freitag[3] W) samstag und sonntag[4]
int al_m2 = 10;     // Alarm2 time Min
int al_h2 = 8;     // Alarm2 time Hour
int al_day2 = 4;    //redo alarm 2 : E) 1mal[1] I) immer[2] A) von montag bis freitag[3] W) samstag und sonntag[4]

int snooze_s = 0; // Snooze Seconds
int snooze_m = 5; // Snooze Minutes

int napDuration_m = 45;
int napDuration_h = 2;

int alarmMode = 0;   //weckart 0: aus, 1: snooze, 2: Buzzer, 3: Relais
int relayActive = 0; //dauerhafte aktivierung des Relais
int setMode = 0;     //Setting mode

int wakingTime = -1; //Vergangene Sekunden seit Waeckstart
int napTime = -1;    //Vergangene Sekunden seit Mittagsschlafstart
int snoozeTime = -1; //Vergangene Sekunden seit snoozeStart

int curText = 0; //Momentan angezeigte Punkte
int curTime = 0;  //Momentan angezeigte Nummer
int curtTA = 0;   //Momentan angezeigte zeit bis alarm
int curddmm = 0;  //Momentan angezeigter tag und monat
int cural_m1 = 0;   //Momentan angezeigte alarm1 minute
int cural_h1 = 0;   //Momentan angezeigte alarm1 stunde
int cural_day1 = 0;   //Momentan angezeigter alarm1 tag
int cural_m2 = 0;   //Momentan angezeigte alarm2 minute
int cural_h2 = 0;   //Momentan angezeigte alarm2 stunde
int cural_day2 = 0;   //Momentan angezeigter alarm2 tag
int curday = 0;   //Momentan angezeigter tag
int aLastState;
int aState;

int m = 0;


volatile unsigned int encoder0Pos = 0;


#define KY_TIM 10   //key [ms]
#define KY_TIMX 500 //key repeat lock [ms]
#define KY_TIMR 150 //key repeat rate [ms]

struct
{                     //Mpast
    char mod;         //Mode
    char buf;         //last key
    unsigned int tim; //timeout [ms]
} kb;

int timms = 0; //Millisecunden Vorteiler
int tims = 0;  //Sekunden Timer

//----------------------------------------------gp-timer--------------------------------
void gptim() //gp-timer 1ms ir-service
{
    if (kb.tim)
        kb.tim--;
    if (timms)
    {
        timms--;
    } //ms timer
    else
    {
        timms = 999;
        tims = (tims + 1) % 86400; //sec timer
        if (wakingTime >= 0)
            wakingTime++; //wakingTime ist -1 wenn der wecker nicht gerade weckt
        if (napTime >= 0)
            napTime++; //napTime ist -1 wenn der Mittagsschlaf nicht gerade gestartet ist
        if (snoozeTime >= 0)
            snoozeTime++; //snoozeTime ist -1 wenn der snooze nicht gerade gestartet ist
    }

    return;
}
//----------------------------------------------------setup----------------------------

void setup() // The setup routine runs once, when you press Reset:
{
    //Serial.begin(9600);

    lcd.begin(16, 2);
    lcd.clear();
    //delay(5000);

    pinMode(BUZZER, OUTPUT);
    //pinMode(RELAY, OUTPUT);

    pinMode(B_ALARM1, INPUT);
    pinMode(B_ALARM2, INPUT);
    pinMode(B_SNOZ, INPUT);
    pinMode(B_NAP, INPUT);
    pinMode(B_SET, INPUT);

    //pinMode(D_A, INPUT);
    //pinMode(D_B, INPUT);

    pinMode (encoder0PinA,INPUT);
    pinMode (encoder0PinB,INPUT);

    //pinMode(B_DCF, INPUT_PULLUP);

    digitalWrite(BUZZER, 0);
    //digitalWrite(RELAY, 0);

    Timer1.initialize(1000);       // set a timer i us (1000us=1ms)
    Timer1.attachInterrupt(gptim); // attach service
    //aLastState = digitalRead(D_A);  // Reads the initial state of REA

      pinMode(encoder0PinA, INPUT); 
  digitalWrite(encoder0PinA, HIGH);       // turn on pull-up resistor
  pinMode(encoder0PinB, INPUT); 
  digitalWrite(encoder0PinB, HIGH);       // turn on pull-up resistor

  attachInterrupt(0, doEncoder, CHANGE);  // encoder pin on interrupt 0 - pin 2
  Serial.begin (9600);
  Serial.println("start");                // a personal quirk


}

//-----------------------------------------------keys---------------------------------
unsigned char getkey(void) //get a key
{
    unsigned char ky = 0;

    if (digitalRead(B_ALARM1))
        ky = 1; //alarm2
    else if (digitalRead(B_ALARM2))
        ky = 2; //alarm2
    else if (digitalRead(B_SNOZ))
        ky = 3; //snooze
    else if (digitalRead(B_NAP))
        ky = 4; //nap (Mittagsschlaf)
    else if (digitalRead(B_SET))
        ky = 5; //set

    //reset key
    if (!ky)
    {
        kb.mod = 0;
        kb.buf = 0;
        return (0);
    }
    //einschwingzeit
    if (!kb.mod)
    {
        kb.mod = 1;
        kb.buf = ky;
        kb.tim = KY_TIM;
        return (0);
    }
    //widerholsperre
    if ((kb.mod == 1) && !kb.tim)
    {
        kb.mod = 2;
        kb.tim = KY_TIMX;
        return (ky);
    }
    //wiederholung
    if ((kb.mod == 2) && !kb.tim)
    {
        kb.tim = KY_TIMR;
        return (ky);
    }

    return (0);
}
//--------------------------------------------------------------------------pot--------
/*int pot() {
  aState = digitalRead(D_A); // Reads the "current" state of the outputA
  // If the previous and the current state of the outputA are different, that means a Pulse has occured
  if (aState != aLastState) {
    // If the outputB state is different to the outputA state, that means the encoder is rotating clockwise
    if (digitalRead(D_B) != aState) {
      return 1;
    } else {
      return -1;
    }
  }
  aLastState = aState; // Updates the previous state of the outputA with the current state
  return 0;
}*/
//------------------------------------------generate time-----------------------------
int genTime()
{
    int tmpMin = tims / 60 + to_m;
    min = tmpMin % 60;
    hour = (tmpMin / 60 + to_h) % 24;
    return (min + hour * 100);
}
//---------------------------------writeTimeIn xx:yy format to LCD---------------------
void writeTime(int i, int time, int x, int y) {
    while (i){
        i--;
        lcd.setCursor(i + x, 0 + y);
        lcd.print(time % 10);
        time /= 10;
    }
}

//------------------------------------------timeTilAlarm------------------------------
int timeTilAlarm(){
    
    int tTAtmpMin = tims / 60 + to_m;
    min = tTAtmpMin % 60;
    hour = (tTAtmpMin / 60 + to_h) % 24;
    return (tTAmin + tTAhour * 100);
}

//-------------------------------------------update display----------------------------
void display(int showSecondRow, int updateAllDisplay)
{
    if (showSecondRow){
        goto onlyRowOne;
    } 
    //-----------------------2Displayzeile-------------------------------
    int cal_day1;
    cal_day1 = al_day1;
    if (al_day1 != cural_day1 /*|| updateAllDisplay*/) {

        lcd.setCursor(0, 1);        //widerhohlungsmodus

        switch(al_day1) {
        case 0:
             lcd.print("0");
            break;
        case 1:
             lcd.print("E");
            break;
        case 2:
             lcd.print("I");
            break;
        case 3:
             lcd.print("A");
            break;
        case 4:
             lcd.print("W");
            break;
         }
        cural_day1 = cal_day1;
    }

    int cal_h1;
    cal_h1 = al_h1;
    if (cal_h1 != cural_h1 /*|| updateAllDisplay*/) {
        writeTime(2, al_h1, 1, 1);
        cural_h1 = cal_h1;
    }

    lcd.setCursor(3, 1);
    lcd.print("|");

    int cal_m1;
    cal_m1 = al_m1;
     if (cal_m1 != cural_m1 /*|| updateAllDisplay*/) {
         writeTime(2, al_m1, 4, 1);
         cural_m1 = cal_m1;
     }


    int cal_day2;
    cal_day2 = al_day2;
     if (cal_day2 != cural_day2 /*|| updateAllDisplay*/) {
        lcd.setCursor(7, 1);        //widerhohlungsmodus
        switch(al_day2) {
        case 0:
            lcd.print("0");
            break;
        case 1:
            lcd.print("E");
            break;
        case 2:
            lcd.print("I");
            break;
        case 3:
            lcd.print("A");
            break;
        case 4:
            lcd.print("W");
            break;
        }
        cural_day2 = cal_day2;
     }


    int cal_h2;
    cal_h2 = al_h2;
    if (cal_h2 != cural_h2 /*|| updateAllDisplay*/) {
        writeTime(2, al_h2, 8, 1);
        cural_h2 = cal_h2;
    }

    lcd.setCursor(10, 1);
    lcd.print("|");

    int cal_m2;
    cal_m2 = al_m2;
    if (cal_m2 != cural_m2 /*|| updateAllDisplay*/) {
        writeTime(2, al_m2, 11, 1);
        cural_m2 = cal_m2;
    }


    int cday;
    cday = day;
    if (cday != curday /*|| updateAllDisplay*/) {
        lcd.setCursor(15, 1);
        lcd.print(day);
        curday = cday;
    }
    
    //-------------------1Displayzeile------------------------------------
    onlyRowOne:
    //uhrzeit
    int cTime = genTime();
     if (cTime != curTime /*|| updateAllDisplay*/) {
         writeTime(2, hour, 0, 0);
         lcd.setCursor(2, 0);
         lcd.print("|");
         writeTime(2, min, 3, 0);
         curTime = cTime;
     }   
     //zeit bis alarm
     int ctTA = timeTilAlarm();
     if (ctTA != curtTA /*|| updateAllDisplay*/) {
         writeTime(2, tTAhour, 6, 0);
         lcd.setCursor(8, 0);
         lcd.print("|");
         writeTime(2, tTAmin, 9, 0);
         curtTA = ctTA;
     }   
     int cddmm = date + month * 100;
     if (cddmm != curddmm /*|| updateAllDisplay*/) {
         writeTime(2, date, 12, 0);
         writeTime(2, month, 14, 0);
         curddmm = cddmm;
     }
     //DD Datum
    // writeTime(2, date, 12, 0);
     //MM Datum
    // writeTime(2, month, 14, 0);
}   
//--------------------------------------------input umgang-------------------------
void handleInput() {
    switch (getkey())
    {
        case 1:
            break;
        case 2:
            break;
        case 3:
            break;
        case 4:
            break;
        case 5:
            break;
    }



}

void doEncoder() {
  /* If pinA and pinB are both high or both low, it is spinning
   * forward. If they're different, it's going backward.
   *
   * For more information on speeding up this process, see
   * [Reference/PortManipulation], specifically the PIND register.
   */
  if (digitalRead(encoder0PinA) == digitalRead(encoder0PinB)) {
    encoder0Pos++;
  } else {
    encoder0Pos--;
  }

  Serial.println (encoder0Pos, DEC);
}

/* See this expanded function to get a better understanding of the
 * meanings of the four possible (pinA, pinB) value pairs:
 */
void doEncoder_Expanded(){
  if (digitalRead(encoder0PinA) == HIGH) {   // found a low-to-high on channel A
    if (digitalRead(encoder0PinB) == LOW) {  // check channel B to see which way
                                             // encoder is turning
      encoder0Pos = encoder0Pos - 1;         // CCW
    } 
    else {
      encoder0Pos = encoder0Pos + 1;         // CW
    }
  }
  else                                        // found a high-to-low on channel A
  { 
    if (digitalRead(encoder0PinB) == LOW) {   // check channel B to see which way
                                              // encoder is turning  
      encoder0Pos = encoder0Pos + 1;          // CW
    } 
    else {
      encoder0Pos = encoder0Pos - 1;          // CCW
    }

  }
  Serial.println (encoder0Pos, DEC);          // debug - remember to comment out
                                              // before final program run
  // you don't want serial slowing down your program if not needed
}
//- -----------------------------------------loop----------------------------------
void loop()
{

    //handleInput();

    //Alarm - Aktivierung
    //checkAlarm();

    //Alarm - ausfuehrung
    //doAlarm();

    //display(0,0);

    //m += pot();

    //lcd.setCursor(0, 0);
    //lcd.print(m);

       
}
