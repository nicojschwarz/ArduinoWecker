//LED 4X7SEG TEST 050515
#include <TimerOne.h>
#include <LiquidCrystal.h>

#define DCF 2
#define B_ALARM1 18 //... das Relais anzustellen
#define B_ALARM2 19 //... den Alarm (Buzzer / Relais) an- / auszustellen
#define B_SNOZ 20   //... Taster der die funktion hat ... den Alarm auszuschalten
#define B_NAP 21    // mittagsschlaf
#define B_A 22 //a (-)
#define B_B 23 //b (+)

#define BUZZER 10 //Buzzer

LiquidCrystal lcd(12, 11, 6, 5, 4, 3); // initialize the library with the numbers of the interface pins

int min = 0;  // Current Time (Minutes)
int hour = 0; // Current Time (Hours)
int date = 23;
int month = 4;
int day = 7;

int tTAmin = 5;
int tTAhour = 91;

int to_m = 30; // Time offset Min
int to_h = 12; // Time offset Hour

int al_m1 = 30;  // Alarm1 time Min
int al_h1 = 6;   // Alarm1 time Hour
int al_day1 = 3; //redo alarm 1 : E) 1mal[1] I) immer[2] A) von montag bis freitag[3] W) samstag und sonntag[4]
int al_m2 = 10;  // Alarm2 time Min
int al_h2 = 8;   // Alarm2 time Hour
int al_day2 = 4; //redo alarm 2 : E) 1mal[1] I) immer[2] A) von montag bis freitag[3] W) samstag und sonntag[4]

int snooze_m = 5; // Snooze Minutes

int napDuration_m = 45;
int napDuration_h = 2;

int wakingTime = -1; //Vergangene Sekunden seit Waeckstart
int napTime = -1;    //Vergangene Sekunden seit Mittagsschlafstart
int snoozeTime = -1; //Vergangene Sekunden seit snoozeStart

int curText = 0;    //Momentan angezeigte Punkte
int curTime = 0;    //Momentan angezeigte Nummer
int curtTA = 0;     //Momentan angezeigte zeit bis alarm
int curddmm = 0;    //Momentan angezeigter tag und monat
int cural_m1 = 0;   //Momentan angezeigte alarm1 minute
int cural_h1 = 0;   //Momentan angezeigte alarm1 stunde
int cural_day1 = 0; //Momentan angezeigter alarm1 tag
int cural_m2 = 0;   //Momentan angezeigte alarm2 minute
int cural_h2 = 0;   //Momentan angezeigte alarm2 stunde
int cural_day2 = 0; //Momentan angezeigter alarm2 tag
int curday = 0;     //Momentan angezeigter tag

int setupAuswahlActiv = 0;  //Ob in Hauptmenue

int onlyOneRow = 0;   //Nur die erste Zeile der Displays wird gezeigt

int dayExchange = 0;    //

char wochentagExchange [2];

