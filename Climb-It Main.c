/*
 * Climb-It Game
 * Team Lambda, 2026
 * Oliver Kettleson-Belinkie
 * Connor Kariotis
 * 
 * Display funtions not implimented yet
 * Brush is using light sensor and vibration sensor
 * Audio functions not implemented yet
 */


/* ---------- Includes ---------- */

//#include <Arduino.h>
#include <stdlib.h>     /* rand() */
#include <string.h>


/* ---------- Debug ---------- */

#define DEBUG 1


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
//#define PIN_AUDIO     9


/* ---------- Sensor Thresholds ---------- */

#define FSR_THRESHOLD       512   /* ADC counts (0-1023), grip trigger level */
#define MIC_THRESHOLD       600   /* ADC counts (0-1023), shout trigger level */
#define BRUSH_PROX_THRESHOLD 400  /* ADC counts (0-1023), proximity trigger level */


/* ---------- Timing ---------- */

#define BUTTON_DEBOUNCE_MS    50
#define POST_COMMAND_DELAY_MS 400


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

static unsigned long command_start_ms = 0;
static unsigned long led_until_ms     = 0;
static int           led_showing      = 0;      /* 0 = off, 1 = green, -1 = red */


/* ========== Initialization ========== */

void init_system()   {

    Serial.begin(115200);

    init_sensors();
    init_audio();
    init_leds();
    init_display();

    randomSeed(analogRead(A5));   /* simple seed for randomness */

    Serial.println("System initialized.");
}

void init_display()  {}

void init_sensors()  {

    pinMode(PIN_FSR, INPUT);
    pinMode(PIN_MIC, INPUT);
    pinMode(PIN_BRUSH_PROX, INPUT);

    pinMode(PIN_BRUSH_HALL, INPUT_PULLUP);  /* LOW when magnet present */
    pinMode(PIN_VIBRATION, INPUT);

    pinMode(PIN_START_STOP, INPUT_PULLUP);  /* button pressed = LOW */

    Serial.println("Sensors initialized.");
}

void init_audio()    {}

void init_leds()     {

    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);

    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, LOW);

    Serial.println("LEDs initialized.");
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

int read_brush_proximity() { 
    
    return analogRead(PIN_BRUSH_PROX);
}

int read_brush_hall()     { 

    return digitalRead(PIN_BRUSH_HALL);
}

int read_vibration()      { 

    return digitalRead(PIN_VIBRATION);
}

int read_brush_action()   { 
    
    int prox_value  = read_brush_proximity();
    int vib_trigger = read_vibration();

    if ((prox_value >= BRUSH_PROX_THRESHOLD) && (vib_trigger == HIGH)) {

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


/* ========== Output Actions ========== */

void update_display()      {}

void play_command_audio()  {}

void play_lose_audio()     {}

void play_win_audio()      {}

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


/* ========== Logic ========== */

const char* command_to_string(Command cmd) {

    switch (cmd) {

        case CMD_GRIP:  return "GRIP";
        case CMD_BRUSH: return "BRUSH";
        case CMD_SHOUT: return "SHOUT";
        default:        return "NONE";
    }
}

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

void reset_game() {     /* Reset all game state variables */

    score           = 0;
    current_command = CMD_NONE;
    time_limit_ms   = 3000;
    game_active     = 0;

    clear_leds();
    serial_println("Game reset.");
}

void start_game() {

    score           = 0;
    current_command = CMD_NONE;
    time_limit_ms   = 3000;
    game_active     = 1;

    clear_leds();

    Serial.println("Game started.");
}

void end_game()   {

    Serial.print("Game over. Final score: ");
    Serial.println(score);

    play_lose_audio();
    set_led_color(0);

    game_active = 0;
}


/* ========== Main Game Loop ========== */

void game_loop() {

    static unsigned long command_start_time = 0;
    static int waiting_for_input = 0;

    Command player_action;

    /* If no command is active, issue a new one */
    if (!waiting_for_input) {

        clear_leds();

        current_command = pick_next_command();
        command_start_time = millis();
        waiting_for_input = 1;

        Serial.print("New command: ");
        Serial.println(command_to_string(current_command));

        play_command_audio();
        update_display();
    }

    /* Check for timeout */
    if ((millis() - command_start_time) >= (unsigned long)time_limit_ms) {

        Serial.println("Timeout - player failed.");
        waiting_for_input = 0;
        end_game();
        return;
    }

    /* Check player input */
    player_action = detect_player_action();

    if (player_action != CMD_NONE) {

        Serial.print("Detected action: ");
        Serial.println(command_to_string(player_action));

        if (check_command_success(player_action)) {

            score++;
            Serial.print("Correct! Score = ");
            Serial.println(score);

            set_led_color(1);
            play_win_audio();

            set_difficulty();
            waiting_for_input = 0;

            delay(POST_COMMAND_DELAY_MS);
            clear_leds();
        } 
        else {

            Serial.println("Wrong action.");
            waiting_for_input = 0;
            end_game();
            return;
        }
    }
}


/* ========== Arduino Entry Points ========== */

void setup() {

    init_system();
}

void loop() {

    static int last_button_state = HIGH;
    int button_state = digitalRead(PIN_START_STOP);

    if ((last_button_state == HIGH) && (button_state == LOW)) {

        delay(BUTTON_DEBOUNCE_MS);

        if (digitalRead(PIN_START_STOP) == LOW) {

            if (!game_active) {

                start_game();
            } 
            else {

                Serial.println("Game already active.");
            }
        }
    }

    last_button_state = button_state;

    if (game_active) {
        
        game_loop();
    }
}
