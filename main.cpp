#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <GxEPD2_3C.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeSans9pt7b.h>

// =================== WiFi ===================
const char* ssid     = "TEData_BE3DE0";
const char* password = "h9f32768";

// =================== API ====================
const char* apiUrl = "https://asmaa-eg.github.io/room-schedule/schedule.json";

// =================== E-paper Pins ====================
#define EPD_SS   19
#define EPD_DC   5
#define EPD_RST  2
#define EPD_BUSY 4

#define MAX_DISPLAY_BUFFER_SIZE 800
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8))

GxEPD2_3C<GxEPD2_290_C90c, MAX_HEIGHT(GxEPD2_290_C90c)> display(GxEPD2_290_C90c(EPD_SS, EPD_DC, EPD_RST, EPD_BUSY));

// =================== Global Variables ===================
String lastToken = "";
int currentLectureIndex = 0;
// Last header bottom Y (set by drawHeader) so content can start below it
int headerBottomY = 0;

// =================== Function Prototypes ===================
void drawSchedule(String dr, String subj, String timeRange);
void drawMeeting(String subj, String timeRange);
void drawTalk(String eventName, String speaker, String timeRange);
void drawSeats(String name, String country, String title, String department);
void drawHeader(const char* topLine, const char* subLine);

// =================== Setup ===================
void setup() {
  Serial.begin(115200);
  Serial.println("Booting...");

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  display.init();
  display.setRotation(1);
  display.setFont(&FreeSans9pt7b);
  display.setTextColor(GxEPD_BLACK);

  Serial.println("Display initialized.");
}

// =================== Main Loop ===================
void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(apiUrl);
    int httpCode = http.GET();

    if (httpCode == 200) {
      String payload = http.getString();
      Serial.println(payload);

      DynamicJsonDocument doc(4096);
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        String newToken   = doc["refresh_token"] | "";
        String formatType = doc["format_token"]  | "";

        if (newToken != lastToken) {
          lastToken = newToken;
          Serial.printf("New refresh detected, format: %s\n", formatType.c_str());

          if (formatType == "schedule") {
            JsonArray lectures = doc["schedule"].as<JsonArray>();
            if (lectures.size() > 0) {
              if (currentLectureIndex >= lectures.size()) currentLectureIndex = 0;

              JsonObject lecture = lectures[currentLectureIndex];
              drawSchedule(
                "Dr: " + String(lecture["dr"].as<const char*>()),
                "Subj: " + String(lecture["subject"].as<const char*>()),
                String(lecture["from"].as<const char*>()) + " - " + String(lecture["to"].as<const char*>())
              );

              currentLectureIndex++;
            }
          }
          else if (formatType == "seats") {
            JsonObject seats = doc["seats"].as<JsonObject>();
            drawSeats(
              String(seats["name"].as<const char*>()),
              String(seats["country"].as<const char*>()),
              String(seats["title"].as<const char*>()),
              String(seats["department"].as<const char*>()));
          }
          else if (formatType == "talk") {
            JsonObject talk = doc["talk"].as<JsonObject>();
            // Determine event name: prefer "event" field, fall back to speaker, then to "Talk"
            const char* ev_c = talk["event"] | "";
            String eventName = String(ev_c);
            if (eventName.length() == 0) {
              const char* sp_c = talk["speaker"] | "";
              eventName = String(sp_c);
              if (eventName.length() == 0) eventName = String("Talk");
            }

            drawTalk(
              eventName,
              String(talk["speaker"].as<const char*>()),
              String(talk["from"].as<const char*>()) + " - " + String(talk["to"].as<const char*>()));
          }
          else if (formatType == "meeting") {
            JsonObject meeting = doc["meeting"].as<JsonObject>();
            drawMeeting(
              "Subject: " + String(meeting["subject"].as<const char*>()),
              String(meeting["from"].as<const char*>()) + " - " + String(meeting["to"].as<const char*>())
            );
          }
        } else {
          Serial.println("No refresh needed.");
        }
      } else {
        Serial.println("JSON parse failed!");
      }
    } else {
      Serial.printf("HTTP GET failed, code: %d\n", httpCode);
    }
    http.end();
  }
  delay(10000);
}

