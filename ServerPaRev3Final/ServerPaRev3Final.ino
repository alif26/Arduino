
#include "WiFiEsp.h"
//#define buzzer = 12;

char ssid[] = "CBN-PA";         // SSID
char pass[] = "12345678";       // password
int status = WL_IDLE_STATUS;    // status radio WiFi

int ledStatus = LOW;

WiFiEspServer server(80);

// menggunakan ring buffer untuk meningkatkan kecepatan dan mengurangi alokasi memori
RingBuffer buf(8);

void setup()
{
  Serial.begin(115200);   // inialisasi serial untuk debugging
  Serial1.begin(9600);    // inialisasi serial untuk modul ESP
 // pinMode(13, OUTPUT);
//  pinMode(buzzer, OUTPUT);

  WiFi.init(&Serial1);    // inialisasi modul ESP

  // memeriksa koneksi modul ESP
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("Modul ESP tidak tersedia");
    while (true); 
  }
  // menghubungkan perangkat ke jaringan WiFi
  Serial.print("ESP Server AP ");
  Serial.println(ssid);


  // set mode ESP ke Akses Point
  status = WiFi.beginAP(ssid, 10, pass, ENC_TYPE_WPA2_PSK);

  Serial.println("Mode Akses Point");
  printWifiStatus();

  // server port 80
  server.begin();
  Serial.println("Server started");
}


void loop()
{
  WiFiEspClient client = server.available();  // listen for incoming clients

  if (client) {
    buf.init();                               // inialisasi ring buffer
    while (client.connected()) {              // loop while the client's connected
      if (client.available()) {               // jika ada byte masuk dari klien
        char c = client.read();               // kemudian dilakukan proses pada ring buffer
        buf.push(c);

        if (buf.endsWith("\r\n\r\n")) {
          sendHttpResponse(client);
          break;
        }

        if (buf.endsWith("GET /H")) {
          digitalWrite(13, HIGH);
          //digitalWrite(buzzer, HIGH);
        }
        else if (buf.endsWith("GET /L")) {
          digitalWrite(13, LOW);
          //digitalWrite(buzzer, LOW);
        }
      }
    }

    // delay web browser
    delay(10);

    client.stop();
    Serial.println("Client disconnected");
  }
}

void sendHttpResponse(WiFiEspClient client)
{
  // HTTP header HTTP/1.1 200 OK. tipe konten text/html
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println( "Connection: close" );
  client.println();
  client.println("<a href=\"/H\">ON</a><br>");
  client.println("<a href=\"/L\">OFF</a><br>");
  //client.println( "Connection: close" );
  client.println();
}

void printWifiStatus()
{
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
}

