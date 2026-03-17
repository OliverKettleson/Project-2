"""
Climb-It Game

Team Lambda, 2026
"""

### ---------- Imports / Libraries / Globals ---------- ###

import random
import time
import sys
import select

COMMANDS = ["GRIP", "BRUSH", "SHOUT"]

# Sensor thresholds — tune these once hardware is available
FSR_THRESHOLD      = 512   # ADC counts (0-1023), FSR06BE grip trigger level
MIC_THRESHOLD      = 600   # ADC counts (0-1023), MAX9814 shout trigger level
BRUSH_PROX_COUNT   = 5     # MTCH1010 proximity counts to register a valid brush stroke
VIBRATION_THRESHOLD = 1    # MSP6914: 1 = vibration detected (digital)

# Game state
score           = 0
current_command = None
time_limit      = 3.0
game_active     = False



### ----------- Initialization ----------- ###

def init_system():          # Initialize all components (display, sensors, audio, LEDs)

    return 0

def init_display():         # Initialize the display for a new game

    return 0

def init_sensors():         # Initialize all input sensors (grip, brush, mic)

    return 0

def init_audio():           # Initialize audio system and load sound files

    return 0

def init_leds():            # Initialize LED system and set default state

    return 0


### ----------- Input Sensors ----------- ###

def read_grip_strength():           # Read the current grip strength from the sensor

    return 0

def read_grip_action():             # Determine if the grip action is strong enough to count as a successful grip

    return 0

def read_brush_sensor():            # Read MTCH1010 proximity detector — returns proximity count for brush stroke

    return 0

def read_brush_action():            # Confirm brush stroke: MTCH1010 proximity count meets threshold AND TLE4906 Hall switch detects magnet

    return 0

def read_mic_level():               # Read the microphone level from MAX9814 eval board (analog)

    return 0

def read_mic_action():              # Determine if the mic action is strong enough to count as a successful shout

    return 0

def read_vibration():               # Read MSP6914 vibration sensor — returns 1 if vibration detected, 0 otherwise

    return 0


### ----------- Output Actions ----------- ###

def update_display():               # Update the display with the current game state (Score + Animation)

    return 0

def play_command_audio():           # Play the audio cue for the current command

    return 0

def play_lose_audio():              # Play the audio cue for losing the game

    return 0

def play_win_audio():               # Play the audio cue for winning the game

    return 0

def set_led_color():                # Set the LED color based on the current action (green for success, red for failure)

    return 0


### ----------- Logic ----------- ###

def pick_next_command():            # Randomly select the next command for the player to perform (grip, brush, or shout)

    global current_command
    current_command = random.choice(COMMANDS)
    return current_command

def check_command_success(player_action):   # Check if the player's action matches the current command and update the score

    global score

    if player_action == current_command:

        score += 1
        return True
    
    return False

def set_difficulty():               # Adjust the difficulty of the game based on the player's score

    global time_limit

    # Decrease time limit every 5 points, minimum 1.0 second
    time_limit = max(1.0, 3.0 - (score // 5) * 0.3)

    return time_limit


### ----------- Game State ----------- ###

def start_game():                   # Start a new game and initialize all necessary components

    global game_active
    reset_game()
    init_system()
    game_active = True

def reset_game():                   # Reset the game state to start a new game

    global score, current_command, time_limit, game_active

    score           = 0
    current_command = None
    time_limit      = 3.0
    game_active     = False

def end_game():                     # End the game and display the final score

    global game_active
    game_active = False


### ----------- Main Game Loop ----------- ###

def game_loop():          # Main game loop — picks command, waits for sensor response, checks result, adjusts difficulty

    while game_active:
        set_difficulty()
        pick_next_command()
        play_command_audio()        # stub: will play spoken command via LM833N amp
        update_display()            # stub: will show command + score on 2.8" TFT

        deadline = time.time() + time_limit
        player_action = None

        while time.time() < deadline:
            if read_grip_action():
                player_action = "GRIP"
                break
            if read_brush_action():
                player_action = "BRUSH"
                break
            if read_mic_action():
                player_action = "SHOUT"
                break

        if check_command_success(player_action):
            set_led_color()         # stub: green on MV5437
            play_win_audio()        # stub: success tone
        else:
            set_led_color()         # stub: red on MV5437
            play_lose_audio()       # stub: fail tone
            end_game()
            return

        update_display()            # stub: update score on TFT

def main():

    init_system()
    start_game()
    game_loop()

if __name__ == "__main__":

    main()