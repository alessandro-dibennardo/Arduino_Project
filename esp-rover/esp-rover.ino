#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#define PIN_L_F D1
#define PIN_L_B D2
#define PIN_R_F D3
#define PIN_R_B D4

// Hotspot credentials
const char* ssid = "add_your_hotspot_name";
const char* password = "add_your_hotspot_password";

ESP8266WebServer server(80);

// Web page with touch joystick
String htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
<style>
  body { 
    text-align: center; font-family: Arial, sans-serif; background: #222; color: #fff; 
    margin: 0; padding: 0; overflow: hidden; touch-action: none; /* Disable zoom and scroll */
  }
  h2 { margin-top: 40px; color: #bbb; }
  #joystickZone { 
    width: 100vw; height: 60vh; display: flex; 
    justify-content: center; align-items: center; margin-top: 20px;
  }
  canvas { 
    background: #333; border-radius: 50%; 
    box-shadow: inset 0 0 20px rgba(0,0,0,0.8), 0 0 10px rgba(0,0,0,0.5); 
  }
  #status { font-size: 24px; font-weight: bold; margin-top: 20px; color: #007BFF; }
</style>
</head>
<body>
  <h2>Rover Joystick</h2>
  <div id="joystickZone">
    <canvas id="joystick" width="260" height="260"></canvas>
  </div>
  <p id="status">STOP</p>

  <script>
    const canvas = document.getElementById('joystick');
    const ctx = canvas.getContext('2d');
    const statusText = document.getElementById('status');
    
    let centerX = canvas.width / 2;
    let centerY = canvas.height / 2;
    let thumbRadius = 50;
    let maxDist = 80; // Maximum distance of the stick from the center
    
    let thumbX = centerX;
    let thumbY = centerY;
    let isDrawing = false;
    let currentCommand = 'stop';

    function draw() {
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      
      // Draw joystick base
      ctx.beginPath();
      ctx.arc(centerX, centerY, maxDist, 0, Math.PI * 2);
      ctx.fillStyle = '#444';
      ctx.fill();
      ctx.lineWidth = 2;
      ctx.strokeStyle = '#222';
      ctx.stroke();
      
      // Draw blue stick
      ctx.beginPath();
      ctx.arc(thumbX, thumbY, thumbRadius, 0, Math.PI * 2);
      ctx.fillStyle = '#007BFF';
      ctx.fill();
    }

    function sendCmd(cmd) {
      // Send the command ONLY if it differs from the previous one, to avoid flooding/crashing the ESP8266
      if (currentCommand !== cmd) {
        currentCommand = cmd;
        fetch('/' + cmd);
        statusText.innerText = cmd.toUpperCase();
      }
    }

    function handleMove(x, y) {
      let dx = x - centerX;
      let dy = y - centerY;
      let distance = Math.sqrt(dx * dx + dy * dy);

      // Keep the stick movement inside the circle
      if (distance > maxDist) {
        thumbX = centerX + (dx * maxDist / distance);
        thumbY = centerY + (dy * maxDist / distance);
      } else {
        thumbX = x;
        thumbY = y;
      }

      // Directional logic
      if (distance < 30) {
        sendCmd('stop');
      } else {
        let angle = Math.atan2(dy, dx) * 180 / Math.PI;
        
        // Compute direction based on tilt angle
        if (angle > -45 && angle <= 45) sendCmd('right');
        else if (angle > 45 && angle <= 135) sendCmd('back');
        else if (angle < -45 && angle >= -135) sendCmd('go');
        else sendCmd('left');
      }
      draw();
    }

    // Touch events
    canvas.addEventListener('touchstart', e => { 
      isDrawing = true; 
      let rect = canvas.getBoundingClientRect();
      handleMove(e.touches[0].clientX - rect.left, e.touches[0].clientY - rect.top); 
    });
    canvas.addEventListener('touchmove', e => { 
      if (isDrawing) { 
        e.preventDefault();
        let rect = canvas.getBoundingClientRect();
        handleMove(e.touches[0].clientX - rect.left, e.touches[0].clientY - rect.top); 
      } 
    }, {passive: false});
    canvas.addEventListener('touchend', e => { 
      isDrawing = false; 
      thumbX = centerX; thumbY = centerY;
      draw(); 
      sendCmd('stop'); 
    });
    
    // Mouse events
    canvas.addEventListener('mousedown', e => { isDrawing = true; handleMove(e.offsetX, e.offsetY); });
    canvas.addEventListener('mousemove', e => { if (isDrawing) handleMove(e.offsetX, e.offsetY); });
    window.addEventListener('mouseup', e => { 
      if(isDrawing) {
        isDrawing = false; thumbX = centerX; thumbY = centerY; draw(); sendCmd('stop'); 
      }
    });

    draw();
  </script>
</body>
</html>
)rawliteral";

// Movement logic
void stop() {
  digitalWrite(PIN_L_F, LOW); digitalWrite(PIN_L_B, LOW);
  digitalWrite(PIN_R_F, LOW); digitalWrite(PIN_R_B, LOW);
  server.send(200, "text/plain", "Stop");
}
void go() {
  digitalWrite(PIN_L_F, HIGH); digitalWrite(PIN_L_B, LOW);
  digitalWrite(PIN_R_F, HIGH); digitalWrite(PIN_R_B, LOW);
  server.send(200, "text/plain", "Go");
}
void back() {
  digitalWrite(PIN_L_F, LOW); digitalWrite(PIN_L_B, HIGH);
  digitalWrite(PIN_R_F, LOW); digitalWrite(PIN_R_B, HIGH);
  server.send(200, "text/plain", "Back");
}
void turnLeft() {
  digitalWrite(PIN_L_F, LOW); digitalWrite(PIN_L_B, HIGH);
  digitalWrite(PIN_R_F, HIGH); digitalWrite(PIN_R_B, LOW);
  server.send(200, "text/plain", "Left");
}
void turnRight() {
  digitalWrite(PIN_L_F, HIGH); digitalWrite(PIN_L_B, LOW);
  digitalWrite(PIN_R_F, LOW); digitalWrite(PIN_R_B, HIGH);
  server.send(200, "text/plain", "Right");
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_L_F, OUTPUT); pinMode(PIN_L_B, OUTPUT);
  pinMode(PIN_R_F, OUTPUT); pinMode(PIN_R_B, OUTPUT);
  
  stop();

  // Disable Wi-Fi power saving to eliminate lag
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  
  Serial.println("\nConnected!");
  Serial.print("IP address of joystick: ");
  Serial.println(WiFi.localIP());

  // Routes now match the commands sent by the JavaScript joystick
  server.on("/", []() { server.send(200, "text/html", htmlPage); });
  server.on("/go", go);
  server.on("/back", back);
  server.on("/left", turnLeft);
  server.on("/right", turnRight);
  server.on("/stop", stop);

  server.begin();
}

void loop() {
  server.handleClient();
}