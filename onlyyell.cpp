/*
 * Climb-It Game — Scream-Only Build
 * Team Lambda, 2026
 * Oliver Kettleson-Belinkie
 *
 * LCD display: ILI9341 TFT + FT6206 touch
 * DFPlayer busy pin used to gate audio playback
 *
 * This build implements ONLY the shout/scream command.
 * Grip (FSR) and brush (Hall + vibration) are excluded.
 *
 * Audio is fully stubbed — command shown on screen only.
 * Restore audio function bodies once DFPlayer is working.
 */


/* ---------- Includes ---------- */

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_FT6206.h>
#include <Wire.h>
#include <SPI.h>


/* ---------- Pin Definitions ---------- */

#define PIN_MIC         A0

#define PIN_DF_TX       4
#define PIN_DF_RX       5
#define PIN_DF_BUSY     3

#define PIN_LED_GREEN   7
#define PIN_LED_RED     6

#define TFT_RST         8
#define TFT_DC          9
#define TFT_CS          10


/* ---------- Sensor Thresholds ---------- */

/*
 * MIC_SAMPLE_MS   — how long (ms) to collect samples for one peak-to-peak read.
 *                   50 ms captures several cycles of a shout without feeling laggy.
 * MIC_THRESHOLD   — minimum peak-to-peak ADC swing (0-1023) to count as a shout.
 *                   Start around 400 and tune up/down based on your mic and room.
 */
#define MIC_SAMPLE_MS    50
#define MIC_THRESHOLD   400


/* ---------- Timing ---------- */

#define POST_COMMAND_DELAY_MS    400
#define SHORT_DEBOUNCE_MS        300
#define MAX_SCORE                 99
#define DRAIN_HARD_TIMEOUT_MS   2000


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

/*
 * SPR_X, SPR_W, SPR_H — sprite bounding box origin and size.
 *
 * FIX: The original SPR_Y_OFF (55) only covered the body. The reaching-arm
 * pose extends a further HAND_Y_OFF (46) pixels above the feet, so the full
 * sprite height from feet to fingertip is 4 (shoe below feet) + 46 = 50 px
 * above ly. We set SPR_Y_OFF to 58 to fully cover the tallest pixel drawn
 * (helm top at ly-51, plus a 7-pixel margin) so eraseSprite() wipes the
 * entire previous frame including the extended arm, preventing ghost images.
 */
#define SPR_X         (CX - 30)
#define SPR_Y_OFF     58        /* was 55 — now covers helm+arm fully */
#define SPR_W         60
#define SPR_H         58

#define COL_CLIMB_L  (CX - 22)
#define COL_CLIMB_R  (CX + 22)
#define HAND_Y_OFF    46

#define COL_DECO_L   50
#define COL_DECO_R   190
#define DECO_COUNT   6

#define COL_WALL      tft.color565(220, 218, 212)
#define COL_WALL_DARK tft.color565(195, 192, 186)
#define COL_WALL_LITE tft.color565(235, 233, 228)
#define COL_FLOOR     tft.color565(80,  60,  40)
#define COL_FLOOR_LN  tft.color565(60,  45,  28)
#define COL_CEIL      tft.color565(50,  50,  60)
#define COL_CEIL_BEAM tft.color565(70,  70,  85)

/* Command banner — sits in wall area just below the UI bar */
#define CMD_BANNER_Y  27
#define CMD_BANNER_H  20


/* ---------- Game State ---------- */

static int           score              = 0;
static int           time_limit_ms      = 3000;
static int           game_active        = 0;
static int           waiting_for_input  = 0;
static unsigned long command_start_time = 0;


/* ---------- Display State ---------- */

int     currentLevel = 0;
int     bgLevel      = 0;

uint8_t climbColIdx[LEVELS_PER_BG + 1];
bool    climbLeft[LEVELS_PER_BG + 1];

int16_t decoLY[DECO_COUNT];
int16_t decoRY[DECO_COUNT];
uint8_t decoLCol[DECO_COUNT];
uint8_t decoRCol[DECO_COUNT];


