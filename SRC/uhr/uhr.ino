//LED 4X7SEG TEST 050515
#include <TimerOne.h>
#include <LiquidCrystal.h>

//#define B_DCF 3
#define B_RELAY 19 //... das Relais anzustellen
#define B_ALARM 18 //... den Alarm (Buzzer / Relais) an- / auszustellen
#define B_SET 17   //... den Modus fuer die Eintellung der Zeit oder der Alarmzeit einzustellen
#define B_MIN 16   //... die Minuten einzustellen (Zeit / Alarmzeit)
#define B_HOUR 15  //... die Stunden einzustellen (Zeit / Alarmzeit)
#define B_SNOZ 14  //... Taster der die funktion hat ... den Alarm auszuschalten
#define B_NAP 13

#define BUZZER 10 //Buzzer
#define RELAY 9   //Relais

LiquidCrystal lcd(12, 11, 5, 4, 3, 2); // initialize the library with the numbers of the interface pins

int min = 0;  // Current Time (Minutes)
int hour = 0; // Current Time (Hours)

int to_m = 0;     // Time offset Min
int to_h = 0;     // Time offset Hour
int al_m = 0;     // Alarm time Min
int al_h = 6;     // Alarm time Hour
int snooze_s = 0; // Snooze Seconds
int snooze_m = 5; // Snooze Minutes
int napDuration_m = 45;
int napDuration_h = 2;

int alarmMode = 0;   //weckart 0: aus, 1: snooze, 2: Buzzer, 3: Relais
int relayActive = 0; //dauerhafte aktivierung des Relais
int setMode = 0;     //Setting mode

int wakingTime = -1; //Vergangene Sekunden seit Waeckstart
int napTime = -1;    //Vergangene Sekunden seit Mittagsschlafstart

int curText = 0; //Momentan angezeigte Punkte
int curNum = 0;  //Momentan angezeigte Nummer

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

//---gp-timer--------------------------------------------
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
    }

    return;
}
//-----------------------------------------------------------

void setup() // The setup routine runs once, when you press Reset:
{
    Serial.begin(9600);

    lcd.begin(16, 2);

    delay(5000);

    pinMode(BUZZER, OUTPUT);
    pinMode(RELAY, OUTPUT);

    pinMode(B_SNOZ, INPUT);
    pinMode(B_RELAY, INPUT_PULLUP);
    pinMode(B_ALARM, INPUT_PULLUP);
    pinMode(B_SET, INPUT_PULLUP);
    pinMode(B_MIN, INPUT_PULLUP);
    pinMode(B_HOUR, INPUT_PULLUP);
    pinMode(B_NAP, INPUT_PULLUP);
    //pinMode(B_DCF, INPUT_PULLUP);

    digitalWrite(BUZZER, 0);
    digitalWrite(RELAY, 0);

    Timer1.initialize(1000);       // set a timer i us (1000us=1ms)
    Timer1.attachInterrupt(gptim); // attach service
}

