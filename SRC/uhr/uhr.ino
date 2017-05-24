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
}

struct DateTime
{
    Time tim;
    int date = 23;
    int month = 4;
    int day = 7;
};

struct Alarm
{
    Time tim;
    int day;    //redo alarm 1 : E) 1mal[1] I) immer[2] A) von montag bis freitag[3] W) samstag und sonntag[4]
    int active; //activate or deactivate al 1
};

struct AlarmCur
{
    int m;   //Momentan angezeigte alarm2 minute
    int h;   //Momentan angezeigte alarm2 stunde
    int day; //Momentan angezeigter alarm2 tag
};

DateTime t; // current time
Time to;    // Time offset

Alarm al1;
Alarm al2;

Time napDuration;
int snooze = 5; // Snooze Minutes

int wakingTime = -1; //Vergangene Sekunden seit Waeckstart
int napTime = -1;    //Vergangene Sekunden seit Mittagsschlafstart
int snoozeTime = -1; //Vergangene Sekunden seit snoozeStart

int curTime = 0; //Momentan angezeigte Nummer
int curtTA = 0;  //Momentan angezeigte zeit bis alarm
int curddmm = 0; //Momentan angezeigter tag und monat
int curday = 0;  //Momentan angezeigter tag

AlarmCur curAl1;
AlarmCur curAl2;

int setupAuswahlActiv = 0; //Ob in Hauptmenue

int onlyOneRow = 0; //Nur die erste Zeile der Displays wird gezeigt

int dayExchange = 0; //

char wochentagExchange[2];