struct {				//
	unsigned int yy;	//received date
	unsigned int mo;
	unsigned int dd;
	unsigned int vld;	//date is valid
	unsigned int hh;	//received time
	unsigned int mm;
	unsigned int pmh;	//previus hh*60+mm
	unsigned int vlt;	//valid time
	//recieve data
	unsigned char fin;	//buffer finalized for decode
	unsigned char cnf;	//finalized count for debug
	unsigned int cnt;	//bit count
	unsigned int le;	//leading edge time
	unsigned int tim;	//time since prevoius leading edge
	char actv;			//dcf activ
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

void dcf_rx(void)		//DCF77 interupt handler
{
    Serial.println("dcf_rx");
	int tim;
	byte val = digitalRead(DCF);
	dcf.actv=1;
	// dcf.tim inc 1ms done by gptime
	// detected pulse is too short: incorrect pulse: reject
	if((dcf.tim-dcf.le)<60) return;

	// flank is detected quickly after previous flank up: reject
	if(dcf.tim<700) return;

//	if(val)						// Flank down invertiert
	if(!val)					// Flank down
	{
		if (!dcf.le) dcf.le=dcf.tim;
    Serial.println("0");
//digitalWrite(DCFA, 0);
		return;
	} 

	if(dcf.le) 					// Flank up
	{
    Serial.println("1");
//digitalWrite(DCFA, 1);
		tim=dcf.tim-dcf.le;		//trailing edge: low/high bit time
		dcf.tim=tim;			//set time = last leading edge
		if((dcf.le) > 1600)		//start detected
			{
			if(dcf.cnt==59) {dcf.fin=1; dcf.cnf++;}
			dcf.cnt=0;
			}
		dcf.cnt++;				//save value
		if(dcf.cnt<60) dcf.bit[dcf.cnt]=tim;
		dcf.le=0;				//reset leading edge time
		return;
	
	}  
	return;
}
//----------------------------------------------------------------------dcf_getv-------
unsigned int dcf_getv(int i0,int i9, int th)	//get dcf-decoded value
{
    Serial.println("dcf_getv");
	//th bit schwellwert >th bit=1
	unsigned int v,c,i;

	v=0;
	c=1;		//c=1,2,4,8,10,20,40,80;
	for(i=i0;i<=i9;i++)
		{
		if(dcf.bit[i]>th) v=v+c;
        c=c*2; if(c==16) c=10;
		}
	return(v);
}
//------------------------------------------------------------------------dcf_getc------
unsigned int dcf_getc(int i0,int i9, int th)	//get dcf-parity-valid
{
    Serial.println("dcf_getc");
	//th bit schwellwert >th bit=1
	unsigned int i,v;
	boolean p,c;

	p=0; c=0;
	if(dcf.bit[i9+1]>th) c=1;
	for(i=i0;i<=i9;i++)
		{
		if(dcf.bit[i]>th) p=~p;		//toggle parity
        }
	if(p && c) return(1);			//ok
	if((!p) && (!c)) return(1);		//ok
	return(0);
}

//-----------------------dcfDecode----------------------------------------------------
unsigned int dcfDecode()
{
    Serial.println("dcfDecode");
	unsigned int th,mh;

	//splittime: low max 120ms, high min 180ms
	//th=112;			//ELV-9461059
	th=130;				//CONRAD-HK56-BS/0
	dcf.mm=dcf_getv(22,28,th);
	dcf.hh=dcf_getv(30,35,th);
	dcf.vlt=dcf_getc(22,28,th);				//parity mm
	if(dcf.vlt) dcf.vlt=dcf_getc(30,35,th);	//parity hh
	if(dcf.vlt) 							//previous+1 = current
		{mh=(dcf.hh*60)+dcf.mm;
		if(((dcf.pmh+1)%1440) != mh) dcf.vlt=0;
		dcf.pmh=mh;
		}
	dcf.dd=dcf_getv(37,42,th);
	dcf.mo=dcf_getv(46,50,th);
	dcf.yy=dcf_getv(51,58,th);
	dcf.vld=dcf_getc(37,58,th);
	dcf.fin=0;		//decoded

	return(1);
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

	dcf.yy=0;
	dcf.mo=0;
	dcf.dd=0;
	dcf.hh=0;
	dcf.mm=0;
	dcf.pmh=0;
	dcf.vld=0;
	dcf.vlt=0;
	dcf.cnt=0;
	dcf.cnf=0;
	dcf.fin=0;
	dcf.le=0;
	dcf.tim=0;
	dcf.actv=0;

	pinMode(DCF,INPUT);					//input dcf-signal
//	attachInterrupt(3, dcf_rx, CHANGE);	//DCF enable ir3
	attachInterrupt(1, dcf_rx, CHANGE);	//DCF enable ir1
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
    int tmpMin = tims / 60 + to_m;
    min = tmpMin % 60;
    hour = (tmpMin / 60 + dcf.hh/*to_h*/) % 24;
    return (min + hour * 100);
}
//---------------------------------nummer mit 0 am anfang auf lcd---------------------
void fulPlott (int i, int time, int x, int y)
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
void writeTime (int time1, int time2, int x, int y) {
    fulPlott(2, time1, x, y);
    lcd.setCursor(x+2, y);
    lcd.print(":");
    fulPlott(2, time2, x+3, y);

}
//------------------------------------------timeTilAlarm------------------------------
int timeTilAlarm()
{

    int tTAtmpMin = tims / 60 + to_m;
    min = tTAtmpMin % 60;
    hour = (tTAtmpMin / 60 + to_h) % 24;
    return (tTAmin + tTAhour * 100);
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
                case 5: case 6:
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
                    al_h1++;
                    break;
                case 6:
                    al_h1--;
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
                        al_m1++;
                        break;
                    case 6:
                        al_m1--;
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
                    if (al_day1 < 4) {
                        al_day1++;
                    }
                    break;
                case 6:
                    if (al_day1 > 1) {
                        al_day1--;
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
                        al_h2++;
                        break;
                    case 6:
                        al_h2--;
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
                        al_m2++;
                        break;
                    case 6:
                        al_m2--;
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
                        if (al_day2 < 4) {
                            al_day2++;
                        }
                        break;
                    case 6:
                        if (al_day2 > 1) {
                            al_day2--;
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
    if (!setupAuswahlActiv) {
        lcd.clear();
        onlyOneRow = 0;
        ersteDisplayZeile(1);
        zweiteDisplayZeile(1);
    }
}

//--------------------------------------------wochentageReRun-----------------------------
char wochentageReRun(int day) {
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
//------------------------------------------wochentage-------------------------------------
char wochentage(int day, int durchgang) {
    if (durchgang) {
        switch (day) {
            case 1:
                return 'M';
            case 2:
                return 'D';
            case 3:
                return 'M';
            case 4:
                return 'D';
            case 5:
                return 'F';
            case 6:
                return 'S';
            case 7:
                return 'S';
        }
    }else {
        switch (day) {
                case 1:
                    return 'o';
                case 2:
                    return 'i';
                case 3:
                    return 'i';
                case 4:
                    return 'o';
                case 5:
                    return 'r';
                case 6:
                    return 'a';
                case 7:
                    return 'o';
        }
    }
}
//-------------------------------------------2Displayzeile----------------------------
void zweiteDisplayZeile(int updateAllDisplay)
{
    int cal_day1;
    cal_day1 = al_day1;
    if (al_day1 != cural_day1 || updateAllDisplay)
    {

        lcd.setCursor(0, 1); //widerhohlungsmodus

        lcd.print(wochentageReRun(al_day1));

        cural_day1 = cal_day1;
    }

    int cal_h1;
    int cal_m1;
    cal_h1 = al_h1;
    cal_m1 = al_m1;
    if (cal_m1 != cural_m1 || cal_h1 != cural_h1 || updateAllDisplay)
    {
        writeTime(al_h1, al_m1, 1, 1);
        cural_h1 = cal_h1;
        cural_m1 = cal_m1;
    }

    int cal_day2;
    cal_day2 = al_day2;
    if (cal_day2 != cural_day2 || updateAllDisplay)
    {
        lcd.setCursor(7, 1); //widerhohlungsmodus

        lcd.print(wochentageReRun(al_day2));

        cural_day2 = cal_day2;
    }

    int cal_h2;
    int cal_m2;
    cal_h2 = al_h2;
    cal_m2 = al_m2;
    if (cal_h2 != cural_h2 || cal_m2 != cural_m2 || updateAllDisplay)
    {
        writeTime(al_h2, al_m2, 8, 1);
        cural_h2 = cal_h2;
        cural_m2 = cal_m2;
    }

    int cday;
    cday = day;
    if (cday != curday || updateAllDisplay)
    {
        lcd.setCursor(14, 1);
        lcd.print(wochentage(day,1));
        lcd.setCursor(15, 1);
        lcd.print(wochentage(day,0));
        curday = cday;
    }
}
//-------------------1Displayzeile---------------------------------------------------

void ersteDisplayZeile(int updateAllDisplay){

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
void setupZweiteZeile(int updateAllDisplay){
    if (!onlyOneRow) {
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
        writeTime(al_h1, al_m1, 11, 1);
        break;
    case 3:
        lcd.setCursor(0, 1);
        lcd.print("A 1 Minute");
        writeTime(al_h1, al_m1, 11, 1);
        break;
    case 4:
        int cal_day1;
        cal_day1 = al_day1;
        if (cal_day1 != cural_day1 || updateAllDisplay)
        {
            lcd.clear();
            ersteDisplayZeile(1);
            lcd.setCursor(0, 1);
            lcd.print("A 1 reRun");
            lcd.setCursor(15, 1); //widerhohlungsmodus

            lcd.print(wochentageReRun(al_day1));

            cural_day1 = cal_day1;
        }
        break;
    case 5:
        lcd.setCursor(0, 1);
        lcd.print("A 2 Stunde");
        writeTime(al_h2, al_m2, 11, 1);
        break;
    case 6:
        lcd.setCursor(0, 1);
        lcd.print("A 2 Minute");
        writeTime(al_h2, al_m2, 11, 1);
        break;
    case 7:
        int cal_day2;
        cal_day2 = al_day2;
        if (cal_day2 != cural_day2 || updateAllDisplay)
        {
            lcd.clear();
            ersteDisplayZeile(1);
            lcd.setCursor(0, 1);
            lcd.print("A 2 reRun");
            lcd.setCursor(15, 1); //widerhohlungsmodus

            lcd.print(wochentageReRun(al_day2));

            cural_day2 = cal_day2;
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
    if (wakingTime >= 0) tone(BUZZER, 300, 500);
}
//-----------------------------Alarm - Aktivierung-------------------------------------
void checkAlarm()
{
    if (napDuration_h * 60 + napDuration_m == napTime) {
        wakingTime = 0;
    }else if (((min == al_m1 && hour == al_h1) || (min == al_m2 && hour == al_h2 ))&& tims % 60 == 0) {
        wakingTime = 0;
    }else if (snoozeTime == snooze_m * 60) {
        wakingTime = 0;
    }
}
//-------------------------alarmDeactivate-----------------------------------------
void alarmDeactivate() {
    switch (getkey()) {
        case 1:
            wakingTime = -1;
            if (snoozeTime != -1) snoozeTime = 0;
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

    //Alarm - Aktivierung
    //checkAlarm();

    //Alarm - ausfuehrung
    //doAlarm();

    if (wakingTime < 0 ) {
        if (setupAuswahlActiv) {
            setupAuswahl();
        }else {
            handleInput();
        }
    }else {
        alarmDeactivate();
    }

    /*if (setupAuswahlActiv) {
        setupAuswahl();
    }else {
        handleInput();
    }*/

    if (!onlyOneRow) {
        zweiteDisplayZeile(0);
        ersteDisplayZeile(0);
    }else {
        setupZweiteZeile(0);
    }

    //dcfDecode();

    //Alarm - Aktivierung
    checkAlarm();

    //Alarm - ausfuehrung
    doAlarm();

    //tone(BUZZER, 300, 500);

    
}
