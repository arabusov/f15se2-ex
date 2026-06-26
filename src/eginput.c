/*
 * eginput.c - SDL keyboard input for the egame flight loop.
 *
 * Replaces the hand-written INT 09h keyboard ISR from egseg3.asm
 * (setInt9Handler / restoreInt9Handler / kbdInt9Handler). The original hooked
 * the keyboard hardware interrupt, read raw scancodes off port 0x60, hand-built
 * BIOS-style key words in the BIOS keyboard buffer, and turned the arrow /
 * numeric-keypad keys into a virtual analog stick. None of that has a meaning
 * in a native process, so this provides the same two behaviours from SDL:
 *
 *   1. A BIOS-style key ring (scan code in AH, ASCII in AL) drained from the SDL
 *      event queue, read back through kbhit()/egReadKey(). stepFlightModel
 *      (egflight.c) reads one word per frame and dispatches it (egkeys.c), which
 *      compares the full word (e.g. 0x1372 = 'r', 0x3b00 = F1), so the AH scan
 *      code has to match what INT 16h would have returned.
 *
 *   2. The virtual stick: held arrow / keypad keys deflect g_joyRawX (roll) and
 *      g_joyRawY (pitch), which stepFlightModel reads when no real joystick is
 *      configured. We poll the live key state each frame rather than tracking
 *      press/release, so diagonals and recentre fall out naturally.
 *
 * Joystick hardware stays out of scope (stubbed in egstubs.c).
 */
#include "egtypes.h"
#include "egcode.h"
#include "egdata.h"
#include "eginput.h"
#include "inttype.h"
#include "gfx.h"
#include "joystick.h"
#include <dos.h>
#include <SDL3/SDL.h>

/* Game tick clock (timer.c); pumped here so the window stays responsive and the
 * clock advances even on a frame that does nothing but poll keys. */
extern void timerPump(void);

/* BIOS-style key ring: each entry is AH = scan code, AL = ASCII, matching the
 * word layout INT 16h returns and the egame dispatch compares against. */
#define EGKEY_RING 32
static uint16 keyRing[EGKEY_RING];
static int ringHead = 0, ringTail = 0;

static void ringPush(uint16 word) {
    int next = (ringTail + 1) % EGKEY_RING;
    if (next == ringHead) return; /* full: drop, as the BIOS buffer would */
    keyRing[ringTail] = word;
    ringTail = next;
}

/* Map an SDL scancode + modifiers to a US-layout BIOS key word, or 0 if the key
 * carries no command meaning (the numeric keypad / arrows are handled as the
 * virtual stick instead). AH is the BIOS make code; AL is the ASCII byte, which
 * is 0 for Alt combos and the function keys, exactly as INT 16h reports. */