/* ---------- Peripheral Objects ---------- */

SoftwareSerial      dfSerial(PIN_DF_RX, PIN_DF_TX);
DFRobotDFPlayerMini dfPlayer;
Adafruit_ILI9341    tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
Adafruit_FT6206     ts  = Adafruit_FT6206();


/* =====================================================================
 * FORWARD DECLARATIONS
 * ===================================================================== */

void init_system();
void init_display();
void init_sensors();
void init_audio();
void init_leds();

int  read_mic_peak_to_peak();
int  read_mic_action();
void drain_sensors();

void play_command_audio();
void play_good_job_audio();
void play_lose_audio();
void play_win_audio();

void set_led_color(int success);
void clear_leds();

void set_difficulty();
int  detect_player_action();
void update_display();
void start_game();
void end_game();
void game_loop();

void lcd_setup();
void lcd_start();
void lcd_climb_step();
void lcd_fail();
void lcd_win();

uint16_t prng(uint16_t s);
uint16_t holdColor(uint8_t i);
bool     readTouch(int &x, int &y);
bool     touchInRegion(int rx, int ry, int rw, int rh);
int      ledgeY(int localLvl);
int      climbHoldX(int i);
int      climbHoldY(int i);
void     generateHolds();
void     drawHold(int x, int y, uint16_t color);
void     drawAllHolds();
void     drawWallPanel(int x, int y, int w, int h);
void     drawWallBorders();
void     drawFloor();
void     drawCeiling();
void     drawBackground();
void     drawResetButton();
void     drawUI();
void     drawCommandPrompt();
void     clearCommandPrompt();
void     repairWall(int top, int bot);
void     eraseSprite(int ly);
void     drawSprite(int ly, int pose);
void     drawClimberFallen(int gx, int gy);
void     showStartScreen();
void     showWinScreen();
void     doFall();
void     doReset();


/* =====================================================================
 * INITIALIZATION
 * ===================================================================== */

void init_system() {
    init_sensors();
    init_audio();
    init_leds();
    init_display();
    randomSeed(analogRead(A5));
}

void init_display() { lcd_setup(); }

void init_sensors() { pinMode(PIN_MIC, INPUT); }

void init_audio() {
    /* Pin reserved for when DFPlayer is restored. No blocking waits. */
    pinMode(PIN_DF_BUSY, INPUT);
}

void init_leds() {
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_RED,   OUTPUT);
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED,   LOW);
}


/* =====================================================================
 * SENSORS
 * ===================================================================== */

/*
 * read_mic_peak_to_peak()
 *
 * Samples the microphone for MIC_SAMPLE_MS milliseconds and returns
 * the difference between the highest and lowest ADC readings seen in
 * that window (0-1023). A quiet room gives a small swing; a shout
 * produces a large one. This is far more reliable than a single
 * instantaneous threshold check, which fires on any random spike.
 */
int read_mic_peak_to_peak() {
    unsigned long start = millis();
    int vmax = 0;
    int vmin = 1023;
    while ((millis() - start) < (unsigned long)MIC_SAMPLE_MS) {
        int v = analogRead(PIN_MIC);
        if (v > vmax) vmax = v;
        if (v < vmin) vmin = v;
    }
    return vmax - vmin;
}

/*
 * read_mic_action()
 *
 * Returns 1 if the peak-to-peak swing in the last MIC_SAMPLE_MS window
 * exceeds MIC_THRESHOLD, 0 otherwise.
 */
int read_mic_action() {
    return (read_mic_peak_to_peak() >= MIC_THRESHOLD) ? 1 : 0;
}

/*
 * drain_sensors()
 *
 * Keeps sampling until the mic has been below threshold for
 * SHORT_DEBOUNCE_MS. DRAIN_HARD_TIMEOUT_MS caps the total wait so a
 * continuously loud environment can't lock up the game indefinitely.
 *
 * Note: each call to read_mic_action() already takes MIC_SAMPLE_MS,
 * so the effective poll rate here is one window per MIC_SAMPLE_MS.
 */
