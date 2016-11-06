#include "WiFiEsp.h"
#include <FuzzyRule.h>
#include <FuzzyComposition.h>
#include <Fuzzy.h>
#include <FuzzyRuleConsequent.h>
#include <FuzzyOutput.h>
#include <FuzzyInput.h>
#include <FuzzyIO.h>
#include <FuzzySet.h>
#include <FuzzyRuleAntecedent.h>

#define trigPin 12
#define echoPin 11

byte sensorInterrupt = 0;
byte sensorPin       = 2;

// Kalibrasi rate air  4.5 per detik untuk kalkulasi rate air litel/jam.
float calibrationFactor = 4.5;

volatile byte pulseCount;

float kecepatan;
unsigned int flowMilliLitres;
unsigned long totalMilliLitres;

unsigned long oldTime;

Fuzzy* fuzzy = new Fuzzy();

char ssid[] = "CBN-PA";         // SSID
char pass[] = "12345678";       // password
int status = WL_IDLE_STATUS;    // status radio WiFi

char server[] = "192.168.4.1";

//unsigned long lastConnectionTime = 0;
//const unsigned long postingInterval = 4000L; // delay update

WiFiEspClient client;

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);
  WiFi.init(&Serial1);
  // memeriksa koneksi modul ESP
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("Modul ESP tidak tersedia");
    while (true);
  }

  // menghubungkan perangkat ke jaringan WiFi
  while ( status != WL_CONNECTED) {
    Serial.print("Menghubungkan perangkat ke jaringan WiFi : ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
  }

  Serial.println("Berhasil terhubung ke jaringan WiFi");

  printWifiStatus();

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  pinMode(sensorPin, INPUT);
  digitalWrite(sensorPin, HIGH);

  pulseCount        = 0;
  kecepatan         = 0;
  flowMilliLitres   = 0;
  totalMilliLitres  = 0;
  oldTime           = 0;

  attachInterrupt(sensorInterrupt, pulseCounter, FALLING);


  // FuzzySet input sensor ketinggian air
  FuzzyInput* distance = new FuzzyInput(1);
  FuzzySet* small = new FuzzySet(0, 20, 20, 40);
  distance->addFuzzySet(small);
  FuzzySet* safe = new FuzzySet(30, 50, 50, 70);
  distance->addFuzzySet(safe);
  FuzzySet* big = new FuzzySet(60, 80, 100, 100);
  distance->addFuzzySet(big);

  fuzzy->addFuzzyInput(distance);

  // FuzzySet input sensor kecepatan air
  FuzzyInput* speed = new FuzzyInput(2);
  FuzzySet* low = new FuzzySet(0, 0, 0, 0);
  distance->addFuzzySet(low);
  FuzzySet* mid = new FuzzySet(1, 10, 10, 20);
  distance->addFuzzySet(mid);
  FuzzySet* high = new FuzzySet(15, 30, 30, 50);
  distance->addFuzzySet(high);

  fuzzy->addFuzzyInput(speed);

  // FuzzySet output dari 2 sensor(ketinggian dan kecepatan air);
  FuzzyOutput* status = new FuzzyOutput(1);
  FuzzySet* normal = new FuzzySet(0, 0, 0, 0);
  status->addFuzzySet(normal);
  FuzzySet* waspada = new FuzzySet(1, 10, 10, 20);
  status->addFuzzySet(waspada);
  FuzzySet* siaga = new FuzzySet(15, 30, 30, 50);
  status->addFuzzySet(siaga);
  FuzzySet* awas = new FuzzySet(45, 60, 70, 70);
  status->addFuzzySet(awas);

  fuzzy->addFuzzyOutput(status);


  // Fuzzy rules
  // FuzzyRule "IF tinggiair = small AND kecepatanAir = low THEN status = normal"
  FuzzyRuleAntecedent* ifDistanceSmallAndSpeedLow = new FuzzyRuleAntecedent(); // Inialisasi pendahuluan ekpresi fuzzy
  ifDistanceSmallAndSpeedLow->joinWithAND(small, low); // Set fuzzy berdasarkan batasan sensor
  FuzzyRuleConsequent* thenStatusNormal = new FuzzyRuleConsequent(); // Inialisasi konsekuen ekpresi fuzzy
  thenStatusNormal->addOutput(normal); // Set fuzzy berdasarkan batasan sensor ke objek konsekuensi fuzzy
  // Inialisasi objek aturan fuzzy
  FuzzyRule* fuzzyRule01 = new FuzzyRule(1, ifDistanceSmallAndSpeedLow, thenStatusNormal); // Proses melewatkan pendahuluan fuzzy dan konsekuen fuzzy
  fuzzy->addFuzzyRule(fuzzyRule01); // Menambahkan aturan fuzzy ke objek fuzzy


  // FuzzyRule "IF tinggiair = small AND kecepatanAir = mid THEN status = waspada"
  FuzzyRuleAntecedent* ifDistanceSmallAndSpeedMid = new FuzzyRuleAntecedent();
  ifDistanceSmallAndSpeedMid->joinWithAND(small, mid);
  FuzzyRuleConsequent* thenStatusWaspada = new FuzzyRuleConsequent();
  thenStatusWaspada->addOutput(waspada);
  // Inialisasi objek aturan fuzzy
  FuzzyRule* fuzzyRule02 = new FuzzyRule(2, ifDistanceSmallAndSpeedMid, thenStatusWaspada);
  fuzzy->addFuzzyRule(fuzzyRule02);

  // FuzzyRule "IF tinggiair = small AND kecepatanAir = high THEN status = siaga"
  FuzzyRuleAntecedent* ifDistanceSmallAndSpeedHigh = new FuzzyRuleAntecedent();
  ifDistanceSmallAndSpeedHigh->joinWithAND(small, high);
  FuzzyRuleConsequent* thenStatusSiaga = new FuzzyRuleConsequent();
  thenStatusSiaga->addOutput(siaga);
  // Inialisasi objek aturan fuzzy
  FuzzyRule* fuzzyRule03 = new FuzzyRule(3, ifDistanceSmallAndSpeedHigh, thenStatusSiaga);
  fuzzy->addFuzzyRule(fuzzyRule03);

  // FuzzyRule "IF tinggiAir = big AND kecepatanAir = high THEN status = awas"
  FuzzyRuleAntecedent* ifDistanceBigAndSpeedHigh = new FuzzyRuleAntecedent();
  ifDistanceBigAndSpeedHigh->joinWithAND(big, high);
  FuzzyRuleConsequent* thenStatusAwas = new FuzzyRuleConsequent();
  thenStatusAwas->addOutput(awas);
  // Inialisasi objek aturan fuzzy
  FuzzyRule* fuzzyRule04 = new FuzzyRule(4, ifDistanceBigAndSpeedHigh, thenStatusAwas);
  fuzzy->addFuzzyRule(fuzzyRule04);
}