static uint16 biosWord(SDL_Scancode sc, SDL_Keymod mod) {
    Uint8 scan;
    char lo, hi; /* unshifted / shifted ASCII */

    switch (sc) {
    case SDL_SCANCODE_A:
        scan = 0x1E;
        lo = 'a';
        hi = 'A';
        break;
    case SDL_SCANCODE_B:
        scan = 0x30;
        lo = 'b';
        hi = 'B';
        break;
    case SDL_SCANCODE_C:
        scan = 0x2E;
        lo = 'c';
        hi = 'C';
        break;
    case SDL_SCANCODE_D:
        scan = 0x20;
        lo = 'd';
        hi = 'D';
        break;
    case SDL_SCANCODE_E:
        scan = 0x12;
        lo = 'e';
        hi = 'E';
        break;
    case SDL_SCANCODE_F:
        scan = 0x21;
        lo = 'f';
        hi = 'F';
        break;
    case SDL_SCANCODE_G:
        scan = 0x22;
        lo = 'g';
        hi = 'G';
        break;
    case SDL_SCANCODE_H:
        scan = 0x23;
        lo = 'h';
        hi = 'H';
        break;
    case SDL_SCANCODE_I:
        scan = 0x17;
        lo = 'i';
        hi = 'I';
        break;
    case SDL_SCANCODE_J:
        scan = 0x24;
        lo = 'j';
        hi = 'J';
        break;
    case SDL_SCANCODE_K:
        scan = 0x25;
        lo = 'k';
        hi = 'K';
        break;
    case SDL_SCANCODE_L:
        scan = 0x26;
        lo = 'l';
        hi = 'L';
        break;
    case SDL_SCANCODE_M:
        scan = 0x32;
        lo = 'm';
        hi = 'M';
        break;
    case SDL_SCANCODE_N:
        scan = 0x31;
        lo = 'n';
        hi = 'N';
        break;
    case SDL_SCANCODE_O:
        scan = 0x18;
        lo = 'o';
        hi = 'O';
        break;
    case SDL_SCANCODE_P:
        scan = 0x19;
        lo = 'p';
        hi = 'P';
        break;
    case SDL_SCANCODE_Q:
        scan = 0x10;
        lo = 'q';
        hi = 'Q';
        break;
    case SDL_SCANCODE_R:
        scan = 0x13;
        lo = 'r';
        hi = 'R';
        break;
    case SDL_SCANCODE_S:
        scan = 0x1F;
        lo = 's';
        hi = 'S';
        break;
    case SDL_SCANCODE_T:
        scan = 0x14;
        lo = 't';
        hi = 'T';
        break;
    case SDL_SCANCODE_U:
        scan = 0x16;
        lo = 'u';
        hi = 'U';
        break;
    case SDL_SCANCODE_V:
        scan = 0x2F;
        lo = 'v';
        hi = 'V';
        break;
    case SDL_SCANCODE_W:
        scan = 0x11;
        lo = 'w';
        hi = 'W';
        break;
    case SDL_SCANCODE_X:
        scan = 0x2D;
        lo = 'x';
        hi = 'X';
        break;
    case SDL_SCANCODE_Y:
        scan = 0x15;
        lo = 'y';
        hi = 'Y';
        break;
    case SDL_SCANCODE_Z:
        scan = 0x2C;
        lo = 'z';
        hi = 'Z';
        break;

    case SDL_SCANCODE_1:
        scan = 0x02;
        lo = '1';
        hi = '!';
        break;
    case SDL_SCANCODE_2:
        scan = 0x03;
        lo = '2';
        hi = '@';
        break;
    case SDL_SCANCODE_3:
        scan = 0x04;
        lo = '3';
        hi = '#';
        break;
    case SDL_SCANCODE_4:
        scan = 0x05;
        lo = '4';
        hi = '$';
        break;
    case SDL_SCANCODE_5:
        scan = 0x06;
        lo = '5';
        hi = '%';
        break;
    case SDL_SCANCODE_6:
        scan = 0x07;
        lo = '6';
        hi = '^';
        break;
    case SDL_SCANCODE_7:
        scan = 0x08;
        lo = '7';
        hi = '&';
        break;
    case SDL_SCANCODE_8:
        scan = 0x09;
        lo = '8';
        hi = '*';
        break;
    case SDL_SCANCODE_9:
        scan = 0x0A;
        lo = '9';
        hi = '(';
        break;
    case SDL_SCANCODE_0:
        scan = 0x0B;
        lo = '0';
        hi = ')';
        break;

    case SDL_SCANCODE_MINUS:
        scan = 0x0C;
        lo = '-';
        hi = '_';
        break;
    case SDL_SCANCODE_EQUALS:
        scan = 0x0D;
        lo = '=';
        hi = '+';
        break;
    case SDL_SCANCODE_LEFTBRACKET:
        scan = 0x1A;
        lo = '[';
        hi = '{';
        break;
    case SDL_SCANCODE_RIGHTBRACKET:
        scan = 0x1B;
        lo = ']';
        hi = '}';
        break;
    case SDL_SCANCODE_SEMICOLON:
        scan = 0x27;
        lo = ';';
        hi = ':';
        break;
    case SDL_SCANCODE_APOSTROPHE:
        scan = 0x28;
        lo = '\'';
        hi = '"';
        break;
    case SDL_SCANCODE_GRAVE:
        scan = 0x29;
        lo = '`';
        hi = '~';
        break;
    case SDL_SCANCODE_BACKSLASH:
        scan = 0x2B;
        lo = '\\';
        hi = '|';
        break;
    case SDL_SCANCODE_COMMA:
        scan = 0x33;
        lo = ',';
        hi = '<';
        break;
    case SDL_SCANCODE_PERIOD:
        scan = 0x34;
        lo = '.';
        hi = '>';
        break;
    case SDL_SCANCODE_SLASH:
        scan = 0x35;
        lo = '/';
        hi = '?';
        break;

    case SDL_SCANCODE_SPACE:
        scan = 0x39;
        lo = ' ';
        hi = ' ';
        break;
    case SDL_SCANCODE_RETURN:
        scan = 0x1C;
        lo = 0x0D;
        hi = 0x0D;
        break;
    case SDL_SCANCODE_BACKSPACE:
        scan = 0x0E;
        lo = 0x08;
        hi = 0x08;
        break;
    case SDL_SCANCODE_ESCAPE:
        scan = 0x01;
        lo = 0x1B;
        hi = 0x1B;
        break;
    case SDL_SCANCODE_TAB:
        scan = 0x0F;
        lo = 0x09;
        hi = 0x09;
        break;

    /* Function keys carry no ASCII; egame compares the bare scan word. */
    case SDL_SCANCODE_F1:
        scan = 0x3B;
        lo = hi = 0;
        break;
    case SDL_SCANCODE_F2:
        scan = 0x3C;
        lo = hi = 0;
        break;
    case SDL_SCANCODE_F3:
        scan = 0x3D;
        lo = hi = 0;
        break;
    case SDL_SCANCODE_F4:
        scan = 0x3E;
        lo = hi = 0;
        break;
    case SDL_SCANCODE_F5:
        scan = 0x3F;
        lo = hi = 0;
        break;
    case SDL_SCANCODE_F6:
        scan = 0x40;
        lo = hi = 0;
        break;
    case SDL_SCANCODE_F7:
        scan = 0x41;
        lo = hi = 0;
        break;
    case SDL_SCANCODE_F8:
        scan = 0x42;
        lo = hi = 0;
        break;
    case SDL_SCANCODE_F9:
        scan = 0x43;
        lo = hi = 0;
        break;
    case SDL_SCANCODE_F10:
        scan = 0x44;
        lo = hi = 0;
        break;

    default:
        return 0;
    }

    /* Alt forces AL = 0 (BIOS reports the scan code alone for Alt combos). */
    if (mod & SDL_KMOD_ALT)
        return (uint16)scan << 8;
    /* Ctrl+letter -> control code 1..26 in AL. */
    if ((mod & SDL_KMOD_CTRL) && lo >= 'a' && lo <= 'z')
        return ((uint16)scan << 8) | (uint8)(lo - 'a' + 1);
    return ((uint16)scan << 8) | (uint8)((mod & SDL_KMOD_SHIFT) ? hi : lo);
}

