//LED 4X7SEG TEST 050515
#include <TimerOne.h>
#include <LiquidCrystal.h>

#define DCF 2
#define B_ALARM1 18 //... das Relais anzustellen
#define B_ALARM2 19 //... den Alarm (Buzzer / Relais) an- / auszustellen
#define B_SNOZ 20   //... Taster der die funktion hat ... den Alarm auszuschalten
#define B_NAP 21    // mittagsschlaf
#define B_A 22      //a (-)
#define B_B 23      //b (+)

#define BUZZER 10 //Buzzer

LiquidCrystal lcd(12, 11, 6, 5, 4, 3); // initialize the library with the numbers of the interface pins

struct Time
{
    int m; //Minutes
    int h; //Stunde
};

struct DateTime
{
    Time tim;
    int date = 23;
    int month = 4;
    int day = 6;
    int year = 2200;
};

struct Alarm
{
    Time tim;
    int day; //redo alarm 1 : E) 1mal[1] I) immer[2] A) von montag bis freitag[3] W) samstag und sonntag[4]
};

struct DateTime t; // current time
struct Time to;    // Time offset

struct Alarm al1;
struct Alarm al2;

struct Time napDuration;
int snooze = 5; // Snooze Minutes

int wakingTime = -1; //Vergangene Sekunden seit Waeckstart
int napTime = -1;    //Vergangene Sekunden seit Mittagsschlafstart
int snoozeTime = -1; //Vergangene Sekunden seit snoozeStart

int curTime = 0; //Momentan angezeigte Nummer
int curtTA = 0;  //Momentan angezeigte zeit bis alarm
int curddmm = 0; //Momentan angezeigter tag und monat
int curday = 0;  //Momentan angezeigter tag

struct Alarm curAl1;
struct Alarm curAl2;

int setupAuswahlActiv = 0; //Ob in Hauptmenue

int onlyOneRow = 0; //Nur die erste Zeile der Displays wird gezeigt

int dayExchange = 0; //

char wochentagExchange[2];

const char wochentage[7][3] = {"Mo", "Di", "Mi", "Do", "Fr", "Sa", "So"};
const char wochentageReRun[8] = {'E', 'I', 'A', 'W', 'e', 'i', 'a', 'w'};

const short monthLengths[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

struct
{                    //
    unsigned int yy; //received date
    unsigned int mo;
    unsigned int dd;
    unsigned int vld; //date is valid
    unsigned int hh;  //received time
    unsigned int mm;
    unsigned int pmh; //previus hh*60+mm
    unsigned int vlt; //valid time
    //recieve data
    unsigned char fin; //buffer finalized for decode
    unsigned char cnf; //finalized count for debug
    unsigned int cnt;  //bit count
    unsigned int le;   //leading edge time
    unsigned int tim;  //time since prevoius leading edge
    char actv;         //dcf activ
    signed int bit[60];
} dcf;

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
        if (tims == 0)
        {
            t.day = (t.day + 1) % 7;

            if (((t.month == 2 && t.year % 4 == 0) ? 1 : 0) + monthLengths[t.month - 1] <= t.date)
            {
                t.date = 0;
                if (++t.month == 13)
                {
                    t.month = 1;
                    t.year++;
                }
            }
            t.date++;
        }
        if (wakingTime >= 0)
            wakingTime++; //wakingTime ist -1 wenn der wecker nicht gerade weckt
        if (napTime >= 0)
            napTime++; //napTime ist -1 wenn der Mittagsschlaf nicht gerade gestartet ist
        if (snoozeTime >= 0)
            snoozeTime++; //snoozeTime ist -1 wenn der snooze nicht gerade gestartet ist
    }

    return;
}
//---internal-----dcf_rx--------------------------------------------------------------

