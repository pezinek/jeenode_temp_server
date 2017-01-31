#include <JeeLib.h>
#include <PortsBMP085.h>
// Simple HTTP server, sending current temperature and pressure from BMP085 sensor in json.
//
// Hardware:
// http://jeelabs.org/jn4
// http://jeelabs.org/pp1
// http://jeelabs.org/cb
// http://jeelabs.org/ec1
//


#include <EtherCard.h>
#include <avr/wdt.h>

static byte mymac[] = { 0x6A,0x6A,0x65,0x2D,0x30,0x31 };
uint8_t ip[] = { 192, 168, 1, 106 };       // The fallback board address.
uint8_t dns[] = { 192, 168, 1, 1 };        // The DNS server address.
uint8_t gateway[] = { 192, 168, 1, 1 };    // The gateway router address.
uint8_t subnet[] = { 255, 255, 255, 0 };   // The subnet mask.
byte Ethernet::buffer[512];
BufferFiller bfill;

PortI2C one (1);
BMP085 psensor (one, 3); // ultra high resolution


void setup() {
  // put your setup code here, to run once:
  Serial.begin(57600);
  Serial.println("\n[temperature_server]");

  wdt_enable(WDTO_8S);

  if (ether.begin(sizeof Ethernet::buffer, mymac, 8) == 0) {
    Serial.println( "Failed to access Ethernet controller");    
    while (true) {
        delay(1000);
    }
  }  
    
  wdt_reset();

  Serial.println("Requesting DHCP");
  if (ether.dhcpSetup()) {
    ether.printIp("Got IP: ", ether.myip);
  } else {
    ether.staticSetup(ip, gateway, dns);
    ether.printIp("DHCP failed, using fixed IP: ", ether.myip);
  }

  wdt_reset();

  Serial.println("Loading calibration data");
  psensor.getCalibData();

  Serial.println("Ready");
}

static word homePage() {

  psensor.startMeas(BMP085::TEMP);
  delay(16);
  int32_t traw = psensor.getResult(BMP085::TEMP);

  psensor.startMeas(BMP085::PRES);
  delay(32);
  int32_t praw = psensor.getResult(BMP085::PRES);

  struct { int16_t temp; int32_t pres; } payload;
  psensor.calculate(payload.temp, payload.pres);

  Serial.print("temp_raw: ");
  Serial.print(traw);
  Serial.print(" pres_raw: ");
  Serial.print(praw);
  Serial.print(" temp: ");
  Serial.print(payload.temp);
  Serial.print(" pres: ");
  Serial.println(payload.pres);
  Serial.flush();

  bfill = ether.tcpOffset();
  bfill.emit_p(PSTR(
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Pragma: no-cache\r\n"
    "\r\n"
    "{\"temperature_raw\" : $L, \"temperature\" : $L.$L, \"pressure_raw\" : $L, \"pressure\" : $L}\r\n"),
    long(traw), long(payload.temp/10), long(abs(payload.temp)%10), long(praw), long(payload.pres));
  return bfill.position();
}


void loop() {
  // put your main code here, to run repeatedly:
  wdt_reset();

  word len = ether.packetReceive();
  word pos = ether.packetLoop(len);

  if (pos)  // check if valid tcp data is received
    ether.httpServerReply(homePage());
  
}