/* Update the virtual stick from the live key state. The original mapped the
 * numeric keypad and arrow keys to one held direction at a time via a scancode
 * table; polling the held state reproduces the same axis assignment and gives
 * smooth diagonals for free. Deflection matches the original first-press value
 * (0x5A off centre); a real joystick is handled elsewhere. */
static void updateStick(void) {
    const bool *ks = SDL_GetKeyboardState(NULL);
    const Uint8 LO = 0x26, HI = 0xDA; /* centre 0x80 -/+ 0x5A */
    Uint8 x = 0x80;                   /* g_joyRawX = roll  (left = LO, right = HI) */
    Uint8 y = 0x80;                   /* g_joyRawY = pitch (up   = LO, down  = HI) */

    if (ks[SDL_SCANCODE_UP] || ks[SDL_SCANCODE_KP_8]) y = LO;
    if (ks[SDL_SCANCODE_DOWN] || ks[SDL_SCANCODE_KP_2]) y = HI;
    if (ks[SDL_SCANCODE_LEFT] || ks[SDL_SCANCODE_KP_4]) x = LO;
    if (ks[SDL_SCANCODE_RIGHT] || ks[SDL_SCANCODE_KP_6]) x = HI;
    /* Keypad diagonals deflect both axes at once. */
    if (ks[SDL_SCANCODE_KP_7]) {
        y = LO;
        x = LO;
    }
    if (ks[SDL_SCANCODE_KP_9]) {
        y = LO;
        x = HI;
    }
    if (ks[SDL_SCANCODE_KP_1]) {
        y = HI;
        x = LO;
    }
    if (ks[SDL_SCANCODE_KP_3]) {
        y = HI;
        x = HI;
    }

    g_joyRawX = x;
    g_joyRawY = y;
}

