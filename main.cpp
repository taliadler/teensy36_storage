#include <Metro.h>
#include <MTP.h>
#include <FlexCAN.h>
#include <TimeLib.h>
#include <SPI.h>
#include <SysCall.h>
#include <Arduino.h>


#define RESTART_ADDR 0xE000ED0C
#define READ_RESTART() (*(volatile uint32_t *)RESTART_ADDR)
#define WRITE_RESTART(val) ((*(volatile uint32_t *)RESTART_ADDR) = (val))


//#include <log_screen.h>
// Try to select the best SD card configuration.
//#if HAS_SDIO_CLASS
#define SD_CONFIG SdioConfig(0)
//#elif ENABLE_DEDICATED_SPI
//#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI)
//#else  // HAS_SDIO_CLASS
//#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI)
//#endif // HAS_SDIO_CLASS

//Timers for running tasks every N milis
Metro sysTimer = Metro(1);
Metro analog_read_timer= Metro(400);
Metro uart1_read_timer = Metro(1);
Metro led_timer = Metro(100);
Metro sdcard_write_timer = Metro(1);

MTPStorage_SD storage; // add from mtp_blinky
MTPD          mtpd(&storage); // // add from mtp_blinky


//Misc
int led = 13;
FlexCAN CANbus(250000);

static CAN_message_t msg,rxmsg;
static uint8_t hexchars[17] = "0123456789abcdef";

int txCount,rxCount;
unsigned int txTimer,rxTimer;
//Sd2Card card;
//SdVolume volume;
//SdFile root;

//Led signal
int LED_STATE_UNINITIALIZED = 0;
int LED_STATE_HEARTBEAT_OK = 1;
int LED_STATE_WAITING_FOR_SD = 2;
int led_state = 0;
int led_cycle_len = 20;
int led_cycle_position = 0;

//sdcard
const uint8_t SD_CS_PIN = 62;
SdFatSdio sd;
SdFatSdioEX sdEx;
SdFile logfile;
cid_t m_cid;
csd_t m_csd;
uint32_t m_eraseSize;
uint32_t m_ocr;

const int g_log_outbuf_size = 4096;
char g_log_outbuf[g_log_outbuf_size];
int32_t g_log_outbuf_pos = 0;



int mgtnsy2_log(char* tag, char *fmt, ...)
{
	
	va_list argv;
	time_t timeNow = now();
	uint32_t now_millis;
	uint32_t now_seconds;
	uint32_t now_remaining_milis;
	char mprintbuf[64 + 512 + 3]; //[Timestamp + TAG = 64][MSG = 512][\r\n\0] 
	int offset = 0;
	int bytes_to_copy = 0;

	now_millis = millis();
	now_seconds = now_millis / 1000;
	now_remaining_milis = now_millis % 1000;

	va_list args;
    va_start(args, fmt);
    offset = snprintf(mprintbuf, 64, "%d.%d [%s]: ", now_seconds, now_remaining_milis, tag);
    offset = offset + vsnprintf(mprintbuf + offset, 512, fmt, args);
    offset = offset + snprintf(mprintbuf + offset, 3, "\r\n");
    
	Serial.printf("%s", mprintbuf);
	
	//writing to log buffer
	bytes_to_copy  = offset;
	
	if (bytes_to_copy > (g_log_outbuf_size - g_log_outbuf_pos)){
		bytes_to_copy = (g_log_outbuf_size - g_log_outbuf_pos);
	}
	
	if (bytes_to_copy > 0){
		memcpy(g_log_outbuf + g_log_outbuf_pos, mprintbuf, bytes_to_copy);
		g_log_outbuf_pos = g_log_outbuf_pos + bytes_to_copy;
	}
    va_end(args);
}

static void hexDump(char* tag, uint8_t dumpLen, uint8_t *bytePtr)
{
	uint8_t working;
	char hexbuf[512 + 1];
	int idx = 0;
	/*
  	time_t timeNow = now();
	uint32_t now_millis;
	uint32_t now_seconds;
	uint32_t now_remaining_milis;
	
	int i, count=0, j=0, flag=0;

	now_millis = millis();
	now_seconds = now_millis / 1000;
	now_remaining_milis = now_millis % 1000;
	Serial.print(now_seconds);
	Serial.print(".");
	Serial.print(now_remaining_milis);
	Serial.print(" [");
	Serial.print(tag);
	Serial.print("]: ");
	*/
	
	if (dumpLen > sizeof(hexbuf)){
		dumpLen = sizeof(hexbuf);
	}
	
	while( dumpLen-- ) {
		working = *bytePtr++;
		hexbuf[idx] = hexchars[ working>>4 ];
		idx++;
		hexbuf[idx] = hexchars[ working&15 ];
		idx++;
	}
	hexbuf[idx] = '\0';
	mgtnsy2_log(tag, "%s", hexbuf);
}


