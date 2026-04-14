/*
 * Climb-It Game
 * Team Lambda, 2026
 * Oliver Kettleson-Belinkie
 */

/* ---------- Includes ---------- */
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_FT6206.h>
#include <Wire.h>
#include <SPI.h>

/* ---------- Display Pins ---------- */
#define TFT_CS   10
#define TFT_DC    9
#define TFT_RST   8

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
Adafruit_FT6206  ts  = Adafruit_FT6206();

/* ---------- Sensor Pins ---------- */
#define PIN_FSR        A0
#define PIN_MIC        A1
#define PIN_BRUSH_HALL  2
#define PIN_VIBRATION   A2
#define PIN_LED_GREEN   7
#define PIN_LED_RED     6

/* ---------- Sensor Thresholds ---------- */
#define FSR_THRESHOLD  512
#define MIC_THRESHOLD  600

/* ---------- Timing ---------- */
#define POST_COMMAND_DELAY_MS 400
#define MAX_SCORE             99

/* ---------- Display Constants ---------- */
#define SCREEN_W      240
#define SCREEN_H      320
#define TOTAL_LEVELS  99
#define LEVELS_PER_BG 10
#define WALL_X        20
#define WALL_W        200
#define CX            120
#define LEDGE_SPACING 28
#define BOTTOM_LEDGE  300
#define SPR_X         (CX - 30)
#define SPR_Y_OFF     55
#define SPR_W         60
#define SPR_H         58
#define COL_CLIMB_L   (CX - 22)
#define COL_CLIMB_R   (CX + 22)
#define HAND_Y_OFF    46
#define COL_DECO_L    50
#define COL_DECO_R    190
#define DECO_COUNT    6

/* ---------- Wall Colors ---------- */
#define COL_WALL      tft.color565(220, 218, 212)
#define COL_WALL_DARK tft.color565(195, 192, 186)
#define COL_WALL_LITE tft.color565(235, 233, 228)
#define COL_FLOOR     tft.color565(80,  60,  40)
#define COL_FLOOR_LN  tft.color565(60,  45,  28)
#define COL_CEIL      tft.color565(50,  50,  60)
#define COL_CEIL_BEAM tft.color565(70,  70,  85)
#define COL_LEDGE     tft.color565(160, 155, 148)

/* ---------- Command Enum ---------- */
typedef enum {
  CMD_GRIP  = 0,
  CMD_BRUSH = 1,
  CMD_SHOUT = 2,
  CMD_NONE  = 3
} Command;

/* ---------- Game State ---------- */
static int     score           = 0;
static Command current_command = CMD_NONE;
static int     time_limit_ms   = 300000;
static int     game_active     = 0;

/* ---------- Display State ---------- */
int     currentLevel = 0;
int     bgLevel      = 0;

uint8_t climbColIdx[LEVELS_PER_BG + 1];
bool    climbLeft[LEVELS_PER_BG + 1];
int16_t decoLY[DECO_COUNT];
int16_t decoRY[DECO_COUNT];
uint8_t decoLCol[DECO_COUNT];
uint8_t decoRCol[DECO_COUNT];

/* ========================================
   LCD FUNCTIONS
   ======================================== */

uint16_t prng(uint16_t s) { return (s * 31337 + 1) & 0x7FFF; }

uint16_t holdColor(uint8_t i) {
  uint16_t cols[10];
  cols[0] = tft.color565(230,  40,  40);
  cols[1] = tft.color565( 40, 160,  40);
  cols[2] = tft.color565( 30,  80, 220);
  cols[3] = tft.color565(230, 180,   0);
  cols[4] = tft.color565(210,  60, 210);
  cols[5] = tft.color565(240, 120,   0);
  cols[6] = tft.color565(  0, 180, 200);
  cols[7] = tft.color565(220,  30, 120);
  cols[8] = tft.color565( 80, 200,  80);
  cols[9] = tft.color565(255, 255,  80);
  return cols[i % 10];
}