/* Gamepad flight bindings ----------------------------------------------------
 * The left stick (roll/pitch) and the fire triggers (RT guns, LT missiles) are
 * read directly by the game through joystick.c. The remaining cockpit actions
 * are discrete keystrokes, so we feed the corresponding BIOS key word into the
 * same ring the keyboard uses: stepFlightModel reads one keyScancode per frame
 * and both it (throttle) and keyDispatch (egkeys.c: weapons, views,
 * countermeasures, ...) act on it. Everything is edge-triggered (one press =
 * one keystroke) except thrust, which auto-repeats while held.
 *
 * Button layout mirrors the cockpit's physical arrangement:
 *   Y  cycle weapon    X  afterburner    A  brake    B  designate target
 *   L1 chaff           R1 flare          Start autopilot   Select landing gear
 *   L3 cycle radar zoom    R3 eject
 *   D-pad up/down thrust +/-   D-pad left/right chase cam (F5) / target view
 *   (F10).  Right stick is a view hat: forward front, left/right/back look that
 *   way (forward also exits the F5/F10 cams). */

/* Selectable cockpit views and their BIOS key words. keyValue holds the active
 * view (0 front, 0x42 left, 0x43 right, 0x41 rear). */
#define VIEW_FRONT_KEY 0x3920 /* space */
#define VIEW_LEFT_KEY 0x3c00  /* F2 */
#define VIEW_RIGHT_KEY 0x3d00 /* F3 */
#define VIEW_REAR_KEY 0x3e00  /* F4 */

/* Rising edge of button b since the last poll. prev state is shared with the
 * thrust auto-repeat below; a button is driven by only one of the two. */
static bool gpPrev[SDL_GAMEPAD_BUTTON_COUNT];

static bool gpEdge(SDL_GamepadButton b) {
    bool now = joy_button(b);
    bool edge = now && !gpPrev[b];
    gpPrev[b] = now;
    return edge;
}

/* True on press and then every repeat interval while held, for throttle ramps. */
static bool gpHeldRepeat(SDL_GamepadButton b, Uint64 *nextRepeat) {
    bool now = joy_button(b);
    if (!now) {
        gpPrev[b] = false;
        return false;
    }
    Uint64 t = SDL_GetTicks();
    if (!gpPrev[b]) {
        gpPrev[b] = true;
        *nextRepeat = t + 250; /* hold this long before the first repeat */
        return true;
    }
    if (t >= *nextRepeat) {
        *nextRepeat = t + 100;
        return true;
    }
    return false;
}

/* Right stick is a 4-way view hat: push forward/left/right/back to look that
 * way (forward = front, which also drops you out of the F5/F10 external cams).
 * Acts only on zone transitions; a centred stick is neutral and makes no
 * change, so it leaves a D-pad-selected camera alone until you pick a view. */
static void rightStickView(void) {
    static int zone = 0; /* last view this stick selected; 0 = centred/neutral */
    const int GATE = 16000;
    int x = joy_axisRaw(SDL_GAMEPAD_AXIS_RIGHTX);
    int y = joy_axisRaw(SDL_GAMEPAD_AXIS_RIGHTY);
    int ax = x < 0 ? -x : x, ay = y < 0 ? -y : y;
    int z = 0;
    if (ay > GATE && ay >= ax) z = (y < 0) ? VIEW_FRONT_KEY : VIEW_REAR_KEY; /* push away / pull back */
    else if (ax > GATE) z = (x < 0) ? VIEW_LEFT_KEY : VIEW_RIGHT_KEY;

    if (z != zone) {
        if (z) ringPush(z); /* entering neutral leaves the current view as-is */
        zone = z;
    }
}