// =================== Draw Functions ===================
// Helper: draw a two-line centered header at the very top using getTextBounds
void drawHeader(const char* topLine, const char* subLine) {
  // Use current font
  int16_t x1, y1;
  uint16_t w1, h1, w2, h2;

  // First line bounds
  display.getTextBounds(topLine, 0, 0, &x1, &y1, &w1, &h1);
  // Second line bounds
  display.getTextBounds(subLine, 0, 0, &x1, &y1, &w2, &h2);

  // Calculate centered x positions
  int16_t centerX = display.width() / 2;
  int16_t topX = centerX - (w1 / 2);
  int16_t subX = centerX - (w2 / 2);

  // Draw top line (red) at the very top (y = h1) leaving a small margin
  display.setTextColor(GxEPD_RED);
  int16_t topY = 6 + h1; // top padding + top line height
  display.setCursor(topX, topY);
  display.print(topLine);

  // Draw sub line (black) just under the top line
  display.setTextColor(GxEPD_RED);
  int16_t subY = topY + h2 + 6; // gap between lines
  display.setCursor(subX, subY);
  display.print(subLine);

  // Record bottom of header so callers can start content below it
  headerBottomY = subY + 4; // small padding after sub line
}
void drawSchedule(String dr, String subj, String timeRange) {
  display.setFont(&FreeSans9pt7b);
  display.firstPage();
  do {
    drawHeader("AASTMT", "College Of Artificial Intelligence");
    // compute a line gap from font height to avoid overlap
    int16_t bx, by; uint16_t bw, bh;
    display.getTextBounds("A", 0, 0, &bx, &by, &bw, &bh);
    int lineGap = bh + 8;
    int y = headerBottomY + lineGap;
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(10, y);
    display.print(dr);
    display.setCursor(10, y + lineGap);
    display.print(subj);
    display.setCursor(10, y + lineGap * 2);
    display.print("Time: " + timeRange);
  } while (display.nextPage());
}

void drawMeeting(String subj, String timeRange) {
  display.setFont(&FreeSans9pt7b);
  display.firstPage();
  do {
    drawHeader("AASTMT", "College Of Artificial Intelligence");
    int16_t bx, by; uint16_t bw, bh;
    display.getTextBounds("A", 0, 0, &bx, &by, &bw, &bh);
    int lineGap = bh + 8;
    int y = headerBottomY + lineGap;
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(10, y);
    display.print("Meeting");
    display.setCursor(10, y + lineGap);
    display.print(subj);
    display.setCursor(10, y + lineGap * 2);
    display.print("Time: " + timeRange);
  } while (display.nextPage());
}

void drawTalk(String eventName, String speaker, String timeRange) {
  display.setFont(&FreeSans9pt7b);
  display.firstPage();
  do {
    drawHeader("AASTMT", "College Of Artificial Intelligence");
    int16_t bx, by; uint16_t bw, bh;
    display.getTextBounds("A", 0, 0, &bx, &by, &bw, &bh);
    int lineGap = bh + 8;
    int y = headerBottomY + lineGap;
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(10, y);
    display.print(eventName);
    display.setCursor(10, y + lineGap);
    display.print("Speaker: " + speaker);
    display.setCursor(10, y + lineGap * 2);
    display.print("Time: " + timeRange);
  } while (display.nextPage());
}

void drawSeats(String name, String country, String title, String department) {
  display.setFont(&FreeSans9pt7b);
  display.firstPage();
  do {
    drawHeader("AASTMT", "College Of Artificial Intelligence");

    int16_t bx, by; uint16_t bw, bh;
    display.getTextBounds("A", 0, 0, &bx, &by, &bw, &bh);
    int lineGap = bh + 8;
    int y = headerBottomY + lineGap;
    display.setTextColor(GxEPD_BLACK);

    // Name
    display.setCursor(10, y);
    display.print(name);

    // Job: combine title + department on the same line
    String jobLine = title;
    if (department.length() > 0) {
      if (jobLine.length() > 0) jobLine += " - ";
      jobLine += department;
    }
    display.setCursor(10, y + lineGap);
    display.print(jobLine);

    // Country
    display.setCursor(10, y + lineGap * 2);
    display.print(country);

  } while (display.nextPage());
}