int climbHoldX(int i) { return (i % 2 == 0) ? COL_CLIMB_L : COL_CLIMB_R; }
int climbHoldY(int i) { return BOTTOM_LEDGE - (i * LEDGE_SPACING) - HAND_Y_OFF; }
int ledgeY(int i)     { return BOTTOM_LEDGE - (i * LEDGE_SPACING); }

bool readTouch(int &x, int &y) {
  if (!ts.touched()) return false;
  TS_Point p = ts.getPoint();
  x = map(p.x, 240, 0, 0, 240);
  y = map(p.y, 320, 0, 0, 320);
  return true;
}
bool touchInRegion(int rx, int ry, int rw, int rh) {
  int x, y;
  if (!readTouch(x, y)) return false;
  return x >= rx && x <= rx + rw && y >= ry && y <= ry + rh;
}

void generateHolds() {
  uint16_t s = (uint16_t)(bgLevel * 173 + 29);
  bool lastLeft = false; int sameCount = 0;
  for (int i = 0; i <= LEVELS_PER_BG; i++) {
    s = prng(s);
    bool goLeft;
    if (sameCount >= 2) { goLeft = !lastLeft; sameCount = 1; }
    else { goLeft = (s % 2 == 0); sameCount = (goLeft == lastLeft) ? sameCount + 1 : 1; }
    lastLeft = goLeft; climbLeft[i] = goLeft;
    s = prng(s); climbColIdx[i] = s % 10;
  }
  for (int i = 0; i < DECO_COUNT; i++) {
    int bandH = 260 / DECO_COUNT, baseY = 30 + i * bandH;
    s = prng(s); decoLY[i]   = baseY + (s % bandH);
    s = prng(s); decoLCol[i] = s % 10;
    s = prng(s); decoRY[i]   = baseY + (s % bandH);
    s = prng(s); decoRCol[i] = s % 10;
  }
}

void drawHold(int x, int y, uint16_t color) {
  tft.fillCircle(x, y, 6, color);
  tft.drawCircle(x, y, 7, tft.color565(80, 80, 80));
  tft.fillCircle(x - 2, y - 2, 2, ILI9341_WHITE);
  tft.drawPixel(x, y, tft.color565(200, 200, 200));
}

void drawAllHolds() {
  for (int i = 0; i <= LEVELS_PER_BG; i++) {
    int hy = climbHoldY(i);
    if (hy < 25 || hy > SCREEN_H) continue;
    drawHold(climbHoldX(i), hy, holdColor(climbColIdx[i]));
  }
  for (int i = 0; i < DECO_COUNT; i++) {
    drawHold(COL_DECO_L, decoLY[i], holdColor(decoLCol[i]));
    drawHold(COL_DECO_R, decoRY[i], holdColor(decoRCol[i]));
  }
}

void drawWallPanel(int x, int y, int w, int h) {
  tft.fillRect(x, y, w, h, COL_WALL);
  for (int px = x; px < x + w; px += 40) {
    tft.drawFastVLine(px,     y, h, COL_WALL_DARK);
    tft.drawFastVLine(px + 1, y, h, COL_WALL_LITE);
  }
  for (int py = y; py < y + h; py += 32) {
    tft.drawFastHLine(x, py,     w, COL_WALL_DARK);
    tft.drawFastHLine(x, py + 1, w, COL_WALL_LITE);
  }
  for (int px = x + 40; px < x + w; px += 40)
    for (int py = y + 32; py < y + h; py += 32) {
      tft.fillCircle(px, py, 2, COL_WALL_DARK);
      tft.drawPixel(px - 1, py - 1, COL_WALL_LITE);
    }
}

void drawWallBorders() {
  tft.fillRect(0, 0, WALL_X, SCREEN_H, tft.color565(100, 95, 90));
  tft.drawFastVLine(WALL_X - 1, 0, SCREEN_H, tft.color565(60, 55, 50));
  tft.fillRect(WALL_X + WALL_W, 0, SCREEN_W - WALL_X - WALL_W, SCREEN_H, tft.color565(100, 95, 90));
  tft.drawFastVLine(WALL_X + WALL_W, 0, SCREEN_H, tft.color565(60, 55, 50));
}