void loop() {
  long duration, ketinggian;
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  ketinggian = (duration / 2) / 29.1;

  //set inputan sensor untuk object fuzzy logic
  fuzzy->setInput(1, ketinggian);
  fuzzy->setInput(2, kecepatan);

  fuzzy->fuzzify();

  float defuzzy = fuzzy->defuzzify(1);



  if (defuzzy < 61 && kecepatan < 0 || defuzzy > 60) {
  }



  Serial.println("");
  Serial.print("Ketinggian air : - ");
  Serial.print(ketinggian);
  Serial.print(" cm");


  kecepatanAir();

  Serial.print("\tDefuzzyfikasi : ");
  Serial.print(defuzzy);


  while (client.available()) {
    char c = client.read();
    Serial.write(c);
  }
  Serial.println();
  // client.stop();
  if (client.connect(server, 80)) {
    if (ketinggian < 15) {
      client.println("GET /H");
    } else {
      client.println(F("GET /L"));
    }
    client.println("HTTP/1.1");
    client.println("Host: 192.168.4.1");
    client.println("Connection: close");
    client.println();
    client.println();
  }
  else {
    Serial.println("Koneksi gagal");
  }
  if (client.connected()) {
    client.stop();
  }
  delay(1000);
}


void kecepatanAir() {
  if ((millis() - oldTime) > 1000)
  {
    detachInterrupt(sensorInterrupt);
    kecepatan = ((1000.0 / (millis() - oldTime)) * pulseCount) / calibrationFactor;

    oldTime = millis();

    flowMilliLitres = (kecepatan / 60) * 1000;

    totalMilliLitres += flowMilliLitres;

    Serial.print("\tKecepatan : ");
    Serial.print(int(kecepatan));
    Serial.print("L/min");

    pulseCount = 0;

    attachInterrupt(sensorInterrupt, pulseCounter, FALLING);
  }
}

void printWifiStatus()
{
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
}

void pulseCounter() {
  pulseCount++;
}