//---keys------------------------------------------------
unsigned char getkey(void) //get a key
{
    unsigned char ky = 0;

    if (digitalRead(B_ALARM))
        ky = 1; //alarm
    else if (digitalRead(B_SET))
        ky = 2; //set
    else if (digitalRead(B_MIN))
        ky = 3; //min
    else if (digitalRead(B_HOUR))
        ky = 4; //hour
    else if (digitalRead(B_RELAY))
        ky = 6; //show
    else if (digitalRead(B_SNOZ))
        ky = 7; //snooze
    else if (digitalRead(B_NAP))
        ky = 8; //nap (Mittagsschlaf)

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

//generate num
int genNum()
{
    if (setMode)
    {
        switch (setMode)
        {
        case 1:
            return (min + hour * 100);
            break;
        case 2:
            return (al_m + al_h * 100);
            break;
        case 3:
            return (snooze_s + snooze_m * 100);
            break;
        case 4:
            return (napDuration_m + napDuration_h * 100);
            break;
        }
    }
    else if (napTime >= 0 && (tims / 5) % 2)
    {
        int leftoverNapTime = (napDuration_h * 60 + napDuration_m) * 60 - napTime;
        if (leftoverNapTime >= 3600)
            leftoverNapTime /= 60;
        return leftoverNapTime % 60 + (leftoverNapTime / 60) * 100;
    }
    return (min + hour * 100);
}

//generate dot mask
int genText()
{
    if (wakingTime >= 0)
    {
        return (0x01 << ((timms / 125) % 4));
    }
    else
    {
        switch (setMode)
        {
        case 0:
            return 0x00;
            break;
        case 1:
            return 0x08;
            break;
        case 2:
            return 0x04;
            break;
        case 3:
            return 0x02;
            break;
        case 4:
            return 0x01;
            break;
        }
    }
    return 0;
}

void writeTime(int time) {
    lcd.setCursor(0, 1);
    for (int i = 0x8; i > 0; i >>= 1)
        lcd.print(time & i);
    curNum = time;
}

void writeText(int text) {
    lcd.setCursor(0, 0);
    lcd.print(text);
    curText = text;
}

void display()
{
    //generate num
    int num = genNum();

    //generate text
    int text = genText();

    //update dirty display
    if (num != curNum)
        writeTime(num);
    
    if(text != curText)
        writeText(text);
}

//INPUT TASTER
void handleInput()
{
    switch (getkey())
    {
    case 1:
        alarmMode = (alarmMode + 1) % 4;
        break;
    case 2:
        setMode = (setMode + 1) % 5;
        break;
    case 3:
        switch (setMode)
        {
        case 1:
            to_m = (to_m + 1) % 60;
            break;
        case 2:
            al_m = (al_m + 1) % 60;
            break;
        case 3:
            snooze_s = (snooze_s + 1) % 60;
            break;
        case 4:
            napDuration_m = (napDuration_m + 1) % 60;
            break;
        }
        break;
    case 4:
        switch (setMode)
        {
        case 1:
            to_h = (to_h + 1) % 60;
            break;
        case 2:
            al_h = (al_h + 1) % 60;
            break;
        case 3:
            snooze_m = (snooze_m + 1) % 60;
            break;
        case 4:
            napDuration_h = (napDuration_h + 1) % 60;
            break;
        }
        break;
    case 5:
        relayActive = -relayActive + 1;
        break;
    case 7:
        wakingTime = -1;
        break;
    case 8:
        if (napTime < 0)
            napTime = 0;
        else
            napTime = -1;
        break;
    }
}

void setRelay(unsigned char state)
{
    if (relayActive || state)
        digitalWrite(RELAY, HIGH);
    else
        digitalWrite(RELAY, LOW);
}

//Alarm - ausfuehrung
void doAlarm()
{
    if (wakingTime < 0)
    {
        setRelay(0);
        digitalWrite(BUZZER, LOW);
    }
    else
    {
        if (wakingTime >= 1800)
            wakingTime = -1;

        switch (alarmMode)
        {
        case 0:
            wakingTime = -1;
            break;
        case 1:
            setRelay(1);
            if (snooze_m * 60 + snooze_s <= wakingTime)
                digitalWrite(BUZZER, HIGH);
            else
                digitalWrite(BUZZER, LOW);
            break;
        case 2:
            digitalWrite(BUZZER, HIGH);
            setRelay(0);
            break;
        case 3:
            setRelay(1);
            digitalWrite(BUZZER, LOW);
            break;
        }
    }
}

//Alarm - Aktivierung
void checkAlarm()
{
    if (min == al_m && hour == al_h && tims % 60 == 0)
    {
        wakingTime = 0;
    }
    else if (napTime >= 0 && napTime / 60 >= napDuration_m + napDuration_h * 60)
    {
        napTime = -1;
        wakingTime = 0;
    }
}

int b = 0;
void loop()
{

    //generate time
    min = ((tims / 60 + to_m) % 60);
    hour = ((tims / 3600 + to_h) % 24); // ((tims / 60) / 60 + to_h) % 24;

    handleInput();

    //Alarm - Aktivierung
    checkAlarm();

    //Alarm - ausfuehrung
    doAlarm();

    display();
}