static void pollGamepadActions(void) {
    /* Weapon-select keys 's'/'m'/'g' cycle missileSpecIndex 0->1->2; rotate
     * through them so one button toggles the loadout. */
    static const uint16 weaponKeys[3] = {0x1f73, 0x326d, 0x2267};
    static int weaponSel = 0;
    static Uint64 thrustUpAt, thrustDownAt;

    if (!joy_isGamepad()) return;

    if (gpEdge(SDL_GAMEPAD_BUTTON_NORTH)) { /* Y: cycle weapon */
        ringPush(weaponKeys[weaponSel]);
        weaponSel = (weaponSel + 1) % 3;
    }
    if (gpEdge(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) ringPush(0x2e63);  /* L1: chaff ('c') */
    if (gpEdge(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) ringPush(0x2166); /* R1: flare ('f') */
    if (gpEdge(SDL_GAMEPAD_BUTTON_WEST)) ringPush(0x1e61);          /* X: afterburner ('a') */
    if (gpEdge(SDL_GAMEPAD_BUTTON_SOUTH)) ringPush(0x3062);         /* A: brake ('b') */
    if (gpEdge(SDL_GAMEPAD_BUTTON_EAST)) ringPush(0x1474);          /* B: designate target ('t') */
    if (gpEdge(SDL_GAMEPAD_BUTTON_START)) ringPush(0x1970);         /* Start: autopilot ('p') */
    if (gpEdge(SDL_GAMEPAD_BUTTON_BACK)) ringPush(0x266c);          /* Select: landing gear ('l') */
    if (gpEdge(SDL_GAMEPAD_BUTTON_LEFT_STICK)) ringPush(0x1372);    /* L3: cycle radar zoom ('r') */
    if (gpEdge(SDL_GAMEPAD_BUTTON_RIGHT_STICK)) ringPush(0x011b);   /* R3: eject (Esc) */

    if (gpEdge(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) ringPush(0x3f00);     /* D-pad left: chase cam (F5) */
    if (gpEdge(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) ringPush(0x4400);    /* D-pad right: target view (F10) */
    rightStickView();

    /* Thrust ramps while up/down is held. */
    if (gpHeldRepeat(SDL_GAMEPAD_BUTTON_DPAD_UP, &thrustUpAt)) ringPush(0x0d3d);     /* '=' */
    if (gpHeldRepeat(SDL_GAMEPAD_BUTTON_DPAD_DOWN, &thrustDownAt)) ringPush(0x0c2d); /* '-' */
}

/* Drain the SDL event queue into the key ring and refresh the virtual stick.
 * Called from every kbhit()/egReadKey() the flight loop makes, so input and
 * the window stay current frame by frame. */
static void pumpInput(void) {
    SDL_Event ev;
    timerPump();
    while (SDL_PollEvent(&ev)) {
        joy_handleEvent(&ev);
        switch (ev.type) {
        case SDL_EVENT_QUIT:
            ringPush(0x1000); /* feed Alt+Q so the mission quits cleanly */
            break;
        case SDL_EVENT_KEY_DOWN: {
            /* Alt+Enter toggles fullscreen; swallow it so it never reaches the
             * key ring as an Alt-shifted Return. */
            if (ev.key.scancode == SDL_SCANCODE_RETURN && (ev.key.mod & SDL_KMOD_ALT)) {
                gfx_toggleFullscreen();
                break;
            }
            uint16 word = biosWord(ev.key.scancode, ev.key.mod);
            if (word) ringPush(word);
            break;
        }
        default:
            break;
        }
    }
    updateStick();
    pollGamepadActions();
}

/* Blocking read, scan code in AH and ASCII in AL (INT 16h function 0). The
 * flight loop only calls this after kbhit() reports a key, but block like the
 * BIOS would for safety. */
int egReadKey(void) {
    uint16 word;
    pumpInput();
    while (ringHead == ringTail) {
        SDL_Delay(2);
        pumpInput();
    }
    word = keyRing[ringHead];
    ringHead = (ringHead + 1) % EGKEY_RING;
    return word;
}

/* Non-zero when a key word is waiting. */
int kbhit(void) {
    pumpInput();
    return (ringHead != ringTail);
}

int far setInt9Handler(void) {
    ringHead = ringTail = 0;
    g_joyRawX = 0x80;
    g_joyRawY = 0x80;
    return 0;
}

int far restoreInt9Handler(void) {
    return 0;
}