void drawFloor() {
  int fy = 285;
  tft.fillRect(0, fy, SCREEN_W, SCREEN_H - fy, COL_FLOOR);
  for (int x = 0; x < SCREEN_W; x += 20)
    tft.drawFastVLine(x, fy, SCREEN_H - fy, COL_FLOOR_LN);
  tft.drawFastHLine(0, fy,     SCREEN_W, tft.color565(120, 90, 55));
  tft.drawFastHLine(0, fy + 1, SCREEN_W, tft.color565(60,  45, 28));
  tft.setTextSize(1);
  tft.setTextColor(tft.color565(120, 90, 55));
  tft.setCursor(82, fy + 8);
  tft.print("CRASH PAD");
}

void drawCeiling() {
  tft.fillRect(0, 25, SCREEN_W, 18, COL_CEIL);
  for (int x = 0; x < SCREEN_W; x += 50)
    tft.fillRect(x, 25, 10, 18, COL_CEIL_BEAM);
  tft.drawFastHLine(0, 43, SCREEN_W, tft.color565(30, 30, 40));
  for (int x = 25; x < SCREEN_W; x += 50) {
    tft.drawFastVLine(x, 43, 8, tft.color565(180, 180, 190));
    tft.fillCircle(x, 52, 3, tft.color565(160, 160, 170));
    tft.drawPixel(x - 1, 50, ILI9341_WHITE);
  }
}

void drawBackground() {
  tft.fillScreen(COL_WALL);
  drawWallPanel(WALL_X, 25, WALL_W, SCREEN_H - 25);
  drawWallBorders();
  if (currentLevel < LEVELS_PER_BG)  drawFloor();
  else if (currentLevel >= 90)       drawCeiling();
  generateHolds();
  drawAllHolds();
}

