#include <Arduino.h>
#include <ESPmDNS.h>
#include <AccelStepper.h>
#include <WebServer.h>
#include <WiFi.h>

// Import WIFI_SSID and WIFI_PASSWORD
#include "private.h"

// The 28BYJ-48 has 2048 steps per full revolution in half-step/full-step config
const int stepsPerRevolution = 2048;

#define WEB_REQUEST_STEPS 3072
#define IN1 19
#define IN2 18
#define IN3 5
#define IN4 17
// Note the pin order: IN1, IN3, IN2, IN4 for proper sequence on many 28BYJ setups
AccelStepper myStepper(AccelStepper::FULL4WIRE, IN1, IN3, IN2, IN4);

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* hostname = "steppercontrol";  // Your custom hostname

const char* htmlPrelude = R"(
<html>
  <head>
  <style> html { font-size: 300%;}</style>
  </head>
<body>
)";

const char* script = R"(
<script>
const CLOCKWISE = 1;
const ANTICLOCKWISE = 2;
const STOP = 3;
async function rotate(target) {
  const url = 'http://steppercontrol.local/';
  urlTarget = 'stop';
  if (target == CLOCKWISE) {
    urlTarget = 'rotatecw';
  } else if (target == ANTICLOCKWISE) {
    urlTarget = 'rotateccw';
  }
  fullUrl = url + urlTarget;
  const data = {target : target};
  try {
    const response = await fetch(fullUrl, {
      method : 'POST',  // Specify the HTTP method
      headers : {
        'Content-Type' :
            'application/json'  // Tell the server you're sending JSON
      },
      body : JSON.stringify(data)  // Convert JavaScript object to string
    });

    if (!response.ok) {
      throw new Error(`HTTP error !Status : $ { response.status }`);
    }

    const result =
        await response.json();  // Parse the JSON response from the server
    console.log('Success:', result);
  } catch (error) {
    console.error('Error:', error);
  }
}
</script>
)";

char HTMLResponse[4096];

// Create a web server object that listens for HTTP request on port 80
WebServer server(80);

void SendHeaders() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
}

// Function to handle requests to the root URL (/)
void handleRoot() {
  Serial.println("handleRoot");
  snprintf(HTMLResponse, sizeof(HTMLResponse) - 1,
  R"(
%s
<h1>ESP32 Stepper Control</h1>
<p>Stepper Control</p>
<form action='/toggleLED' method='post'>
<button type='button' class='btn-cw' onclick='rotate(CLOCKWISE)' style='font-size: 100%%;'>Rotate CW</button>
<button type='button' class='btn-ccw' onclick='rotate(ANTICLOCKWISE)' style='font-size: 100%%;'>Rotate CCW</button>
<button type='button' class='btn-ccw' onclick='rotate(STOP)' style='font-size: 100%%;'>Stop</button>
</form>
%s
</body></html>
)",
           htmlPrelude, script);
  SendHeaders();
  server.send(200, "text/html", HTMLResponse);
}

// Function to handle 404 (Not Found) errors
void handleNotFound() { server.send(404, "text/plain", "404: Not Found"); }

void handleRotateCW() {
  // if (!server.hasArg("plain")) {
  //   server.send(400, "application/json", "{\"error\":\"Body missing\"}");
  //   return;
  // }
  Serial.println("handleRotateCW");
  server.send(200, "application/json", "{}");
  myStepper.stop();
  myStepper.move(WEB_REQUEST_STEPS);
}

void handleRotateCCW() {
  // if (!server.hasArg("plain")) {
  //   server.send(400, "application/json", "{\"error\":\"Body missing\"}");
  //   return;
  // }
  Serial.println("handleRotateCCW");
  server.send(200, "application/json", "{}");
  myStepper.stop();
  myStepper.move(-WEB_REQUEST_STEPS);
}

void handleRotateStop() {
  // if (!server.hasArg("plain")) {
  //   server.send(400, "application/json", "{\"error\":\"Body missing\"}");
  //   return;
  // }
  Serial.println("handleRotateStop");
  server.send(200, "application/json", "{}");
  myStepper.stop();
}

void setup() {
  Serial.begin(115200);
  myStepper.setMaxSpeed(400.0);
  myStepper.setAcceleration(200.0);
  myStepper.setSpeed(300);

  // 1. Assign the hostname BEFORE starting Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostname);

  // 2. Connect to the Wi-Fi network
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    for (int i = 0; i < 5; i++) {
      delay(200);
      Serial.print(".");
    }
  }

  // 3. Print assigned network values locally
  Serial.println("\nConnected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("DHCP Hostname: ");
  Serial.println(WiFi.getHostname());

  // 4. Start mDNS to broadcast the hostname over the network
  if (!MDNS.begin(hostname)) {
    Serial.println("Error setting up mDNS responder!");
  } else {
    Serial.printf("mDNS responder started. Access device at: http://%s.local\n",
                  hostname);
  }

  // Define routing paths
  server.on("/", handleRoot);
  server.on("/rotatecw", HTTP_POST, handleRotateCW);
  server.on("/rotateccw", HTTP_POST, handleRotateCCW);
  server.on("/stop", HTTP_POST, handleRotateStop);
  server.onNotFound(handleNotFound);

  // Start the server
  server.begin();
  Serial.println("HTTP server started");
}

static bool stepperActive = false;
void loop() {
  server.handleClient();
  myStepper.run();
  if (myStepper.isRunning()) {
    stepperActive = true;
  } else if (stepperActive) {
    stepperActive = false;
    myStepper.disableOutputs();
  }
}