int mgtnsy2_sdcard_setup(void)
{
	uint32_t sdcapacity_sectors;
	uint32_t sdcapacity_MB;
	uint32_t freeClusterCount = 0;
	int ret = 0;
	uint32_t filelen;
	
	//RTC_IER |= 0x10;  // Enable seconds IRQ from RTC peripheral
	//NVIC_ENABLE_IRQ(IRQ_RTC_SECOND); // Enable seconds IRS function in NVIC
	
	if (!SD.begin()){
		Serial.println("SDCARD NOT WORKING");
		return -1;
	}
	
	if (!sd.card()->readCID(&m_cid) || !sd.card()->readCSD(&m_csd) || !sd.card()->readOCR(&m_ocr)) {
		Serial.println("SDCARD NOT WORKING2");
		return -2;
	}
	
	switch (sd.card()->type()){
		case SD_CARD_TYPE_SD1:
			Serial.println("SDCARD-TYPE: SD1");
			break;
		case SD_CARD_TYPE_SD2:
			Serial.println("SDCARD-TYPE: SD2");
			break;
		case SD_CARD_TYPE_SDHC:
			Serial.println("SDCARD-TYPE: SDHC/SDXC");
			break;
		default:
			Serial.println("Unknoen SDCARD TYPE");
			break;
	}
	
	sdcapacity_sectors = sdCardCapacity(&m_csd);
	sdcapacity_MB = (sdcapacity_sectors / 2048); 
	mgtnsy2_log("SDCARD", "capacity: %d sectors (512 bytes), %d MB", sdcapacity_sectors, sdcapacity_MB);
	/*
	if (!sd.volumeBegin()) {
		mgtnsy2_log("SDCARD", "volumeBegin failed. Is the card formatted? Lets try to format\n");
		ret = sd.format();
		if (ret != 0){
			return -3;
		}
		
		if (!sd.volumeBegin()){
			return -4;
		}
	}*/
  
	freeClusterCount = sd.freeClusterCount();
	/*if (sd.fatType() <= 32){
		mgtnsy2_log("SDCARD", "volume found: FAT, not good for us. formatting");
		ret = sd.format();
		mgtnsy2_log("SDCARD", "after format, ret: %d", ret);
	}else{
		mgtnsy2_log("SDCARD", "volume found: exFAT");
	}*/
	
	//Check if log file exists and whats its size:
	if (sd.exists("mgtnsy2_log.txt")){
		mgtnsy2_log("SDCARD", "log file exists");
	}
	
	//Now lets open the file for logging
	ret =logfile.open("mgtnsy2_log.txt", O_RDWR | O_CREAT);
	if (ret == 0){
		mgtnsy2_log("SDCARD", "failed to open/create file mgtnsy_log.txt");
		return -5;
	}
	
	filelen = logfile.available();
	Serial.printf("size of mgtnsy2_log.txt == %d \n", filelen);
	
	sdcard_write_timer.reset();
	
	return 0;
}

void mgtnsy2_sdcard_proc(void)
{
	//nothing for now
	if (!sdcard_write_timer.check()){
		return;
	}
	
	if (g_log_outbuf_pos > 0){
		//mgtnsy2_log("SDCARD", "writing %d bytes", g_log_outbuf_pos);
		logfile.write(g_log_outbuf, g_log_outbuf_pos);
		logfile.flush();
		g_log_outbuf_pos = 0;
	}
}

void mgtnsy2_can_setup(void)
{
	CANbus.begin();
	pinMode(led, OUTPUT);
	digitalWrite(led, 1);

	delay(1000);

	sysTimer.reset();
}

void mgtnsy2_analog_setup(void)
{
	pinMode(A11, INPUT);
	analog_read_timer.reset();
}

