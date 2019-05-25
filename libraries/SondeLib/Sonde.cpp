#include <U8x8lib.h>
#include <U8g2lib.h>

#include "Sonde.h"
#include "RS41.h"
#include "DFM.h"
#include "SX1278FSK.h"
#include "Display.h"

extern U8X8_SSD1306_128X64_NONAME_SW_I2C *u8x8;
extern SX1278FSK sx1278;

//SondeInfo si = { STYPE_RS41, 403.450, "P1234567", true, 48.1234, 14.9876, 543, 3.97, -0.5, true, 120 };
const char *evstring[]={"NONE", "KEY1S", "KEY1D", "KEY1M", "KEY1L", "KEY2S", "KEY2D", "KEY2M", "KEY2L",
                               "VIEWTO", "RXTO", "NORXTO", "(max)"};

Sonde::Sonde() {
	config.button_pin = 0;
	config.button2_pin = T4 + 128;     // T4 == GPIO13, should be ok for v1 and v2
	config.touch_thresh = 60;
	config.led_pout = 9;	
	// Try autodetecting board type
  	// Seems like on startup, GPIO4 is 1 on v1 boards, 0 on v2.1 boards?
   	int autodetect = gpio_get_level((gpio_num_t)4); 
	if(autodetect==1) {
		config.oled_sda = 4;
		config.oled_scl = 15;
	} else {
		config.oled_sda = 21;
		config.oled_scl = 22;
	}
	//
	config.oled_rst = 16;
	config.noisefloor = -125;
	strcpy(config.call,"NOCALL");
	strcpy(config.passcode, "---");
	config.maxsonde=15;
	config.debug=0;
	config.wifi=1;
	config.wifiap=1;
	config.display=1;
	config.startfreq=400;
	config.channelbw=10;
	config.spectrum=10;
	config.timer=0;
	config.marker=0;
	config.showafc=0;
	config.freqofs=0;
	config.rs41.agcbw=25000;
	config.rs41.rxbw=12000;
	config.udpfeed.active = 1;
	config.udpfeed.type = 0;
	strcpy(config.udpfeed.host, "192.168.42.20");
	strcpy(config.udpfeed.symbol, "/O");
	config.udpfeed.port = 9002;
	config.udpfeed.highrate = 1;
	config.udpfeed.idformat = ID_DFMGRAW;
	config.tcpfeed.active = 0;
	config.tcpfeed.type = 1;
	strcpy(config.tcpfeed.host, "radiosondy.info");
	strcpy(config.tcpfeed.symbol, "/O");
	config.tcpfeed.port = 12345;
	config.tcpfeed.highrate = 10;
	config.tcpfeed.idformat = ID_DFMDXL;
}

