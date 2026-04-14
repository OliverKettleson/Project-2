/*
 * Climb-It Game
 * Team Lambda, 2026
 * Oliver Kettleson-Belinkie
 * 
 * Display funtions not implimented yet
 * Brush is using hall sensor and vibration sensor
 * Audio functions not implemented yet
 */


/* ---------- Includes ---------- */

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

/* ---------- FORWARD DECLARATIONS ---------- */

//Connect to LCD system
void lcd_setup();
void lcd_start();
void lcd_climb_step();
void lcd_fail();
void lcd_win();

/* ---------- Pin Definitions ---------- */

/* Inputs */
#define PIN_FSR         A0   /* grip force */
#define PIN_MIC         A1   /* mic level */
#define PIN_BRUSH_HALL  2    /* hall switch — digital, LOW = magnet present */
#define PIN_VIBRATION   3    /* vibration sensor — digital, HIGH = vibration */

/* Outputs */
#define PIN_LED_GREEN   5    /* bi-color LED — green anode */
#define PIN_LED_RED     6    /* bi-color LED — red anode */

/* DFPlayer Mini UART */
#define PIN_DF_RX       10   /* Arduino RX <- DFPlayer TX */
#define PIN_DF_TX       11   /* Arduino TX -> DFPlayer RX */


/* ---------- Sensor Thresholds ---------- */

#define FSR_THRESHOLD       512   /* ADC counts (0-1023), grip trigger level */
#define MIC_THRESHOLD       600   /* ADC counts (0-1023), shout trigger level */


/* ---------- Timing ---------- */

#define POST_COMMAND_DELAY_MS 400
#define FAIL_DISPLAY_MS        2000  /* how long to show the fail state      */
#define SHORT_DEBOUNCE_MS      300
#define MAX_SCORE             99


/* ---------- DFPlayer track numbers ---------- */
/*
 * Rename the files on the SD card so the DFPlayer can address them by number.
 * Put them in a folder called /MP3 on the card root.
 *
 *   /MP3/0004.mp3  <- GetReady.mp3
 *   /MP3/0005.mp3  <- Squeeze.mp3
 *   /MP3/0006.mp3  <- Brush.mp3
 *   /MP3/0007.mp3  <- Scream.mp3
 *   /MP3/0008.mp3  <- GoodJob.mp3
 *   /MP3/0009.mp3  <- NextLevel.mp3
 *   /MP3/0010.mp3  <- NiceSend.mp3
 *   /MP3/0011.mp3  <- YouFell.mp3
 *
 * Tracks 0001-0003 are your old files; leave them alone, just don't use them.
 */
#define TRACK_GET_READY  4
#define TRACK_SQUEEZE    5
#define TRACK_BRUSH      6
#define TRACK_SCREAM     7
#define TRACK_GOOD_JOB   8
#define TRACK_NEXT_LEVEL 9
#define TRACK_NICE_SEND  10
#define TRACK_YOU_FELL   11


/* ---------- Command Enum ---------- */

typedef enum {

    CMD_GRIP  = 0,
    CMD_BRUSH = 1,
    CMD_SHOUT = 2,
    CMD_NONE  = 3
} Command;


/* ---------- Game State ---------- */

static int          score           = 0;
static Command      current_command = CMD_NONE;
static int          time_limit_ms   = 3000;   /* milliseconds */
static int          game_active     = 0;

static int     waiting_for_input    = 0;
static unsigned long command_start_time = 0;

SoftwareSerial dfSerial(PIN_DF_RX, PIN_DF_TX);
DFRobotDFPlayerMini dfPlayer;

/* ---------- Helper Prototypes ---------- */
void init_system();
void init_display();
void init_sensors();
void init_audio();
void init_leds();

int read_grip_strength();
int read_grip_action();
int read_vibration();
int read_brush_action();
int read_mic_level();
int read_mic_action();

void drain_sensors();

void update_display();
void play_track(int track);
void play_command_audio();
void play_lose_audio();
void play_win_audio();
void play_good_job_audio();
void set_led_color(int success);
void clear_leds();

Command pick_next_command();
void set_difficulty();
Command detect_player_action();

void start_game();
void end_game();
void game_loop();

/* ========== Initialization ========== */

void init_system()   {

    init_sensors();
    init_audio();
    init_leds();
    init_display();

    randomSeed(analogRead(A5));   /* simple seed for randomness */
}

void init_display()  {

    lcd_setup();
}

void init_sensors()  {

    pinMode(PIN_FSR, INPUT);
    pinMode(PIN_MIC, INPUT);

    pinMode(PIN_BRUSH_HALL, INPUT_PULLUP);  /* LOW when magnet present */
    pinMode(PIN_VIBRATION, INPUT);
}

void init_audio()    {

    dfSerial.begin(9600);

    /* Give the DFPlayer ~2 s to boot, then initialise */
    delay(2000);
    dfPlayer.begin(dfSerial);
    dfPlayer.volume(25);
}

void init_leds()     {

    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);

    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, LOW);
}


/* ========== Input Sensors ========== */