void dcf_rx(void) //DCF77 interupt handler
{
    int tim;
    byte val = digitalRead(DCF);
    dcf.actv = 1;
    // dcf.tim inc 1ms done by gptime
    // detected pulse is too short: incorrect pulse: reject
    if ((dcf.tim - dcf.le) < 60)
        return;

    // flank is detected quickly after previous flank up: reject
    if (dcf.tim < 700)
        return;

    //	if(val)						// Flank down invertiert
    if (!val) // Flank down
    {
        if (!dcf.le)
            dcf.le = dcf.tim;
        //digitalWrite(DCFA, 0);
        return;
    }

    if (dcf.le) // Flank up
    {
        //digitalWrite(DCFA, 1);
        tim = dcf.tim - dcf.le; //trailing edge: low/high bit time
        dcf.tim = tim;          //set time = last leading edge
        if ((dcf.le) > 1600)    //start detected
        {
            if (dcf.cnt == 59)
            {
                dcf.fin = 1;
                dcf.cnf++;
            }
            dcf.cnt = 0;
        }
        dcf.cnt++; //save value
        if (dcf.cnt < 60)
            dcf.bit[dcf.cnt] = tim;
        dcf.le = 0; //reset leading edge time
        return;
    }
    return;
}
//----------------------------------------------------------------------dcf_getv-------
unsigned int dcf_getv(int i0, int i9, int th) //get dcf-decoded value
{
    Serial.println("dcf_getv");
    //th bit schwellwert >th bit=1
    unsigned int v, c, i;

    v = 0;
    c = 1; //c=1,2,4,8,10,20,40,80;
    for (i = i0; i <= i9; i++)
    {
        if (dcf.bit[i] > th)
            v = v + c;
        c = c * 2;
        if (c == 16)
            c = 10;
    }
    return (v);
}
//------------------------------------------------------------------------dcf_getc------
unsigned int dcf_getc(int i0, int i9, int th) //get dcf-parity-valid
{
    Serial.println("dcf_getc");
    //th bit schwellwert >th bit=1
    unsigned int i, v;
    boolean p, c;

    p = 0;
    c = 0;
    if (dcf.bit[i9 + 1] > th)
        c = 1;
    for (i = i0; i <= i9; i++)
    {
        if (dcf.bit[i] > th)
            p = ~p; //toggle parity
    }
    if (p && c)
        return (1); //ok
    if ((!p) && (!c))
        return (1); //ok
    return (0);
}
//-----------------------dcfDecode----------------------------------------------------
unsigned int dcfDecode()
{
    Serial.println("dcfDecode");
    unsigned int th, mh;

    //splittime: low max 120ms, high min 180ms
    //th=112;			//ELV-9461059
    th = 130; //CONRAD-HK56-BS/0
    dcf.mm = dcf_getv(22, 28, th);
    dcf.hh = dcf_getv(30, 35, th);
    dcf.vlt = dcf_getc(22, 28, th); //parity mm
    if (dcf.vlt)
        th = 130; //CONRAD-HK56-BS/0
    dcf.mm = dcf_getv(22, 28, th);
    dcf.hh = dcf_getv(30, 35, th);
    dcf.vlt = dcf_getc(22, 28, th); //parity mm
    if (dcf.vlt)
        dcf.vlt = dcf_getc(30, 35, th); //parity hh
    if (dcf.vlt)                        //previous+1 = current
    {
        mh = (dcf.hh * 60) + dcf.mm;
        if (((dcf.pmh + 1) % 1440) != mh)
            dcf.vlt = 0;
        dcf.pmh = mh;
    }
    dcf.dd = dcf_getv(37, 42, th);
    dcf.mo = dcf_getv(46, 50, th);
    dcf.yy = dcf_getv(51, 58, th);
    dcf.vld = dcf_getc(37, 58, th);
    dcf.fin = 0; //decoded

    return (1);
}

//----------------------------------------------------setup----------------------------
void setup() // The setup routine runs once, when you press Reset:
{

    lcd.begin(16, 2);
    lcd.clear();
    //delay(5000);

    pinMode(BUZZER, OUTPUT);

    pinMode(B_ALARM1, INPUT);
    pinMode(B_ALARM2, INPUT);
    pinMode(B_SNOZ, INPUT);
    pinMode(B_NAP, INPUT);
    pinMode(B_A, INPUT);
    pinMode(B_A, INPUT);

    //pinMode(B_DCF, INPUT_PULLUP);

    digitalWrite(BUZZER, 0);
    //digitalWrite(RELAY, 0);

    Timer1.initialize(1000);       // set a timer i us (1000us=1ms)
    Timer1.attachInterrupt(gptim); // attach service
                                   //aLastState = digitalRead(D_A);  // Reads the initial state of REA

    Serial.begin(9600);

    Serial.println("dcfSetup");

    dcf.yy = 0;
    dcf.mo = 0;
    dcf.dd = 0;
    dcf.hh = 0;
    dcf.mm = 0;
    dcf.pmh = 0;
    dcf.vld = 0;
    dcf.vlt = 0;
    dcf.cnt = 0;
    dcf.cnf = 0;
    dcf.fin = 0;
    dcf.le = 0;
    dcf.tim = 0;
    dcf.actv = 0;

    al1.tim.m = 30;
    al1.tim.h = 6;
    al1.day = 2;

    al2.tim.m = 30;
    al2.tim.h = 6;
    al2.day = 5;

    to.m = 30;
    to.h = 12;

    napDuration.m = 45;
    napDuration.h = 0;

    pinMode(DCF, INPUT);                //input dcf-signal
                                        //	attachInterrupt(3, dcf_rx, CHANGE);	//DCF enable ir3
    attachInterrupt(1, dcf_rx, CHANGE); //DCF enable ir1
}