void Sonde::setConfig(const char *cfg) {
	while(*cfg==' '||*cfg=='\t') cfg++;
	if(*cfg=='#') return;
	char *s = strchr(cfg,'=');
	if(!s) return;
	char *val = s+1;
	*s=0; s--;
	while(s>cfg && (*s==' '||*s=='\t')) { *s=0; s--; }
	Serial.printf("configuration option '%s'=%s \n", cfg, val);
	if(strcmp(cfg,"noisefloor")==0) {
		config.noisefloor = atoi(val);
		if(config.noisefloor==0) config.noisefloor=-130;
	} else if(strcmp(cfg,"call")==0) {
		strncpy(config.call, val, 9);
	} else if(strcmp(cfg,"passcode")==0) {
		strncpy(config.passcode, val, 9);
	} else if(strcmp(cfg,"button_pin")==0) {
		config.button_pin = atoi(val);
	} else if(strcmp(cfg,"button2_pin")==0) {
		config.button2_pin = atoi(val);
	} else if(strcmp(cfg,"touch_thresh")==0) {
		config.touch_thresh = atoi(val);
	} else if(strcmp(cfg,"led_pout")==0) {
		config.led_pout = atoi(val);		
	} else if(strcmp(cfg,"oled_sda")==0) {
		config.oled_sda = atoi(val);
	} else if(strcmp(cfg,"oled_scl")==0) {
		config.oled_scl = atoi(val);
	} else if(strcmp(cfg,"oled_rst")==0) {
		config.oled_rst = atoi(val);
	} else if(strcmp(cfg,"maxsonde")==0) {
		config.maxsonde = atoi(val);
		if(config.maxsonde>MAXSONDE) config.maxsonde=MAXSONDE;
	} else if(strcmp(cfg,"debug")==0) {
		config.debug = atoi(val);
	} else if(strcmp(cfg,"wifi")==0) {
		config.wifi = atoi(val);			
	} else if(strcmp(cfg,"wifiap")==0) {
		config.wifiap = atoi(val);
	} else if(strcmp(cfg,"display")==0) {
		config.display = atoi(val);
		disp.setLayout(config.display);
	} else if(strcmp(cfg,"startfreq")==0) {
		config.startfreq = atoi(val);
	} else if(strcmp(cfg,"channelbw")==0) {
		config.channelbw = atoi(val);	
	} else if(strcmp(cfg,"spectrum")==0) {
		config.spectrum = atoi(val);
	} else if(strcmp(cfg,"timer")==0) {
		config.timer = atoi(val);
	} else if(strcmp(cfg,"marker")==0) {
		config.marker = atoi(val);					
	} else if(strcmp(cfg,"showafc")==0) {
		config.showafc = atoi(val);
	} else if(strcmp(cfg,"freqofs")==0) {
		config.freqofs = atoi(val);
	} else if(strcmp(cfg,"rs41.agcbw")==0) {
		config.rs41.agcbw = atoi(val);
	} else if(strcmp(cfg,"rs41.rxbw")==0) {
		config.rs41.rxbw = atoi(val);
	} else if(strcmp(cfg,"axudp.active")==0) {
		config.udpfeed.active = atoi(val)>0;
	} else if(strcmp(cfg,"axudp.host")==0) {
		strncpy(config.udpfeed.host, val, 63);
	} else if(strcmp(cfg,"axudp.port")==0) {
		config.udpfeed.port = atoi(val);
	} else if(strcmp(cfg,"axudp.symbol")==0) {
		strncpy(config.udpfeed.symbol, val, 3);
	} else if(strcmp(cfg,"axudp.highrate")==0) {
		config.udpfeed.highrate = atoi(val);
	} else if(strcmp(cfg,"axudp.idformat")==0) {
		config.udpfeed.idformat = atoi(val);
	} else if(strcmp(cfg,"tcp.active")==0) {
		config.tcpfeed.active = atoi(val)>0;
	} else if(strcmp(cfg,"tcp.host")==0) {
		strncpy(config.tcpfeed.host, val, 63);
	} else if(strcmp(cfg,"tcp.port")==0) {
		config.tcpfeed.port = atoi(val);
	} else if(strcmp(cfg,"tcp.symbol")==0) {
		strncpy(config.tcpfeed.symbol, val, 3);
	} else if(strcmp(cfg,"tcp.highrate")==0) {
		config.tcpfeed.highrate = atoi(val);
	} else if(strcmp(cfg,"tcp.idformat")==0) {
		config.tcpfeed.idformat = atoi(val);
	} else {
		Serial.printf("Invalid config option '%s'=%s \n", cfg, val);
	}
}

void Sonde::clearIP() {
	disp.clearIP();
}

void Sonde::setIP(const char *ip, bool AP) {
	disp.setIP(ip, AP);
}

void Sonde::clearSonde() {
	nSonde = 0;
}
void Sonde::addSonde(float frequency, SondeType type, int active, char *launchsite)  {
	if(nSonde>=config.maxsonde) {
		Serial.println("Cannot add another sonde, MAXSONDE reached");
		return;
	}
	sondeList[nSonde].type = type;
	sondeList[nSonde].freq = frequency;
	sondeList[nSonde].active = active;
	strncpy(sondeList[nSonde].launchsite, launchsite, 17);	
	memcpy(sondeList[nSonde].rxStat, "\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3", 18); // unknown/undefined
	nSonde++;
}
void Sonde::nextConfig() {
	currentSonde++;
	// Skip non-active entries (but don't loop forever if there are no active ones)
	for(int i=0; i<config.maxsonde; i++) {
		if(!sondeList[currentSonde].active) {
			currentSonde++;
			if(currentSonde>=nSonde) currentSonde=0;
		}
	}
	if(currentSonde>=nSonde) {
		currentSonde=0;
	}
	setup();
}
SondeInfo *Sonde::si() {
	return &sondeList[currentSonde];
}