int read_grip_strength()  { 
    
    return analogRead(PIN_FSR);
}

int read_grip_action()    { 
    
    int fsr_value = read_grip_strength();

    if (fsr_value >= FSR_THRESHOLD) {

        return 1;
    }

    return 0; 
}

int read_brush_hall()     { 

    return digitalRead(PIN_BRUSH_HALL);
}

int read_vibration()      { 

    return digitalRead(PIN_VIBRATION);
}

int read_brush_action()   { 
    
    int hall_trigger = read_brush_hall();
    int vib_trigger  = read_vibration();

    if ((hall_trigger == LOW) && (vib_trigger == HIGH)) {

        return 1;
    }

    return 0;
}

int read_mic_level()      { 
    
    return analogRead(PIN_MIC);
}

int read_mic_action()     { 
    
    int mic_value = read_mic_level();

    if (mic_value >= MIC_THRESHOLD) {

        return 1;
    }

    return 0;
}

void drain_sensors() {

    unsigned long drain_start = millis();

    while ((millis() - drain_start) < (unsigned long)SHORT_DEBOUNCE_MS) {

        /* Keep looping while any sensor is still active */
        if (read_grip_action() || read_brush_action() || read_mic_action()) {

            drain_start = millis();   /* reset window whenever still triggered */
        }
    }
}

/* ========== Output Actions ========== */

void update_display()      {

    if (!game_active) return;

    lcd_climb_step();
}

void play_track(int track) {

    dfPlayer.playMp3Folder(track);
}

void play_command_audio()  {

    switch (current_command) {

        case CMD_GRIP:

            play_track(TRACK_SQUEEZE);
            break;

        case CMD_BRUSH:

            play_track(TRACK_BRUSH);
            break;

        case CMD_SHOUT:

            play_track(TRACK_SHOUT);
            break;

        default:
            break;
    }
}

void play_good_job_audio() {

    play_track(TRACK_GOOD_JOB);
}

void play_lose_audio() {

    play_track(TRACK_YOU_FELL);
}

void play_win_audio() {

    play_track(TRACK_NICE_SEND);
}

void set_led_color(int success) {

    if (success) {

        digitalWrite(PIN_LED_GREEN, HIGH);
        digitalWrite(PIN_LED_RED, LOW);
    } 
    else {

        digitalWrite(PIN_LED_GREEN, LOW);
        digitalWrite(PIN_LED_RED, HIGH);
    }
}

void clear_leds() {

    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, LOW);
}


/* ========== Logic ========== */

Command pick_next_command() {               /* Randomly select the next command */

    int cmd = random(0, 3);   /* Random number between 0 and 2 */

    return (Command) cmd;
}

void set_difficulty() { 

    if (score > 0 && score % 5 == 0) {

        time_limit_ms -= 200;

        if (time_limit_ms < 1000) {

            time_limit_ms = 1000;
        }

        play_track(TRACK_NEXT_LEVEL);
    }
}

Command detect_player_action() {
  
    if (read_grip_action()) {

        return CMD_GRIP;
    }

    if (read_brush_action()) {

        return CMD_BRUSH;
    }

    if (read_mic_action()) {

        return CMD_SHOUT;
    }

    return CMD_NONE;
}

/* ========== Game State ========== */

void start_game() {

    score           = 0;
    current_command = CMD_NONE;
    time_limit_ms   = 3000;
    game_active     = 1;

    waiting_for_input    = 0;
    command_start_time   = 0;

    drain_sensors();   /* FIX 2: clear any lingering sensor state */

    play_track(TRACK_GET_READY);
    lcd_start();
}

void end_game()   {

    game_active = 0;
    play_lose_audio();
    set_led_color(0);
    lcd_fail();

    delay(FAIL_DISPLAY_MS);

    clear_leds();
}


/* ========== Main Game Loop ========== */

vvoid game_loop() {

    Command player_action;

    if (!waiting_for_input) {

        clear_leds();
        drain_sensors();           

        current_command    = pick_next_command();
        command_start_time = millis();
        waiting_for_input  = 1;

        play_command_audio();
        update_display();                
    }

    if ((millis() - command_start_time) >= (unsigned long)time_limit_ms) {

        waiting_for_input = 0;
        end_game();
        return;
    }

    player_action = detect_player_action();

    if (player_action == CMD_NONE) return;   /* nothing yet, keep polling */

    waiting_for_input = 0;

    if (player_action == current_command) {

     
        score++;
        set_led_color(1);
        play_good_job_audio();
        update_display();

        if (score >= MAX_SCORE) {

            play_win_audio();
            lcd_win();
            delay(FAIL_DISPLAY_MS);   /* let the win state display */
            start_game();
            return;
        }

        set_difficulty();
        delay(POST_COMMAND_DELAY_MS);
    }
    else {

        /* Wrong input */
        end_game();
    }
}


/* ========== Arduino Entry Points ========== */

void setup() {

    init_system();
    start_game();
}

void loop() {

    if (game_active) {
        
        game_loop();
    }
    else {

        delay(500);
        start_game();
    }
}