void drawUI() {
  tft.fillRect(0, 0, SCREEN_W, 24, ILI9341_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(4, 4);
  tft.print("Lvl ");
  tft.print(currentLevel);
  tft.print("/");
  tft.print(TOTAL_LEVELS);
  int barW = map(currentLevel, 0, TOTAL_LEVELS, 0, 120);
  tft.fillRect(55, 5, 120, 6, tft.color565(40, 40, 40));
  tft.fillRect(55, 5, barW,  6, ILI9341_GREEN);
  // show current command top right
  tft.setTextColor(ILI9341_YELLOW);
  tft.setCursor(150, 4);
  switch (current_command) {
    case CMD_GRIP:  tft.print("GRIP! "); break;
    case CMD_BRUSH: tft.print("BRUSH!"); break;
    case CMD_SHOUT: tft.print("SHOUT!"); break;
    default:        tft.print("      "); break;
  }
}

void repairWall(int top, int bot) {
  tft.fillRect(SPR_X, top, SPR_W, bot - top, COL_WALL);
  for (int px = WALL_X; px < WALL_X + WALL_W; px += 40) {
    if (px >= SPR_X && px <= SPR_X + SPR_W) {
      tft.drawFastVLine(px,     top, bot - top, COL_WALL_DARK);
      tft.drawFastVLine(px + 1, top, bot - top, COL_WALL_LITE);
    }
  }
  for (int py = 0; py < SCREEN_H; py += 32) {
    if (py >= top && py <= bot) {
      tft.drawFastHLine(SPR_X, py,     SPR_W, COL_WALL_DARK);
      tft.drawFastHLine(SPR_X, py + 1, SPR_W, COL_WALL_LITE);
    }
  }
}

void eraseSprite(int ly) {
  int top = max(25, ly - SPR_Y_OFF);
  int bot = min(SCREEN_H, ly + 5);
  repairWall(top, bot);
  if (currentLevel < LEVELS_PER_BG && bot >= 285) drawFloor();
  int local = currentLevel % LEVELS_PER_BG;
  for (int i = local; i >= max(0, local - 3); i--)
    drawHold(climbHoldX(i), climbHoldY(i), holdColor(climbColIdx[i]));
}

void drawSprite(int ly, int pose) {
  uint16_t skin  = tft.color565(255, 200, 150);
  uint16_t shirt = tft.color565(200,  50,  50);
  uint16_t pants = tft.color565( 50,  50, 140);
  uint16_t shoe  = tft.color565( 30,  20,  10);
  uint16_t helm  = tft.color565(255, 200,   0);
  int y = ly;
  tft.fillRect(CX - 10, y - 4,  9, 5, shoe);
  tft.fillRect(CX + 2,  y - 4,  9, 5, shoe);
  tft.fillRect(CX - 8,  y - 17, 6, 13, pants);
  tft.fillRect(CX + 2,  y - 17, 6, 13, pants);
  tft.fillRect(CX - 9,  y - 31, 19, 15, shirt);
  tft.fillCircle(CX, y - 41, 9, skin);
  tft.fillRect(CX - 8, y - 51, 17, 8, helm);
  tft.fillRect(CX - 9, y - 45, 19, 4, helm);
  if (pose == 0) {
    tft.fillRect(CX - 19, y - 29, 10, 4, shirt);
    tft.fillRect(CX + 9,  y - 29, 10, 4, shirt);
    tft.fillRect(CX - 22, y - 29,  4, 4, skin);
    tft.fillRect(CX + 19, y - 29,  4, 4, skin);
  } else {
    bool reachLeft = (currentLevel % 2 == 0);
    int hx = reachLeft ? COL_CLIMB_L : COL_CLIMB_R;
    int hy = y - HAND_Y_OFF;
    if (reachLeft) {
      tft.drawLine(CX - 8, y - 28, hx, hy, shirt);
      tft.drawLine(CX - 7, y - 28, hx, hy, shirt);
      tft.drawLine(CX - 6, y - 28, hx, hy, shirt);
      tft.fillCircle(hx, hy, 3, skin);
      tft.fillRect(CX + 9,  y - 29, 10, 4, shirt);
      tft.fillRect(CX + 19, y - 29,  4, 4, skin);
    } else {
      tft.drawLine(CX + 8,  y - 28, hx, hy, shirt);
      tft.drawLine(CX + 9,  y - 28, hx, hy, shirt);
      tft.drawLine(CX + 10, y - 28, hx, hy, shirt);
      tft.fillCircle(hx, hy, 3, skin);
      tft.fillRect(CX - 19, y - 29, 10, 4, shirt);
      tft.fillRect(CX - 22, y - 29,  4, 4, skin);
    }
  }
}

void drawClimberFallen(int gx, int gy) {
  uint16_t skin  = tft.color565(255, 200, 150);
  uint16_t shirt = tft.color565(200,  50,  50);
  uint16_t pants = tft.color565( 50,  50, 140);
  uint16_t shoe  = tft.color565( 30,  20,  10);
  uint16_t helm  = tft.color565(255, 200,   0);
  tft.fillRect(gx, gy - 5, 36, 10, shirt);
  tft.fillCircle(gx + 44, gy, 9, skin);
  tft.fillRect(gx + 38, gy - 16, 17, 6, helm);
  tft.fillRect(gx - 4, gy - 14, 6, 14, shirt);
  tft.fillRect(gx - 4, gy - 18, 6, 6, skin);
  tft.fillRect(gx + 20, gy + 6, 6, 14, shirt);
  tft.fillRect(gx + 20, gy + 20, 6, 6, skin);
  tft.fillRect(gx - 18, gy - 4, 20, 8, pants);
  tft.fillRect(gx - 18, gy - 4, 8, 18, pants);
  tft.fillRect(gx - 18, gy + 14, 16, 6, shoe);
  tft.fillRect(gx - 26, gy - 4, 8, 6, shoe);
}

/* ========================================
   LCD INTERFACE FUNCTIONS
   called by game logic below
   ======================================== */

void lcd_setup() {
  Wire.begin();
  Wire.setTimeout(10);
  ts.begin(40);
  tft.begin(8000000);
  tft.setRotation(0);
  tft.fillScreen(ILI9341_BLACK);
  delay(100);
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextWrap(false);
  tft.cp437(true);
}

void lcd_start() {
  currentLevel = 0;
  bgLevel      = 0;
  // show start screen and wait for touch
  tft.setTextWrap(false);
  tft.fillScreen(tft.color565(10, 80, 160));
  for (int x = 0; x < SCREEN_W; x++) {
    int h = max(80, 180 - abs(x - 120));
    tft.drawFastVLine(x, h, SCREEN_H - h, tft.color565(90, 85, 80));
  }
  for (int x = 90; x < 150; x++) {
    int h   = max(80, 180 - abs(x - 120));
    int snH = max(0, 10 - abs(x - 120) / 2);
    tft.drawFastVLine(x, h, snH, ILI9341_WHITE);
  }
  tft.fillRect(CX - 10, 236,  9, 5, tft.color565(30,  20,  10));
  tft.fillRect(CX + 2,  236,  9, 5, tft.color565(30,  20,  10));
  tft.fillRect(CX - 8,  223,  6, 13, tft.color565(50,  50, 140));
  tft.fillRect(CX + 2,  223,  6, 13, tft.color565(50,  50, 140));
  tft.fillRect(CX - 9,  209, 19, 15, tft.color565(200,  50,  50));
  tft.fillCircle(CX, 199, 9, tft.color565(255, 200, 150));
  tft.fillRect(CX - 8,  189, 17, 8, tft.color565(255, 200,   0));
  tft.fillRect(CX - 9,  195, 19, 4, tft.color565(255, 200,   0));
  tft.fillRect(CX - 19, 211, 10, 4, tft.color565(200,  50,  50));
  tft.fillRect(CX + 9,  211, 10, 4, tft.color565(200,  50,  50));
  tft.fillRect(CX - 22, 211,  4, 4, tft.color565(255, 200, 150));
  tft.fillRect(CX + 19, 211,  4, 4, tft.color565(255, 200, 150));
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(66, 155);
  tft.print("Climb-It!");
  tft.fillRoundRect(60, 255, 120, 45, 8, tft.color565(50, 180, 50));
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_BLACK);
  tft.setCursor(90, 270);
  tft.print("START");
  while (!touchInRegion(60, 255, 120, 45));
  delay(300);
  drawBackground();
  drawUI();
  drawSprite(ledgeY(0), 0);
}