void Sonde::setup() {
	// Test only: setIP("123.456.789.012");
	sondeList[currentSonde].lastState = -1;
	sondeList[currentSonde].viewStart = millis();
	// update receiver config
	Serial.print("\nSonde::setup() on sonde index ");
	Serial.println(currentSonde);
	switch(sondeList[currentSonde].type) {
	case STYPE_RS41:
		rs41.setup();
		rs41.setFrequency(sondeList[currentSonde].freq * 1000000);
		break;
	case STYPE_DFM06:
	case STYPE_DFM09:
		dfm.setup( sondeList[currentSonde].type==STYPE_DFM06?0:1 );
		dfm.setFrequency(sondeList[currentSonde].freq * 1000000);
		break;
	}
	// debug
	float afcbw = sx1278.getAFCBandwidth();
	float rxbw = sx1278.getRxBandwidth();
	Serial.printf("AFC BW: %f  RX BW: %f\n", afcbw, rxbw);

}
int Sonde::receiveFrame() {
	int ret;
	if(sondeList[currentSonde].type == STYPE_RS41) {
		ret = rs41.receiveFrame();
	} else {
		ret = dfm.receiveFrame();
	}
	memmove(sonde.si()->rxStat+1, sonde.si()->rxStat, 17);
	sonde.si()->rxStat[0] = ret;
	if(ret==0) {
		if(sonde.si()->lastState != 1) {
			sonde.si()->rxStart = millis();
			sonde.si()->lastState = 1;
		}
	} else {
		if(sonde.si()->lastState != 0) {
			sonde.si()->norxStart = millis();
			sonde.si()->lastState = 0;
		}
	}
	return ret;  // 0: OK, 1: Timeuot, 2: Other error, 3: unknown
}

uint8_t Sonde::timeoutEvent() {
	uint32_t now = millis();
#if 0
	Serial.printf("Timeout check: %ld - %ld vs %ld; %ld - %ld vs %ld; %ld - %ld vs %ld\n",
		now, sonde.si()->viewStart, disp.layout->timeouts[0],
		now, sonde.si()->rxStart, disp.layout->timeouts[1],
		now, sonde.si()->norxStart, disp.layout->timeouts[2]);
#endif
	Serial.printf("lastState is %d\n", sonde.si()->lastState);
	if(disp.layout->timeouts[0]>=0 && now - sonde.si()->viewStart >= disp.layout->timeouts[0]) {
		Serial.println("View timer expired");
		return EVT_VIEWTO;
	}
	if(sonde.si()->lastState==1 && disp.layout->timeouts[1]>=0 && now - sonde.si()->rxStart >= disp.layout->timeouts[1]) {
		Serial.println("RX timer expired");
		return EVT_RXTO;
	}
	if(sonde.si()->lastState==0 && disp.layout->timeouts[2]>=0 && now - sonde.si()->norxStart >= disp.layout->timeouts[2]) {
		Serial.println("NORX timer expired");
		return EVT_NORXTO;
	}
	return 0;
}

int Sonde::updateState(int8_t event) {
	Serial.printf("Sonde::updateState for event %d\n", event);
	if(event==ACT_NONE) return -1;
	if(event==ACT_NEXTSONDE) {
		Serial.printf("advancing to next sonde\n");
		sonde.nextConfig();
		return -1;
	}
	if (event==ACT_PREVSONDE) {
		// TODO
		Serial.printf("previous not supported, advancing to next sonde\n");
		sonde.nextConfig();
		return -1;
	}
	if (event==ACT_DISPLAY_SPECTRUM || event==ACT_DISPLAY_WIFI) {
		return event;
	}
	int n = event;
	if(event==ACT_DISPLAY_DEFAULT) {
		n = config.display;
	}
	if(n>=0&&n<4) {
		disp.setLayout(n);
		clearDisplay();
		updateDisplay();
	}		
	// TODO: change display to n
	return -1;
}

void Sonde::updateDisplayPos() {
	disp.updateDisplayPos();
}

void Sonde::updateDisplayPos2() {
	disp.updateDisplayPos2();
}

void Sonde::updateDisplayID() {
	disp.updateDisplayID();
}
	
void Sonde::updateDisplayRSSI() {
	disp.updateDisplayRSSI();
}

void Sonde::updateStat() {
	disp.updateStat();
}

void Sonde::updateDisplayRXConfig() {
	disp.updateDisplayRXConfig();
}

void Sonde::updateDisplayIP() {
	disp.updateDisplayIP();
}

// Probing RS41
// 40x.xxx MHz
void Sonde::updateDisplayScanner() {
	disp.setLayout(0);
	disp.updateDisplay();
	disp.setLayout(config.display);
#if 0
	char buf[16];
	u8x8->setFont(u8x8_font_7x14_1x2_r);
	u8x8->drawString(0, 0, "Scan:");
	u8x8->drawString(8, 0,  sondeTypeStr[si()->type]);
	snprintf(buf, 16, "%3.3f MHz", si()->freq);
	u8x8->drawString(0,3, buf);
	snprintf(buf, 16, "%s", si()->launchsite);
	u8x8->drawString(0,5, buf);	
	updateDisplayIP();
#endif
}

void Sonde::updateDisplay()
{
	disp.updateDisplay();
}

void Sonde::clearDisplay() {
	u8x8->clearDisplay();
}

Sonde sonde = Sonde();