void drain_sensors() {
    unsigned long drain_start   = millis();
    unsigned long overall_start = millis();
    while ((millis() - drain_start) < (unsigned long)SHORT_DEBOUNCE_MS) {
        if ((millis() - overall_start) >= (unsigned long)DRAIN_HARD_TIMEOUT_MS) break;
        if (read_mic_action()) drain_start = millis();
    }
}


/* =====================================================================
 * AUDIO STUBS
 * Bodies are empty. Restore when DFPlayer is working.
 * ===================================================================== */

void play_command_audio()  { /* stub */ }
void play_good_job_audio() { /* stub */ }
void play_lose_audio()     { /* stub */ }
void play_win_audio()      { /* stub */ }


/* =====================================================================
 * LEDs
 * ===================================================================== */

void set_led_color(int success) {
    digitalWrite(PIN_LED_GREEN, success ? HIGH : LOW);
    digitalWrite(PIN_LED_RED,   success ? LOW  : HIGH);
}

void clear_leds() {
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED,   LOW);
}


/* =====================================================================
 * COMMAND PROMPT
 * ===================================================================== */

void drawCommandPrompt() {
    tft.fillRect(WALL_X, CMD_BANNER_Y, WALL_W, CMD_BANNER_H,
                 tft.color565(200, 80, 0));
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(62, CMD_BANNER_Y + 2);
    tft.print("SCREAM!");
}

void clearCommandPrompt() {
    tft.fillRect(WALL_X, CMD_BANNER_Y, WALL_W, CMD_BANNER_H, COL_WALL);
    for (int py = 0; py < SCREEN_H; py += 32) {
        if (py >= CMD_BANNER_Y && py <= CMD_BANNER_Y + CMD_BANNER_H) {
            tft.drawFastHLine(WALL_X, py,     WALL_W, COL_WALL_DARK);
            tft.drawFastHLine(WALL_X, py + 1, WALL_W, COL_WALL_LITE);
        }
    }
}


/* =====================================================================
 * GAME LOGIC
 * ===================================================================== */

void update_display() {
    if (!game_active) return;
    lcd_climb_step();
}

void set_difficulty() {
    if (score > 0 && score % 5 == 0) {
        time_limit_ms -= 200;
        if (time_limit_ms < 1000) time_limit_ms = 1000;
    }
}

int detect_player_action() { return read_mic_action(); }

void start_game() {
    score              = 0;
    time_limit_ms      = 3000;
    game_active        = 1;
    waiting_for_input  = 0;
    command_start_time = 0;
    currentLevel       = 0;
    bgLevel            = 0;

    drain_sensors();
    lcd_start();
}

void end_game() {
    game_active = 0;
    clearCommandPrompt();
    play_lose_audio();
    set_led_color(0);
    lcd_fail();
    clear_leds();
}

void game_loop() {
    if (!waiting_for_input) {
        clear_leds();
        drain_sensors();

        command_start_time = millis();
        waiting_for_input  = 1;

        play_command_audio();
        drawCommandPrompt();
    }

    /* Timed out */
    if ((millis() - command_start_time) >= (unsigned long)time_limit_ms) {
        waiting_for_input = 0;
        end_game();
        return;
    }

    if (!detect_player_action()) return;

    waiting_for_input = 0;

    score++;
    set_led_color(1);
    play_good_job_audio();
    clearCommandPrompt();
    update_display();

    if (score >= MAX_SCORE) {
        play_win_audio();
        lcd_win();
        return;
    }

    set_difficulty();
    delay(POST_COMMAND_DELAY_MS);
}


/* =====================================================================
 * DISPLAY HELPERS
 * ===================================================================== */

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

int ledgeY(int localLvl) { return BOTTOM_LEDGE - (localLvl * LEDGE_SPACING); }
int climbHoldX(int i)    { return climbLeft[i] ? COL_CLIMB_L : COL_CLIMB_R; }
int climbHoldY(int i)    { return ledgeY(i) - HAND_Y_OFF; }