//-----------------------------------------------keys---------------------------------
unsigned char getkey(void) //get a key
{
    unsigned char ky = 0;

    if (digitalRead(B_ALARM1))
        ky = 1; //alarm1
    else if (digitalRead(B_ALARM2))
        ky = 2; //alarm2
    else if (digitalRead(B_SNOZ))
        ky = 3; //snooze
    else if (digitalRead(B_NAP))
        ky = 4; //nap (Mittagsschlaf)
    else if (digitalRead(B_A))
        ky = 5; //a (+)
    else if (digitalRead(B_B))
        ky = 6; //b (-)

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
//------------------------------------------generate time----------------------------
int genTime()
{
    return (t.tim.m + t.tim.h * 100);
}
//----------------------------------------------calculate current time---------------
void calcCT()
{
    int tmpMin = tims / 60 + to.m;
    t.tim.m = tmpMin % 60;
    t.tim.h = (tmpMin / 60 + to.h) % 24;
}
//---------------------------------nummer mit 0 am anfang auf lcd---------------------
void fulPlott(int i, int time, int x, int y)
{
    while (i)
    {
        i--;
        lcd.setCursor(i + x, 0 + y);
        lcd.print(time % 10);
        time /= 10;
    }
}
//------------------------------------writeTimeIn xx:yy format to LCD-----------------
void writeTime(int time1, int time2, int x, int y)
{
    fulPlott(2, time1, x, y);
    lcd.setCursor(x + 2, y);
    lcd.print(":");
    fulPlott(2, time2, x + 3, y);
}
//------------------------------------------timeTilAlarm------------------------------
int timeTilAlarm()
{
    int til[4] = {32767, 32767, 32767, 32767};

    if (napTime >= 0)
    {
        til[0] = napDuration.h * 60 + napDuration.m - napTime / 60;
    }

    if (snoozeTime >= 0)
    {
        til[1] = snooze - snoozeTime / 60;
    }

    int al1_days = 0; //TODO: Unit test

    if (al1.tim.h * 60 + al1.tim.m < t.tim.h * 60 + t.tim.m)
    {
        al1_days++;
        for (int d = 1; !checkAlarmRepeatAtDay(al1.day, (t.day + d) % 7) && d < 7; d++)
            al1_days++;
    }
    else
    {
        for (int d = 0; !checkAlarmRepeatAtDay(al1.day, (t.day + d) % 7) && d < 7; d++)
            al1_days++;
    }

    if (al1_days < 7)
        til[2] = (24 * 60 * al1_days) + ((al1.tim.h * 60 + al1.tim.m) - (t.tim.h * 60 + t.tim.m));

    int al2_days = 0; //TODO: Unit test

    if (al2.tim.h * 60 + al2.tim.m < t.tim.h * 60 + t.tim.m)
    {
        al2_days++;
        for (int d = 1; !checkAlarmRepeatAtDay(al2.day, (t.day + d) % 7) && d < 7; d++)
            al2_days++;
    }
    else
    {
        for (int d = 0; !checkAlarmRepeatAtDay(al2.day, (t.day + d) % 7) && d < 7; d++)
            al2_days++;
    }

    if (al2_days < 7)
        til[3] = (24 * 60 * al2_days) + ((al2.tim.h * 60 + al2.tim.m) - (t.tim.h * 60 + t.tim.m));

    int minVal = 32767;
    for (int i = 0; i < 4; i++)
        if (til[i] < minVal)
            minVal = til[i];

    if (minVal == 32767)
        return 0;
    if (minVal >= 24 * 60)
        return -(minVal / (24 * 60) * 100 + (minVal / 60) % 24);
    return minVal / 60 * 100 + minVal % 60;
}
//----------------------------------------------------------------setupAuswahl---------
void setupAuswahl()
{

    switch (setupAuswahlActiv)
    {
    case 1:
        switch (getkey())
        {
        case 1:
            setupAuswahlActiv = 2;
            break;
        case 2:
            setupAuswahlActiv = 5;
            break;
        case 3:
            setupAuswahlActiv = 10; // snooze
            break;
        case 4:
            setupAuswahlActiv = 8;
            break;
        case 5:
        case 6:
            setupAuswahlActiv = 0;
            break;
        }
        break;
    case 2:
        switch (getkey())
        {
        case 1:
            setupAuswahlActiv = 3;
            break;
        case 5:
            if (al1.tim.h >= 23)
                al1.tim.h = 0;
            else
                al1.tim.h++;
            break;
        case 6:
            if (al1.tim.h <= 0)
                al1.tim.h = 23;
            else
                al1.tim.h--;
            break;
        }
        break;
    case 3:
        switch (getkey())
        {
        case 1:
            setupAuswahlActiv = 4;
            setupZweiteZeile(1);
            break;
        case 5:
            if (al1.tim.m >= 59)
                al1.tim.m = 0;
            else
                al1.tim.m++;
            break;
        case 6:
            if (al1.tim.m <= 0)
                al1.tim.m = 59;
            else
                al1.tim.m--;
            break;
        }
        break;
    case 4:
        if (al1.day >= 4)
            al1.day -= 4;
        switch (getkey())
        {
        case 1:
            setupAuswahlActiv = 0;
            break;
        case 5:
            if (al1.day >= 3)
                al1.day = 0;
            else
                al1.day++;
            break;
        case 6:
            if (al1.day <= 0)
                al1.day = 3;
            else
                al1.day--;
            break;
        }
        break;
    case 5:
        switch (getkey())
        {
        case 2:
            setupAuswahlActiv = 6;
            break;
        case 5:
            if (al2.tim.h >= 23)
                al2.tim.h = 0;
            else
                al2.tim.h++;
            break;
        case 6:
            if (al2.tim.h <= 0)
                al2.tim.h = 23;
            else
                al2.tim.h--;
            break;
        }
        break;
    case 6:
        switch (getkey())
        {
        case 2:
            setupAuswahlActiv = 7;
            setupZweiteZeile(1);
            break;
        case 5:
            if (al2.tim.m >= 59)
                al2.tim.m = 0;
            else
                al2.tim.m++;
            break;
        case 6:
            if (al2.tim.m <= 0)
                al2.tim.m = 59;
            else
                al2.tim.m--;
            break;
        }
        break;
    case 7:
        if (al2.day >= 4)
            al2.day -= 4;
        switch (getkey())
        {
        case 2:
            setupAuswahlActiv = 0;
            break;
        case 5:
            if (al2.day < 3)
            {
                al2.day++;
            }
            break;
        case 6:
            if (al2.day > 0)
            {
                al2.day--;
            }
            break;
        }
        break;
    case 8:
        switch (getkey())
        {
        case 4:
            setupAuswahlActiv = 9;
            break;
        case 5:
            if (napDuration.h >= 23)
                napDuration.h = 0;
            else
                napDuration.h++;
            break;
        case 6:
            if (napDuration.h <= 0)
                napDuration.h = 23;
            else
                napDuration.h--;
            break;
        }
        break;
    case 9:
        switch (getkey())
        {
        case 4:
            setupAuswahlActiv = 0;
            break;
        case 5:
            if (napDuration.m >= 59)
                napDuration.m = 0;
            else
                napDuration.m++;
            break;
        case 6:
            if (napDuration.m <= 0)
                napDuration.m = 59;
            else
                napDuration.m--;
            break;
        }
        break;

    case 10:
        switch (getkey())
        {
        case 3:
            setupAuswahlActiv = 0;
            break;
        case 5:
            if (snooze >= 59)
                snooze = 0;
            else
                snooze++;
            break;
        case 6:
            if (snooze <= 0)
                snooze = 59;
            else
                snooze--;
            break;
        }
        break;
    }
    if (!setupAuswahlActiv)
    {
        lcd.clear();
        onlyOneRow = 0;
        ersteDisplayZeile(1);
        zweiteDisplayZeile(1);
    }
}
//-------------------------------------------2Displayzeile----------------------------
void zweiteDisplayZeile(int updateAllDisplay)
{
    if (al1.day != curAl1.day || updateAllDisplay)
    {
        lcd.setCursor(0, 1); //widerhohlungsmodus

        lcd.print(wochentageReRun[al1.day]);

        curAl1.day = al1.day;
    }

    if (al1.tim.m != curAl1.tim.m || al1.tim.h != curAl1.tim.h || updateAllDisplay)
    {
        writeTime(al1.tim.h, al1.tim.m, 1, 1);
        curAl1.tim.h = al1.tim.h;
        curAl1.tim.m = al1.tim.m;
    }

    if (al2.day != curAl2.day || updateAllDisplay)
    {
        lcd.setCursor(7, 1); //widerhohlungsmodus

        lcd.print(wochentageReRun[al2.day]);

        curAl2.day = al2.day;
    }

    if (al2.tim.h != curAl2.tim.h || al2.tim.m != curAl2.tim.m || updateAllDisplay)
    {
        writeTime(al2.tim.h, al2.tim.m, 8, 1);
        curAl2.tim.h = al2.tim.h;
        curAl2.tim.m = al2.tim.m;
    }

    if (t.day != curday || updateAllDisplay)
    {
        lcd.setCursor(14, 1);
        lcd.print(wochentage[t.day]);
        curday = t.day;
    }
}
//-------------------1Displayzeile---------------------------------------------------

void ersteDisplayZeile(int updateAllDisplay)
{

    int cTime = genTime();
    if (cTime != curTime || updateAllDisplay)
    {
        writeTime(t.tim.h, t.tim.m, 0, 0);
        curTime = cTime;
    }
    //zeit bis alarm
    int ctTA = timeTilAlarm();
    if (ctTA != curtTA || updateAllDisplay)
    {
        if (ctTA > 0) // Alarm klingelt in ctTA (Format: hhmm)
            writeTime(ctTA / 100, ctTA % 100, 6, 0);
        else if (ctTA == 0) // Alarm deaktiviert
        {
            lcd.setCursor(6, 0);
            lcd.print("     ");
        }
        else // Alarm ist mehrere tage hin (ctTA format: -tthh) auf display: 'T't:hh. zB T3:04 (3 tage und 4 stunden)
        {
            ctTA = -ctTA;
            lcd.setCursor(6, 0);
            lcd.print("T");
            lcd.setCursor(7, 0);
            lcd.print(ctTA / 100);
            lcd.setCursor(8, 0);
            lcd.print(":");
            fulPlott(2, ctTA % 100, 9, 0);
        }
        curtTA = ctTA;
    }
    int cddmm = t.date + t.month * 100;
    if (cddmm != curddmm || updateAllDisplay)
    {
        fulPlott(2, t.date, 12, 0);
        fulPlott(2, t.month, 14, 0);
        curddmm = cddmm;
    }
}
//--------------------------------------------input umgang-------------------------
void handleInput()
{
    switch (getkey())
    {
    case 1:
        if (al1.day > 3)
            al1.day -= 4;
        else
            al1.day += 4;
        break;
    case 2:
        if (al2.day > 3)
            al2.day -= 4;
        else
            al2.day += 4;
        break;
    case 3:
        break;
    case 4:
        if (napTime >= 0)
            napTime = -1;
        else
            napTime = 0;
        break;
    case 5:
        setupZweiteZeile(0);
        setupAuswahlActiv = 1;
        break;
    case 6:
        setupZweiteZeile(0);
        setupAuswahlActiv = 1;
        break;
    }
}
//-------------------------------------------------setupZweiteZeile----------------
void setupZweiteZeile(int updateAllDisplay)
{
    if (!onlyOneRow)
    {
        lcd.clear();
        ersteDisplayZeile(1);
        onlyOneRow = 1;
    }
    switch (setupAuswahlActiv)
    {
    case 1:
        lcd.setCursor(0, 1);
        lcd.print("Menue");
        break;
    case 2:
        lcd.setCursor(0, 1);
        lcd.print("A 1 Stunde");
        writeTime(al1.tim.h, al1.tim.m, 11, 1);
        break;
    case 3:
        lcd.setCursor(0, 1);
        lcd.print("A 1 Minute");
        writeTime(al1.tim.h, al1.tim.m, 11, 1);
        break;
    case 4:
        if (al1.day != curAl1.day || updateAllDisplay)
        {
            lcd.clear();
            ersteDisplayZeile(1);
            lcd.setCursor(0, 1);
            lcd.print("A 1 reRun");
            lcd.setCursor(15, 1); //widerhohlungsmodus

            lcd.print(wochentageReRun[al1.day]);

            curAl1.day = al1.day;
        }
        break;
    case 5:
        lcd.setCursor(0, 1);
        lcd.print("A 2 Stunde");
        writeTime(al2.tim.h, al2.tim.m, 11, 1);
        break;
    case 6:
        lcd.setCursor(0, 1);
        lcd.print("A 2 Minute");
        writeTime(al2.tim.h, al2.tim.m, 11, 1);
        break;
    case 7:
        if (al2.day != curAl2.day || updateAllDisplay)
        {
            lcd.clear();
            ersteDisplayZeile(1);
            lcd.setCursor(0, 1);
            lcd.print("A 2 reRun");
            lcd.setCursor(15, 1); //widerhohlungsmodus

            lcd.print(wochentageReRun[al2.day]);

            curAl2.day = al2.day;
        }
        break;
    case 8:
        lcd.setCursor(0, 1);
        lcd.print("Mittag H");
        writeTime(napDuration.h, napDuration.m, 11, 1);
        break;
    case 9:
        lcd.setCursor(0, 1);
        lcd.print("Mittag M");
        writeTime(napDuration.h, napDuration.m, 11, 1);
        break;
    case 10:
        lcd.setCursor(0, 1);
        lcd.print("Snooze M");
        fulPlott(2, snooze, 14, 1);
        break;
    }
}
//-------------------------------Alarm - ausfuehrung--------------------------------
void doAlarm()
{
    if (wakingTime >= 0)
        tone(BUZZER, 300, 5);
}
//--------------------------------------------------
int checkAlarmRepeat(int repeat)
{
    return checkAlarmRepeatAtDay(repeat, t.day);
}
int checkAlarmRepeatAtDay(int repeat, int day)
{
    switch (repeat)
    {
    case 0:
    case 1:
        return 1;
    case 2:
        return (day <= 4) ? 1 : 0;
    case 3:
        return (day > 4) ? 1 : 0;
    case 4:
    case 5:
    case 6:
    case 7:
        return 0;
    default:
        return 1;
    }
}
//-----------------------------Alarm - Aktivierung-------------------------------------
void checkAlarm()
{
    if (napDuration.h * 3600 + napDuration.m * 60 <= napTime)
    {
        wakingTime = 0;
        napTime = -1;
    }
    else if (((t.tim.m == al2.tim.m && t.tim.h == al2.tim.h && checkAlarmRepeat(al2.day)) ||
              (t.tim.m == al1.tim.m && t.tim.h == al1.tim.h && checkAlarmRepeat(al1.day))) &&
             tims % 60 == 0)
    {
        wakingTime = 0;
    }
    else if (snoozeTime >= snooze * 3600)
    {
        wakingTime = 0;
    }
}
//-------------------------alarmDeactivate-----------------------------------------
void alarmDeactivate()
{
    switch (getkey())
    {
    case 1:
    case 2:
    case 4:
    case 5:
    case 6:
        wakingTime = -1;
        snoozeTime = 0;
        break;
    case 3:
        wakingTime = -1;
        snoozeTime = -1;
        break;
    }
}
//- -----------------------------------------loop----------------------------------
void loop()
{
    calcCT();
    //Alarm - Aktivierung
    //checkAlarm();

    //Alarm - ausfuehrung
    //doAlarm();

    if (wakingTime < 0 && snoozeTime < 0)
    {
        if (setupAuswahlActiv)
        {
            setupAuswahl();
        }
        else
        {
            handleInput();
        }
    }
    else
    {
        alarmDeactivate();
    }

    /*if (setupAuswahlActiv) {
        setupAuswahl();
    }else {
        handleInput();
    }*/

    if (!onlyOneRow)
    {
        zweiteDisplayZeile(0);
        ersteDisplayZeile(0);
    }
    else
    {
        setupZweiteZeile(0);
    }

    //dcfDecode();

    //Alarm - Aktivierungal2
    checkAlarm();

    //Alarm - ausfuehrung
    doAlarm();

    //tone(BUZZER, 300, 500);
}