char wochentage[7][3] = {"Mo", "Di", "Mi", "Do", "Fr", "Sa", "So"};

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
    //Serial.begin(9600);

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

    al1.m = 30;
    al1.h = 6;
    al1.day = 3;
    al1.active = 0;

    al2.m = 30;
    al2.h = 6;
    al2.day = 3;
    al2.active = 0;

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
    /*int tTAtmpMin = tims / 60 + to.m;
    min = tTAtmpMin % 60;
    hour = (tTAtmpMin / 60 + to.h) % 24;

    return (tTAmin + tTAhour * 100);

    (hour * 60 + min) - (al1.h * 60 + al1.m)*/
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
            al1.h++;
            break;
        case 6:
            al1.h--;
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
            al1.m++;
            break;
        case 6:
            al1.m--;
            break;
        }
        break;
    case 4:
        switch (getkey())
        {
        case 1:
            setupAuswahlActiv = 0;
            break;
        case 5:
            if (al1.day < 4)
            {
                al1.day++;
            }
            break;
        case 6:
            if (al1.day > 1)
            {
                al1.day--;
            }
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
            al2.h++;
            break;
        case 6:
            al2.h--;
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
            al2.m++;
            break;
        case 6:
            al2.m--;
            break;
        }
        break;
    case 7:
        switch (getkey())
        {
        case 2:
            setupAuswahlActiv = 0;
            break;
        case 5:
            if (al2.day < 4)
            {
                al2.day++;
            }
            break;
        case 6:
            if (al2.day > 1)
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
            napDuration_h++;
            break;
        case 6:
            napDuration_h--;
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
            napDuration_m++;
            break;
        case 6:
            napDuration_m--;
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

//--------------------------------------------wochentageReRun-----------------------------
char wochentageReRun(int day)
{
    switch (day)
    {
    case 0:
        return '0';
        break;
    case 1:
        return 'E';
        break;
    case 2:
        return 'I';
        break;
    case 3:
        return 'A';
        break;
    case 4:
        return 'W';
        break;
    }
}
//-------------------------------------------2Displayzeile----------------------------
void zweiteDisplayZeile(int updateAllDisplay)
{
    int cal_day1;
    cal_day1 = al1.day;
    if (al1.day != curAl1.day || updateAllDisplay)
    {

        lcd.setCursor(0, 1); //widerhohlungsmodus

        lcd.print(wochentageReRun(al1.day));

        curAl1.day = cal_day1;
    }

    int cal_h1;
    int cal_m1;
    cal_h1 = al1.h;
    cal_m1 = al1.m;
    if (cal_m1 != curAl1.m || cal_h1 != curAl1.h || updateAllDisplay)
    {
        writeTime(al1.h, al1.m, 1, 1);
        curAl1.h = cal_h1;
        curAl1.m = cal_m1;
    }

    int cal_day2;
    cal_day2 = al2.day;
    if (cal_day2 != curAl2.day || updateAllDisplay)
    {
        lcd.setCursor(7, 1); //widerhohlungsmodus

        lcd.print(wochentageReRun(al2.day));

        curAl2.day = cal_day2;
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
        writeTime(hour, min, 0, 0);
        curTime = cTime;
    }
    //zeit bis alarm
    int ctTA = timeTilAlarm();
    if (ctTA != curtTA || updateAllDisplay)
    {
        writeTime(tTAhour, tTAmin, 6, 0);
        curtTA = ctTA;
    }
    int cddmm = date + month * 100;
    if (cddmm != curddmm || updateAllDisplay)
    {
        fulPlott(2, date, 12, 0);
        fulPlott(2, month, 14, 0);
        curddmm = cddmm;
    }
}
//--------------------------------------------input umgang-------------------------
void handleInput()
{
    switch (getkey())
    {
    case 1:

        //Alarm 1 scharfstellen / entschaerfen
        break;
    case 2:
        //Alarm 1 scharfstellen / entschaerfen
        break;
    case 3:
        //Sznooze des alarm
        break;
    case 4:
        //Nap Start / End
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
        writeTime(al1.h, al1.m, 11, 1);
        break;
    case 3:
        lcd.setCursor(0, 1);
        lcd.print("A 1 Minute");
        writeTime(al1.h, al1.m, 11, 1);
        break;
    case 4:
        int cal_day1;
        cal_day1 = al1.day;
        if (cal_day1 != curAl1.day || updateAllDisplay)
        {
            lcd.clear();
            ersteDisplayZeile(1);
            lcd.setCursor(0, 1);
            lcd.print("A 1 reRun");
            lcd.setCursor(15, 1); //widerhohlungsmodus

            lcd.print(wochentageReRun(al1.day));

            curAl1.day = cal_day1;
        }
        break;
    case 5:
        lcd.setCursor(0, 1);
        lcd.print("A 2 Stunde");
        writeTime(al2.h, al2.m, 11, 1);
        break;
    case 6:
        lcd.setCursor(0, 1);
        lcd.print("A 2 Minute");
        writeTime(al2.h, al2.m, 11, 1);
        break;
    case 7:
        int cal_day2;
        cal_day2 = al2.day;
        if (cal_day2 != curAl2.day || updateAllDisplay)
        {
            lcd.clear();
            ersteDisplayZeile(1);
            lcd.setCursor(0, 1);
            lcd.print("A 2 reRun");
            lcd.setCursor(15, 1); //widerhohlungsmodus

            lcd.print(wochentageReRun(al2.day));

            curAl2.day = cal_day2;
        }
        break;
    case 8:
        lcd.setCursor(0, 1);
        lcd.print("Mittag H");
        writeTime(napDuration_h, napDuration_m, 11, 1);
        break;
    case 9:
        lcd.setCursor(0, 1);
        lcd.print("Mittag M");
        writeTime(napDuration_h, napDuration_m, 11, 1);
        break;
    }
}
//-------------------------------Alarm - ausfuehrung--------------------------------
void doAlarm()
{
    if (wakingTime >= 0)
        tone(BUZZER, 300, 5);
}
//-----------------------------Alarm - Aktivierung-------------------------------------
void checkAlarm()
{
    if (napDuration_h * 60 + napDuration_m == napTime)
    {
        wakingTime = 0;
    }
    else if (((min == al1.m && hour == al1.h) || (min == al2.m && hour == al2.h)) && tims % 60 == 0)
    {
        wakingTime = 0;
    }
    else if (snoozeTime == snooze_m * 60)
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
        if (snoozeTime != -1)
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

    //Alarm - Aktivierung
    checkAlarm();

    //Alarm - ausfuehrung
    doAlarm();

    //tone(BUZZER, 300, 500);
}