void lcd_climb_step() {
  int local = currentLevel % LEVELS_PER_BG;
  int sprY  = ledgeY(local);
  eraseSprite(sprY);
  drawSprite(sprY, 1);
  delay(100);
  currentLevel++;
  local = currentLevel % LEVELS_PER_BG;
  if (local == 0) {
    bgLevel++;
    drawBackground();
  } else {
    eraseSprite(sprY);
    drawSprite(ledgeY(local), 0);
  }
  drawUI();
}

void lcd_fail() {
  tft.setTextWrap(false);
  tft.fillScreen(tft.color565(180, 0, 0));
  delay(200);
  drawWallPanel(WALL_X, 25, WALL_W, SCREEN_H - 25);
  drawWallBorders();
  drawFloor();
  drawAllHolds();
  drawClimberFallen(70, 282);
  tft.setTextSize(3);
  tft.setTextColor(ILI9341_RED);
  tft.setCursor(30, 60);
  tft.print("YOU FELL!");
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(36, 130);
  tft.print("Reached level:");
  tft.setTextSize(3);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setCursor(90, 158);
  tft.print(score);
  // no retry button — game logic calls start_game() which calls lcd_start()
  delay(3000);
}

void lcd_win() {
  tft.setTextWrap(false);
  tft.fillScreen(tft.color565(5, 20, 80));
  for (int i = 0; i < 30; i++)
    tft.fillCircle((i * 47 + 13) % 230 + 5, (i * 31 + 7) % 140 + 5, 1, ILI9341_WHITE);
  tft.setTextSize(3);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setCursor(78, 80);
  tft.print("SUMMIT!");
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(60, 140);
  tft.print("You reached the top!");
  delay(3000);
  // game logic calls start_game() after lcd_win()
}