// -------------------------------------------------------------
void can_read_proc(void)
{
  // service software timers based on Metro tick
  if (!sysTimer.check() ) {
	  return;
  }

  // if not time-delayed, read CAN messages and print 1st byte
	while ( CANbus.read(rxmsg) ) {
		hexDump( "CAN", sizeof(rxmsg), (uint8_t *)&rxmsg );
    }
	
}

void analog_read_proc(void)
{
	
	int valA11 = 0;	
	if (!analog_read_timer.check() ) {
		return;
	}	
	valA11 = analogRead(A11);
	mgtnsy2_log("ANALOG", "A11=%d", valA11);	
}

void mgtnsy2_uart1_setup(void)
{
	Serial1.begin(115200);
	uart1_read_timer.reset();
}

void mgtnsy2_uart1_read_proc(void)
{
	char buf[256];
	int available_len = 0;
	// service software timers based on Metro tick
	if (!uart1_read_timer.check() ) {
		return;
	}
	
	available_len = Serial1.available();
	
	while (available_len > 0) {
		Serial1.readBytesUntil('\n', buf, sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = '\0';
		available_len = Serial1.available();
		mgtnsy2_log("UART1", "%s", buf);
	}
}

void mgtnsy2_led_setup(void)
{
	pinMode(LED_BUILTIN, OUTPUT);
	led_timer.reset();
}

void mgtnsy2_led_proc(void)
{
	if (!led_timer.check() ) {
		return;
	}


	if (led_state == LED_STATE_HEARTBEAT_OK){
		if (led_cycle_position == 0 || led_cycle_position == 2){
			digitalWrite(LED_BUILTIN, HIGH);
		}else{
			digitalWrite(LED_BUILTIN, LOW);
		}
	}else if (led_state == LED_STATE_WAITING_FOR_SD){
		if (led_cycle_position == 1 || led_cycle_position == 2 || led_cycle_position == 3 || led_cycle_position == 4 ||  led_cycle_position == 5 ||
			led_cycle_position == 11 || led_cycle_position == 12 || led_cycle_position == 13 || led_cycle_position == 14 || led_cycle_position == 15){
			digitalWrite(LED_BUILTIN, HIGH);
		}else{
			digitalWrite(LED_BUILTIN, LOW);
		}
	}
	
	led_cycle_position++;
	if (led_cycle_position > led_cycle_len){
		led_cycle_position = 0;
	}
}

//usb_packet_t_2 *receive_buffer;  

Metro enter_timer = Metro(3000);	
// the setup() method runs once, when the sketch starts

const int  MaxChars = 100;

char receivedChars[MaxChars]; // an array to store the received data

char com_received [MaxChars]; 	

bool enter_command = false;

static byte com_index = 0;

static int count_e = 0;

int num = 0;

char del;

static int com_count = 0;

char exit_str [] = "exit";

void log_screen(void);

bool stor = false;  // bool variable to control mtpd.loop to open and close storage





// the loop() methor runs over and over again,
// as long as the board has power

// flushing the serial
void serialFlush(){
	while (Serial.available() > 0) {
    		del = Serial.read();
  	}
}   


// the loop() methor runs over and over again,
// as long as the board has power

void loop() {	
		mtpd.loop(stor);
		can_read_proc();
		analog_read_proc();
		mgtnsy2_uart1_read_proc();
		mgtnsy2_led_proc();
		mgtnsy2_sdcard_proc();
		num++;
	
}



// detect exit command
int check_exit(){
	if ((com_received [com_index-4] == 'e') && (com_received [com_index-3] == 'x') && (com_received [com_index-2] == 'i') && (com_received [com_index-1] == 't') && (com_count == 4)) {
		com_received [com_index-4] = com_received [com_index-3] = com_received [com_index-2] = com_received [com_index-1] = ' ';
		return 1;
	}
	else return 0;
}



// detect reboot command
int check_reboot(){
	if ((com_received [com_index-6] == 'r') && (com_received [com_index-5] == 'e') && (com_received [com_index-4] == 'b') && 
		(com_received [com_index-3] == 'o') && (com_received [com_index-2] == 'o') && (com_received [com_index-1] ==  't') && (com_count == 6))	
		return 1;
	else return 0;
}


//detect storage command
int check_storage(){
	if ((com_received [com_index-7] == 's') && (com_received [com_index-6] == 't') && (com_received [com_index-5] == 'o') && 
		(com_received [com_index-4] == 'r') && (com_received [com_index-3] == 'a') && (com_received [com_index-2] ==  'g') && (com_received [com_index-1] ==  'e') && (com_count == 7))	
		return 1;
	else return 0;
}

// detect unstorage command
int check_unstorage(){
	if ((com_received [com_index-9] == 'u') && (com_received [com_index-8] == 'n') && (com_received [com_index-7] == 's') && (com_received [com_index-6] == 't') && (com_received [com_index-5] == 'o') && 
		(com_received [com_index-4] == 'r') && (com_received [com_index-3] == 'a') && (com_received [com_index-2] ==  'g') && (com_received [com_index-1] ==  'e') && (com_count == 9))	
		return 1;
	else return 0;
}


// open a command line
void command_line (void){			
	char ch_cm;					
	while (enter_command) {		 
		ch_cm = Serial.read();
		if ((ch_cm >= 33) && (ch_cm <= 126)) { 
			Serial.print(ch_cm); 
			com_received [com_index++] = ch_cm; 
			com_count++;
		}				
		if (ch_cm == 13) {		
			if (check_exit() == 1) {
				Serial.println();
				Serial.println();
				Serial.println("-- Log Screen --");	
				Serial.println();		
				enter_command = false;
			}
			else if (check_storage() == 1) {
				Serial.println();
				Serial.println();
				Serial.println("-- STORAGE OPEN --");	
				Serial.println();
				stor = true;
				enter_command = false;
			}
			else if (check_unstorage() == 1) {
				Serial.println();
				Serial.println();
				Serial.println("-- STORAGE CLOSE--");	
				Serial.println();
				stor = false;
				enter_command = false;
			}
			else if (check_reboot() == 1) {	
				WRITE_RESTART(0x5FA0004); 
				serialFlush();
 			}
			else {			
				Serial.println();
				Serial.print (">$");
				com_count = 0;
			}
		}
	} 
}

int count_check = 0; // count

// check again the enters to prevent mistakes on the connect to picicom
bool check_again(){
	char c;
	int counter = 0;
	while (Serial.available()) {
      		c = Serial.read();
			if (c == 13) counter++;
	}
	if (counter > 0){
		count_check++;
		return false;
	}
	return true; 	
}


// check if the enter button pressed 3 times in 3 seconds, if it pressed - go to command line...
void serialEvent() { 		  
	static byte index = 0;	
	char ch;	
	while (Serial.available()) {
      		ch = Serial.read();
		if (index < MaxChars) receivedChars[index++] = ch; 
		else receivedChars[index] = 0;
	    if (ch == 13) count_e++;
		if (count_e >= 3) { 
			if ((count_check > 1) || (check_again())) {
				enter_command = true; 			
				Serial.println();
				Serial.println("-- Exit to command line --");
				Serial.println();
				Serial.println ("1- Press 'exit' to back log screen");
				Serial.println ("2- Press 'storage' to open storage");
				Serial.println ("3- Press 'unstorage' to close storage");
				Serial.println ("4- Press 'reboot' to reboot teensy");
				serialFlush();			
				Serial.println();
				Serial.print (">$");
				com_count = 0;				
				command_line();	
			}
		}        
	}  
}

// check every 3 seconds if there was a serial event like pressing enter button
void check_enter (void)
{			
	if (!enter_timer.check()) return;
	else {	
		count_e = 0;
		serialEvent();			 					    
	}
}


void log_screen(void) {	
	Serial.println("-- Log Screen --");	
	Serial.println();
	Serial.flush();
	while (!enter_command) {
		check_enter();	
		loop();				
	}		
}


// open the log screen,run the loop function while the enter button wasn't press 3 times in 3 seconds
extern "C" int main(void)
{
	int ret = 0;
    Serial.begin(115200);
    serialFlush();
    delay(2000);
    mgtnsy2_analog_setup();
	mgtnsy2_can_setup();
	mgtnsy2_uart1_setup();
	ret = mgtnsy2_sdcard_setup();
	if (ret != 0){
		led_state = LED_STATE_WAITING_FOR_SD;	
	}else{
		led_state = LED_STATE_HEARTBEAT_OK;
	}
	serialFlush();
	log_screen();
  
}

