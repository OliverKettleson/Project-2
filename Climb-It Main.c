/*
 * Climb-It Game
 * Team Lambda, 2026
 * Oliver Kettleson-Belinkie
 */


/* ---------- Includes ---------- */

//#include <Arduino.h>
#include <stdlib.h>     /* rand() */
#include <string.h>


/* ---------- Pin Definitions ---------- */

/* Inputs */
#define PIN_FSR         A0   /* grip force */
#define PIN_MIC         A1   /* mic level */
#define PIN_BRUSH_PROX  A2   /* proximity count */
#define PIN_BRUSH_HALL  2    /* hall switch — digital, LOW = magnet present */
#define PIN_VIBRATION   3    /* vibration sensor — digital, HIGH = vibration */
#define PIN_START_STOP  4    /* tactile button */

/* Outputs */
#define PIN_LED_GREEN   5    /* bi-color LED — green anode */
#define PIN_LED_RED     6    /* bi-color LED — red anode */


/* ---------- Sensor Thresholds ---------- */

#define FSR_THRESHOLD       512   /* ADC counts (0-1023), grip trigger level */
#define MIC_THRESHOLD       600   /* ADC counts (0-1023), shout trigger level */
#define BRUSH_PROX_THRESHOLD 400  /* ADC counts (0-1023), proximity trigger level */


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


/* ========== Initialization ========== */

void init_system()   {}

void init_display()  {}

void init_sensors()  {}

void init_audio()    {}

void init_leds()     {}


/* ========== Input Sensors ========== */

int read_grip_strength()  { 
    
    return 0; 
}

int read_grip_action()    { 
    
    return 0; 
}

int read_brush_proximity() { 
    
    return 0; 
}

int read_brush_hall()     { 

    return 0; 
}

int read_vibration()      { 

    return 0; 
}

int read_brush_action()   { 
    
    return 0; 
}

int read_mic_level()      { 
    
    return 0; 
}

int read_mic_action()     { 
    
    return 0; 
}


/* ========== Output Actions ========== */

void update_display()      {}

void play_command_audio()  {}

void play_lose_audio()     {}

void play_win_audio()      {}

void set_led_color(int success) {

    if (success) {

        //digitalWrite(PIN_LED_GREEN, HIGH);
        //digitalWrite(PIN_LED_RED, LOW);
    } 
    else {

        //digitalWrite(PIN_LED_GREEN, LOW);
        //digitalWrite(PIN_LED_RED, HIGH);
    }
}


/* ========== Logic ========== */

Command pick_next_command() {               /* Randomly select the next command */

    int cmd = rand() % 3;   /* Random number between 0 and 2 */

    return (Command) cmd;
}

int check_command_success(Command player_action) {

    int success = (player_action == current_command);
    return success;
}

void set_difficulty() { 

    if (score > 0 && score % 5 == 0 && time_limit_ms > 1000) {      

        time_limit_ms -= 200;       // Decrease time limit by 200ms every 5 levels, down to a minimum of 1 second - starts at 3 seconds
    }
}


/* ========== Game State ========== */

void reset_game() {     /* Reset all game state variables */

    score           = 0;
    current_command = CMD_NONE;
    time_limit_ms   = 3000;
    game_active     = 0;
}

void start_game() {

    game_active = 1;
}

void end_game()   {

    play_lose_audio();
    game_active = 0;
}


/* ========== Main Game Loop ========== */

void game_loop() {}


/* ========== Arduino Entry Points ========== */

void setup() {

    start_game();
}

void loop() {

    if (game_active) {

        game_loop();
    }
}