/* ========================================
   GAME LOGIC (their code, unchanged)
   ======================================== */

void init_system() {
  init_sensors();
  init_audio();
  init_leds();
  init_display();
  randomSeed(analogRead(A5));
}

void init_display() { lcd_setup(); }

void init_sensors() {
  pinMode(PIN_FSR, INPUT);
  pinMode(PIN_MIC, INPUT);
  pinMode(PIN_BRUSH_HALL, INPUT_PULLUP);
  pinMode(PIN_VIBRATION, INPUT);
}

void init_audio() {}

void init_leds() {
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED,   OUTPUT);
  digitalWrite(PIN_LED_GREEN, LOW);
  digitalWrite(PIN_LED_RED,   LOW);
}

int read_grip_action() { return analogRead(PIN_FSR) >= FSR_THRESHOLD ? 1 : 0; }

int read_brush_action() {
  return (digitalRead(PIN_BRUSH_HALL) == LOW && digitalRead(PIN_VIBRATION) == HIGH) ? 1 : 0;
}

int read_mic_action() { return analogRead(PIN_MIC) >= MIC_THRESHOLD ? 1 : 0; }

Command detect_player_action() {
  if (read_grip_action())  return CMD_GRIP;
  if (read_brush_action()) return CMD_BRUSH;
  if (read_mic_action())   return CMD_SHOUT;
  return CMD_NONE;
}

void set_led_color(int success) {
  digitalWrite(PIN_LED_GREEN, success ? HIGH : LOW);
  digitalWrite(PIN_LED_RED,   success ? LOW  : HIGH);
}
void clear_leds() {
  digitalWrite(PIN_LED_GREEN, LOW);
  digitalWrite(PIN_LED_RED,   LOW);
}

Command pick_next_command() { return (Command)random(0, 3); }

void set_difficulty() {
  if (score > 0 && score % 5 == 0) {
    time_limit_ms -= 200;
    if (time_limit_ms < 1000) time_limit_ms = 1000;
  }
}

void update_display() {
  if (!game_active) return;
  drawUI();          // refresh command shown on screen
  lcd_climb_step();  // animate the climb
}

void play_command_audio() {}
void play_lose_audio()    {}
void play_win_audio()     {}

void start_game() {
  score           = 0;
  current_command = CMD_NONE;
  time_limit_ms   = 3000;
  game_active     = 1;
  clear_leds();
  lcd_start();
}

void end_game() {
  game_active = 0;
  play_lose_audio();
  set_led_color(0);
  lcd_fail();
  start_game();
}

void game_loop() {
  static unsigned long command_start_time = 0;
  static int waiting_for_input = 0;
  Command player_action;

  if (!waiting_for_input) {
    clear_leds();
    current_command    = pick_next_command();
    command_start_time = millis();
    waiting_for_input  = 1;
    play_command_audio();
    drawUI();  // show new command
  }

  if ((millis() - command_start_time) >= (unsigned long)time_limit_ms) {
    waiting_for_input = 0;
    end_game();
    return;
  }

  player_action = detect_player_action();
  if (player_action != CMD_NONE) {
    if (player_action == current_command) {
      score++;
      set_led_color(1);
      update_display();
      if (score >= MAX_SCORE) {
        lcd_win();
        waiting_for_input = 0;
        start_game();
        return;
      }
      set_difficulty();
      waiting_for_input = 0;
      delay(POST_COMMAND_DELAY_MS);
      clear_leds();
    } else {
      waiting_for_input = 0;
      end_game();
    }
  }
}

/* ========== Arduino Entry Points ========== */
void setup() {
  init_system();
  start_game();
}

void loop() {
  if (game_active) game_loop();
}
