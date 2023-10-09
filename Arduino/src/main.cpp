#include <LowPower.h>
#include <DHT11.h>
#include <LiquidCrystal.h>

/*
LCD Circuit:
 * LCD RS pin to digital pin 12
 * LCD Enable pin to digital pin 10
 * LCD D4 pin to digital pin 5
 * LCD D5 pin to digital pin 4
 * LCD D6 pin to digital pin 3
 * LCD D7 pin to digital pin 7
 * LCD R/W pin to ground
 * LCD VSS pin to ground
 * LCD VCC pin to 5V
 * 10K resistor:
 * ends to +5V and ground
 * wiper to LCD VO pin (pin 3)
*/

// ============ PINS ============ 
// LCD Pins
const int rs = 12, en = 10, d4 = 5, d5 = 4, d6 = 3, d7 = 7;
const int lcdBacklightPin = A0;

// DHT11 Pin
const int dhtPin = 8;

// RGB LED Pins
const int rPin = 9;
const int gPin = 6;
const int bPin = 11;

// ============ SETTINGS ============
const int maxTransmissionInterval = 6000;
const int temperatureInterval = 5000; 
const int powerOffInterval = 20000;


// ============ VARIABLES ============
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
DHT11 dht(dhtPin);

int currR = 0, currG = 0, currB = 0;
int lastR = 0, lastG = 0, lastB = 0;

char buffer[16];
int temperature = 0;
unsigned int lastTransmission = millis();
unsigned int lastTempReading = 0;
unsigned int avgTransmissionInterval = 200;
bool connection = false;

// ============ FUNCTIONS ============
void readColor();
void readStats();
double interpolate(double k, double start, double end);
void setColor(int r, int g, int b);
double clamp(double a, double start, double end);

void setup()
{
	Serial.begin(9600);

	pinMode(rPin, OUTPUT);
	pinMode(gPin, OUTPUT);
	pinMode(bPin, OUTPUT);
	pinMode(2, INPUT);
	pinMode(lcdBacklightPin, OUTPUT);
	digitalWrite(lcdBacklightPin, HIGH);

	lcd.begin(16, 2);

	lcd.print("Waiting for");
	lcd.setCursor(0, 1);
	lcd.print("connection...");
}

void loop()
{
	unsigned int now = millis();

	if (Serial.available() > 0)
	{
		connection = true;

		Serial.readBytes(buffer, 1);
		char op = buffer[0];

		switch (op)
		{
		case 'A':
			readColor();
			break;
		case 'B':
			readColor();
			readStats();
			break;

		case 'C':
			lcd.clear();
			lcd.setCursor(0, 0);
			Serial.readBytes(buffer, 16);
			lcd.print(buffer);
			Serial.readBytes(buffer, 16);
			lcd.setCursor(0, 1);
			lcd.print(buffer);
			Serial.flush();
			break;
		}

		Serial.print("GOT");

		unsigned int transmissionInterval = now - lastTransmission;
		avgTransmissionInterval = (avgTransmissionInterval + transmissionInterval) / 2;
		lastTransmission = now;
	}

	// Interpolate color values
	unsigned int timeSinceLastTransmission = now - lastTransmission;
	double k = (double)timeSinceLastTransmission / (double)min(avgTransmissionInterval, 150);
	setColor((int)interpolate(k, lastR, currR), (int)interpolate(k, lastG, currG), (int)interpolate(k, lastB, currB));

	// Check if connection is lost
	unsigned int elapsedTillLastTransmission = now - lastTransmission;
	if (elapsedTillLastTransmission >= maxTransmissionInterval && connection)
	{
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("Connection lost.");
		lcd.setCursor(0, 1);
		lcd.print("Reconnecting...");
		connection = false;
	}

	if (elapsedTillLastTransmission >= powerOffInterval && !connection)
	{
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("Powering off...");
		delay(2000);
		lcd.noDisplay();
		digitalWrite(lcdBacklightPin, LOW);
		delay(1000);
		LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
	}

	// Take temperature reading
	unsigned int elapsedTillLastTemperatureReading = now - lastTempReading;
	if (elapsedTillLastTemperatureReading >= temperatureInterval || temperature == 0)
	{
		int reading = dht.readTemperature();
		if (reading != 253)
		{
			temperature = reading;
		}
		lastTempReading = millis();
	}
}

void readColor()
{
	// Read color values - 9 characters "255255255" => (255, 255, 255)
	Serial.readBytes(buffer, 9);
	// Hundreds place, tens place, ones place
	int rH = buffer[0] - '0';
	int rT = buffer[1] - '0';
	int rO = buffer[2] - '0';

	int gH = buffer[3] - '0';
	int gT = buffer[4] - '0';
	int gO = buffer[5] - '0';

	int bH = buffer[6] - '0';
	int bT = buffer[7] - '0';
	int bO = buffer[8] - '0';

	int r = rH * 100 + rT * 10 + rO;
	int g = gH * 100 + gT * 10 + gO;
	int b = bH * 100 + bT * 10 + bO;

	setColor(currR, currG, currB);
	lastR = currR;
	lastG = currG;
	lastB = currB;
	currR = r;
	currG = g;
	currB = b;
}

void readStats()
{
	// Read first line
	Serial.readBytes(buffer, 16);
	lcd.setCursor(0, 0);
	lcd.print(buffer);

	// Read second line
	Serial.readBytes(buffer, 12);
	lcd.setCursor(0, 1);
	lcd.print(buffer);

	// Print temperature
	lcd.setCursor(12, 1);
	lcd.print(temperature);
	lcd.print((char)223);
	lcd.print("C");
}

double interpolate(double k, double start, double end)
{
	// return (((double)(end - start)) * k) + start;
	return clamp((((double)(end - start)) * k) + start, start, end);
}

void setColor(int r, int g, int b)
{
	analogWrite(rPin, clamp(r, 0, 255));
	analogWrite(gPin, clamp(g, 0, 255));
	analogWrite(bPin, clamp(b, 0, 255));
}

double clamp(double a, double start, double end)
{
	double lower = min(start, end);
	double upper = max(start, end);
	return min(max(a, lower), upper);
}