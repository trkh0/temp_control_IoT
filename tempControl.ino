// Load Wi-Fi library
#include <WiFi.h>
#include "DHT.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <esp32-hal-timer.h>

#define DHT11PIN 16
#define LED_PIN 2
#define IP_BUTTON_PIN 5

const char* ssid = "HUAWEI_p30_lite";
const char* password = "87654321";
// Set web server port number to 80
WiFiServer server(80);

hw_timer_t *timer = NULL;
DHT dht(DHT11PIN, DHT11);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Variable to store the HTTP request
String header;

String led_state = "off";
String mode_state = "auto";
float treshold = 24.0;
float temp;
int humi;
int mode = 0; //mode: 0 - auto ; 1 - manual
int update = 1;

byte degree_symbol[8] =
{
  0b00111,
  0b00101,
  0b00111,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};

//read from dht sensor
void readDHT(float *temp, int *humi) {
  *humi = dht.readHumidity();
  *temp = dht.readTemperature();
}

//updates the temperature and humidity values on the LCD screen
void updateScreen(float temp, int humi) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp = ");
  lcd.print(temp);
  lcd.write(0);
  lcd.print("C");
  lcd.setCursor(0, 1);
  lcd.print("Humidity = ");
  lcd.print(humi);
  lcd.print("%");
}

//sets the led in auto mode
void setLed(float temp) {
  if (mode == 0) {
    if (temp >= treshold) {
      digitalWrite(LED_PIN, HIGH);
      led_state = "on";
    } else {
      digitalWrite(LED_PIN, LOW);
      led_state = "off";
    }
  }
}

void dataUpdate(){
  if (update == 1) {
    readDHT(&temp, &humi);
    updateScreen(temp, humi);
    if (mode == 0)
      setLed(temp);
    update = 0;
  }
}

// timer callback routine
void IRAM_ATTR onTimer() {
  update = 1;
}

void setup() {
  Serial.begin(115200);

  //initializing DHT sensor
  dht.begin();

  //initializing 1602LCD
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, degree_symbol);

  //display LCD startup message
  lcd.setCursor(0, 0);
  lcd.print(" Starting up... ");
  delay(2000);
  lcd.clear();

  //setting GPIO mode for pins
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(IP_BUTTON_PIN, INPUT);


  // connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  lcd.setCursor(0, 0);
  lcd.print("Connecting to:");
  lcd.setCursor(0, 1);
  lcd.print(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();

  // Create and attach the timer
  timer = timerBegin(0, 80, true);  // Timer 0, divider 80
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000000, true); // 1 second = 1,000,000 microseconds
  timerAlarmEnable(timer);

  // Start the timer
  timerAlarmEnable(timer);
}
void loop() {
  WiFiClient client = server.available(); // Listen for incoming clients

  // if the timer interrupt sets update to 1, we read from the dht sensor and process the data
  dataUpdate();

  //display IP if button is pressed
  if (digitalRead(IP_BUTTON_PIN) == HIGH){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(WiFi.localIP());
    lcd.setCursor(0, 1);
    lcd.print("Port: 80");
    delay(100);
  }

  if (client) { // If a new client connects,
    Serial.println("New Client."); // print a message out in the serial port
    String currentLine = ""; // make a String to hold incoming data from the client
    while (client.connected()) { // loop while the client's connected

       // if the timer interrupt sets update to 1, we read from the dht sensor and process the data
       // we do this even if the client is connected
      dataUpdate();

      if (client.available()) { // if there's bytes to read from the client,
        char c = client.read(); // read a byte, then
        Serial.write(c); // print it out the serial monitor
        header += c;
        if (c == '\n') { // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP / 1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            // turns the GPIOs on and off
            if (header.indexOf("GET /MODE/manual") >= 0) {
              Serial.println("manual mode");
              mode = 1;
              mode_state = "manual";
            } else if (header.indexOf("GET /MODE/auto") >= 0) {
              Serial.println("auto mode");
              mode = 0;
              mode_state = "auto";
            } else if (header.indexOf("GET /LED/on") >= 0) {
              Serial.println("LED on");
              led_state = "on";
              digitalWrite(LED_PIN, HIGH);
            } else if (header.indexOf("GET /LED/off") >= 0) {
              Serial.println("LED off");
              led_state = "off";
              digitalWrite(LED_PIN, LOW);
            } else if (header.indexOf("GET /TRESHOLD/plus") >= 0) {
              Serial.println("TRESHOLD plus");
              treshold += .5;
            } else if (header.indexOf("GET /TRESHOLD/minus") >= 0) {
              Serial.println("TRESHOLD minus");
              treshold -= .5;
            }
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\"content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons
            client.println("<style>html { font-family: Helvetica; display:inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border:none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin:2px; cursor: pointer;}");
            client.println(".button2 {background-color:#555555;}</style></head>");
            // Web Page Heading
            client.println("<body><h1>Temperature control</h1>");
            // Display the temperature and humidity
            client.println("<p>TEMP " + String(temp) + "</p>");
            client.println("<p>HUMIDITY " + String(humi) + "</p>");
            // mode switching
            if (mode_state == "auto") {
              client.println("<p><a href=\"/MODE/manual\"><button class=\"button\">MANUAL</button></a></p>");
            } else {
              client.println("<p><a href=\"/MODE/auto\"><button class=\"button button2\">AUTO</button></a></p>");
            }
            client.println("<p>Mode: " + mode_state + "</p>");

            // ON OFF button in manual mode
            if (mode_state == "manual") {
              if (led_state == "off") {
                client.println("<p><a href=\"/LED/on\"><button class=\"button\">ON</button></a></p>");
              }
              if (led_state == "on") {
                client.println("<p><a href=\"/LED/off\"><button class=\"button button2\">OFF</button></a></p>");
              }
            // treshold settings in auto mode
            } else {
              client.println("<p>Temperature treshold: " + String(treshold) + "</p>");
              client.println("<p><a href=\"/TRESHOLD/minus\"><button class=\"button button2\">-</button></a><a href=\"/TRESHOLD/plus\"><button class=\"button\">+</button></a></p>");
            }
            client.println("</body></html>");
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') { // if you got anything else but a carriage return character,
          currentLine += c; // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}