void generateHolds() {
    uint16_t s = (uint16_t)(bgLevel * 173 + 29);
    bool lastLeft  = false;
    int  sameCount = 0;
    for (int i = 0; i <= LEVELS_PER_BG; i++) {
        s = prng(s);
        bool goLeft;
        if (sameCount >= 2) { goLeft = !lastLeft; sameCount = 1; }
        else { goLeft = (s % 2 == 0); sameCount = (goLeft == lastLeft) ? sameCount + 1 : 1; }
        lastLeft       = goLeft;
        climbLeft[i]   = goLeft;
        s = prng(s);
        climbColIdx[i] = s % 10;
    }
    for (int i = 0; i < DECO_COUNT; i++) {
        int bandH = 260 / DECO_COUNT;
        int baseY = 30 + i * bandH;
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
        int hx = (i % 2 == 0) ? COL_CLIMB_L : COL_CLIMB_R;
        drawHold(hx, hy, holdColor(climbColIdx[i]));
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

void drawResetButton() {
    tft.fillRoundRect(190, 2, 46, 20, 4, tft.color565(180, 40, 40));
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(196, 8);
    tft.print("RESET");
}

void drawUI() {
    tft.fillRect(0, 0, 185, 24, ILI9341_BLACK);
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
    drawResetButton();
}

/*
 * repairWall()
 *
 * Repaints a vertical strip of wall texture between rows top..bot,
 * but only within the sprite's horizontal footprint (SPR_X..SPR_X+SPR_W).
 *
 * FIX: The erase region is also widened to include COL_CLIMB_L and
 * COL_CLIMB_R (the hold columns) so the reaching-arm endpoint — a
 * fillCircle drawn at the hold position — is also fully erased.
 */
void repairWall(int top, int bot) {
    /* Widen erase rect to cover the hold columns the arm reaches to */
    int eraseX = min(SPR_X, min(COL_CLIMB_L - 8, COL_DECO_L - 8));
    int eraseR = max(SPR_X + SPR_W, max(COL_CLIMB_R + 8, COL_DECO_R + 8));
    int eraseW = eraseR - eraseX;

    tft.fillRect(eraseX, top, eraseW, bot - top, COL_WALL);

    for (int px = WALL_X; px < WALL_X + WALL_W; px += 40) {
        if (px >= eraseX && px <= eraseR) {
            tft.drawFastVLine(px,     top, bot - top, COL_WALL_DARK);
            tft.drawFastVLine(px + 1, top, bot - top, COL_WALL_LITE);
        }
    }
    for (int py = 0; py < SCREEN_H; py += 32) {
        if (py >= top && py <= bot) {
            tft.drawFastHLine(eraseX, py,     eraseW, COL_WALL_DARK);
            tft.drawFastHLine(eraseX, py + 1, eraseW, COL_WALL_LITE);
        }
    }
}

/*
 * eraseSprite()
 *
 * FIX: top is now clamped using SPR_Y_OFF (58) which covers the full
 * sprite height including the reaching arm (previously 55, which left
 * the top of the arm uncleared and caused ghost duplicates).
 * After clearing, nearby holds are redrawn so the erase doesn't
 * accidentally blank them.
 */
void eraseSprite(int ly) {
    int top = max(25, ly - SPR_Y_OFF);   /* SPR_Y_OFF=58 covers helm+arm */
    int bot = min(SCREEN_H, ly + 5);
    repairWall(top, bot);
    if (currentLevel < LEVELS_PER_BG && bot >= 285) drawFloor();
    int local = currentLevel % LEVELS_PER_BG;
    for (int i = local; i >= max(0, local - 3); i--) {
        int hy = climbHoldY(i);
        int hx = (i % 2 == 0) ? COL_CLIMB_L : COL_CLIMB_R;
        drawHold(hx, hy, holdColor(climbColIdx[i]));
    }
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
    tft.fillRect(gx + 38, gy - 16, 17,  6, helm);
    tft.fillRect(gx -  4, gy - 14,  6, 14, shirt);
    tft.fillRect(gx -  4, gy - 18,  6,  6, skin);
    tft.fillRect(gx + 20, gy +  6,  6, 14, shirt);
    tft.fillRect(gx + 20, gy + 20,  6,  6, skin);
    tft.fillRect(gx - 18, gy -  4, 20,  8, pants);
    tft.fillRect(gx - 18, gy -  4,  8, 18, pants);
    tft.fillRect(gx - 18, gy + 14, 16,  6, shoe);
    tft.fillRect(gx - 26, gy -  4,  8,  6, shoe);
}

void showStartScreen() {
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
    tft.fillRect(CX +  2, 236,  9, 5, tft.color565(30,  20,  10));
    tft.fillRect(CX -  8, 223,  6, 13, tft.color565(50,  50, 140));
    tft.fillRect(CX +  2, 223,  6, 13, tft.color565(50,  50, 140));
    tft.fillRect(CX -  9, 209, 19, 15, tft.color565(200,  50,  50));
    tft.fillCircle(CX, 199, 9, tft.color565(255, 200, 150));
    tft.fillRect(CX -  8, 189, 17,  8, tft.color565(255, 200,   0));
    tft.fillRect(CX -  9, 195, 19,  4, tft.color565(255, 200,   0));
    tft.fillRect(CX - 19, 211, 10,  4, tft.color565(200,  50,  50));
    tft.fillRect(CX +  9, 211, 10,  4, tft.color565(200,  50,  50));
    tft.fillRect(CX - 22, 211,  4,  4, tft.color565(255, 200, 150));
    tft.fillRect(CX + 19, 211,  4,  4, tft.color565(255, 200, 150));
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
}

void showWinScreen() {
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
    tft.fillRoundRect(60, 230, 120, 45, 8, tft.color565(50, 180, 50));
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_BLACK);
    tft.setCursor(78, 245);
    tft.print("RESTART");
    while (!touchInRegion(60, 230, 120, 45));
    delay(200);
    currentLevel = 0; bgLevel = 0;
    showStartScreen();
    drawBackground(); drawUI();
    drawSprite(ledgeY(0), 0);
}

void doFall() {
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
    tft.print(currentLevel);
    tft.fillRoundRect(170, 5, 65, 28, 6, tft.color565(50, 180, 50));
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_BLACK);
    tft.setCursor(178, 12);
    tft.print("RETRY");
    while (!touchInRegion(170, 5, 65, 28));
    delay(200);
    currentLevel = 0; bgLevel = 0;
    showStartScreen();
    drawBackground(); drawUI();
    drawSprite(ledgeY(0), 0);
}

