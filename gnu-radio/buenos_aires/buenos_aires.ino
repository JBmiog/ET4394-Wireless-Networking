/*
Jeffry Miog, Jeffrey.miog@gmail.com
march 2016

simple dummy broadcaster for a gnu-radio project, in which we try to 
receive the transmitted data on serial_1 
settings: 
frequency carrier = 433.99 mHz
baud rate air = 1200
transmit power is- 8dBm

*/


void setup() {                
  pinMode(16, OUTPUT); // init LED     

  Serial.begin(9600); // init serial c8ommunication @ 9600 baud
  Serial1.begin(9600);
  // serial is the USB connection
  // for Transmitter Serial1, Serial2 or Serial 3 are used 
  // dependant on the used poort

}

void loop() {
  
  // it is very good practice to having a blinking LED 
  // this indicates that the programm is running  
  digitalWrite(16, HIGH);   // set the LED on
  delay(100);              // wait for half a second

  digitalWrite(16, LOW);    // set the LED off
  delay(200);              // wait for half a se
  Serial1.println("Buenos Aires");  // starts new line
}