void doReset() {
    currentLevel = 0; bgLevel = 0;
    showStartScreen();
    drawBackground(); drawUI();
    drawSprite(ledgeY(0), 0);
}


/* =====================================================================
 * LCD INTERFACE
 * ===================================================================== */

void lcd_setup() {
    /* TFT must initialise before I2C/touch */
    tft.begin();
    delay(200);
    tft.setRotation(0);
    tft.fillScreen(ILI9341_BLACK);
    tft.setTextWrap(false);
    tft.cp437(true);

    Wire.begin();
    Wire.setTimeout(10);
    ts.begin(40);

    showStartScreen();
    drawBackground();
    drawUI();
    drawSprite(ledgeY(0), 0);
}

void lcd_start() {
    drawBackground();
    drawUI();
    drawSprite(ledgeY(0), 0);
}

void lcd_climb_step() {
    if (currentLevel < TOTAL_LEVELS) {
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
        }

        drawSprite(ledgeY(local), 0);
        drawUI();
    }
}

void lcd_fail() { doFall(); }
void lcd_win()  { showWinScreen(); }


/* =====================================================================
 * ARDUINO ENTRY POINTS
 * ===================================================================== */

void setup() {
    init_system();
    start_game();
}

void loop() {
    if (game_active) {
        if (touchInRegion(190, 2, 46, 20)) {
            doReset();
            start_game();
            return;
        }
        game_loop();
    } else {
        delay(500);
        start_game();
    }
}
