#include "plugin.h"
#include "lib/helper.h"
#include "lib/playback_control.h"

PLUGIN_HEADER

#if (CONFIG_KEYPAD == IPOD_1G2G_PAD) || \
    (CONFIG_KEYPAD == IPOD_3G_PAD)   || \
    (CONFIG_KEYPAD == IPOD_4G_PAD)
#define BTN_SELECT  BUTTON_SELECT
#define BTN_MENU    BUTTON_MENU
#define BTN_UP      BUTTON_SCROLL_BACK
#define BTN_DOWN    BUTTON_SCROLL_FWD
#define BTN_LEFT    BUTTON_LEFT
#define BTN_RIGHT   BUTTON_RIGHT
#else
#error No keymap defined!
#endif

static const int FACE_SIZE = 30;
static const int PIP_SIZE = 6;
static const unsigned LCD_GREY = LCD_RGBPACK(105,105,105);

struct game_context {
    int utotal;     /* Upper section's total score */
    int bonus;      /* 35 if upper section's score is 63 or over */
    int ltotal;     /* Lower section's total score */
    int score;      /* Upper total + bonus + lower total */
};
struct game_context game = { 0, 0, 0, 0 };

typedef struct game_option {
    char *label;    /* Textual explanation of dice combos */
    bool disabled;  /* Can this option be selected by the player? */
} Option;

Option *cursor;
Option opts[] = {
    { "Ones",         true },    /* 0  */
    { "Twos",         true },    /* 1  */
    { "Threes",       true },    /* 2  */
    { "Fours",        true },    /* 3  */
    { "Fives",        true },    /* 4  */
    { "Sixes",        true },    /* 5  */
    { "3 of a Kind",  true },    /* 6  */
    { "4 of a Kind",  true },    /* 7  */
    { "Full House",   true },    /* 8  */
    { "Sm Straight",  true },    /* 9  */
    { "Lg Straight",  true },    /* 10 */
    { "Yahtzee",      true },    /* 11 */
    { "Chance",       true },    /* 12 */
    { "",             true },    /* 13 - Die 1 */
    { "",             true },    /* 14 - Die 2 */
    { "",             true },    /* 15 - Die 3 */
    { "",             true },    /* 16 - Die 4 */
    { "",             true },    /* 17 - Die 5 */
    { "",             false }    /* 18 - Roll Dice */
};
#define NOPTS (sizeof opts / sizeof opts[0])

#define THREE_OF_A_KIND 6
#define FOUR_OF_A_KIND  7
#define FULL_HOUSE      8
#define SM_STRAIGHT     9
#define LG_STRAIGHT     10
#define YAHTZEE         11
#define CHANCE          12

typedef struct rolled_die {
    int value;      /* The value of the die (ex: 1, 3, 6) */
    bool held;      /* Does the player want to hold this die? */
} Die;

Die dice[] = {      /* Array of dice for the player to roll */
    { 1, true },
    { 2, true },
    { 3, true },
    { 4, true },
    { 5, true }
};
#define NDICE (sizeof dice / sizeof dice[0])

void draw_top_left(int xpos, int ypos, int psize)
{
    rb->lcd_fillrect(xpos + psize - (psize/2),
                     ypos + psize - (psize/2),
                     psize, psize);
}

void draw_top_center(int xpos, int ypos, int fsize, int psize)
{
    rb->lcd_fillrect(xpos + (fsize/2) - (psize/2),
                     ypos + psize - (psize/2),
                     psize, psize);
}

void draw_top_right(int xpos, int ypos, int fsize, int psize)
{
    rb->lcd_fillrect(xpos + fsize - psize - (psize/2),
                     ypos + (psize/2),
                     psize, psize);
}

void draw_middle_left(int xpos, int ypos, int fsize, int psize)
{
    rb->lcd_fillrect(xpos + psize - (psize/2),
                     ypos + (fsize/2) - (psize/2),
                     psize, psize);
}

void draw_middle_center(int xpos, int ypos, int fsize, int psize)
{
    rb->lcd_fillrect(xpos + (fsize/2) - (psize/2),
                     ypos + (fsize/2) - (psize/2),
                     psize, psize);
}

void draw_middle_right(int xpos, int ypos, int fsize, int psize)
{
    rb->lcd_fillrect(xpos + fsize - psize - (psize/2),
                     ypos + (fsize/2) - (psize/2),
                     psize, psize);
}

void draw_bottom_left(int xpos, int ypos, int fsize, int psize)
{
    rb->lcd_fillrect(xpos + psize - (psize/2),
                     ypos + fsize - psize - (psize/2),
                     psize, psize);
}

void draw_bottom_center(int xpos, int ypos, int fsize, int psize)
{
    rb->lcd_fillrect(xpos + (fsize/2) - (psize/2),
                     ypos + fsize - psize - (psize/2),
                     psize, psize);
}

void draw_bottom_right(int xpos, int ypos, int fsize, int psize)
{
    rb->lcd_fillrect(xpos + fsize - psize - (psize/2),
                     ypos + fsize - psize - (psize/2),
                     psize, psize);
}

void draw_die(int xpos, int ypos, int value, bool is_small, bool disabled)
{
    int fsize = (is_small) ? FACE_SIZE / 2 : FACE_SIZE;
    int psize = (is_small) ? PIP_SIZE / 2 : PIP_SIZE;

    /* Draw the face of the die */
    unsigned face_color = (disabled) ? LCD_GREY : LCD_WHITE;
    rb->lcd_set_foreground(face_color);
    rb->lcd_fillrect(xpos, ypos, fsize, fsize);

    /* Draw pips on the face of the die */
    rb->lcd_set_foreground(LCD_BLACK);
    switch (value) {
    case 1:
        draw_middle_center(xpos, ypos, fsize, psize);
        break;

    case 2:
        draw_top_right(xpos, ypos, fsize, psize);
        draw_bottom_left(xpos, ypos, fsize, psize);
        break;

    case 3:
        draw_top_right(xpos, ypos, fsize, psize);
        draw_middle_center(xpos, ypos, fsize, psize);
        draw_bottom_left(xpos, ypos, fsize, psize);
        break;

    case 4:
        draw_top_left(xpos, ypos, psize);
        draw_top_right(xpos, ypos, fsize, psize);
        draw_bottom_left(xpos, ypos, fsize, psize);
        draw_bottom_right(xpos, ypos, fsize, psize);
        break;

    case 5:
        draw_top_left(xpos, ypos, psize);
        draw_top_right(xpos, ypos, fsize, psize);
        draw_middle_center(xpos, ypos, fsize, psize);
        draw_bottom_left(xpos, ypos, fsize, psize);
        draw_bottom_right(xpos, ypos, fsize, psize);
        break;

    case 6:
        draw_top_left(xpos, ypos, psize);
        draw_top_right(xpos, ypos, fsize, psize);
        draw_middle_left(xpos, ypos, fsize, psize);
        draw_middle_right(xpos, ypos, fsize, psize);
        draw_bottom_left(xpos, ypos, fsize, psize);
        draw_bottom_right(xpos, ypos, fsize, psize);
        break;

    default: /* No pips - only used for Chance */
        break;
    }
}

void draw_dice()
{
    Die *d;
    int offset = 15;
    int xpos = (LCD_WIDTH/2) + (LCD_WIDTH/4) - (FACE_SIZE/2);
    for (d = dice; d < dice + NDICE; d++) {
        draw_die(xpos, offset, d->value, false, d->held);
        offset += 35;
    }
}

void draw_combos()
{
    /* Upper section - multiples of 1 through 6 */
    int i, offset = (LCD_WIDTH/2) - (FACE_SIZE/2);
    for (i = 5; i >= 0; i--) {
        draw_die(offset, 10, i+1, true, opts[i].disabled);
        offset -= 20;
    }
    
    /* 3 of a kind */
    offset = (LCD_WIDTH/2) - (FACE_SIZE/2);
    for (i = 0; i < 3; i++) {
        draw_die(offset, 50, 1, true, opts[THREE_OF_A_KIND].disabled);
        offset -= 20;
    }
    
    /* 4 of a kind */
    offset = (LCD_WIDTH/2) - (FACE_SIZE/2);
    for (i = 0; i < 4; i++) {
        draw_die(offset, 70, 1, true, opts[FOUR_OF_A_KIND].disabled);
        offset -= 20;
    }

    /* Full house */
    offset = (LCD_WIDTH/2) - (FACE_SIZE/2);
    for (i = 0; i < 3; i++) {
        draw_die(offset, 90, 2, true, opts[FULL_HOUSE].disabled);
        offset -= 20;
    }
    for (i = 0; i < 2; i++) {
        draw_die(offset, 90, 1, true, opts[FULL_HOUSE].disabled);
        offset -= 20;
    }

    /* Small straight */
    offset = (LCD_WIDTH/2) - (FACE_SIZE/2);
    for (i = 4; i > 0; i--) {
        draw_die(offset, 110, i, true, opts[SM_STRAIGHT].disabled);
        offset -= 20;
    }

    /* Large straight */
    offset = (LCD_WIDTH/2) - (FACE_SIZE/2);
    for (i = 5; i > 0; i--) {
        draw_die(offset, 130, i, true, opts[LG_STRAIGHT].disabled);
        offset -= 20;
    }

    /* Yahtzee */
    offset = (LCD_WIDTH/2) - (FACE_SIZE/2);
    for (i = 0; i < 5; i++) {
        draw_die(offset, 150, 1, true, opts[YAHTZEE].disabled);
        offset -= 20;
    }

    /* Chance */
    offset = (LCD_WIDTH/2) - (FACE_SIZE/2);
    for (i = 0; i < 5; i++) {
        draw_die(offset, 170, 7, true, opts[CHANCE].disabled);
        offset -= 20;
    }
}

void draw_text()
{
    char score[24];
    rb->lcd_set_foreground(LCD_WHITE);

    /* Selected combo label */
    rb->lcd_putsxy(10, LCD_HEIGHT-30, cursor->label);

    /* Player's score */
    game.score = game.utotal + game.bonus + game.ltotal;
    rb->snprintf(score, sizeof(score), "Score: %d", game.score);
    rb->lcd_putsxy(10, LCD_HEIGHT-14, score);

    /* "Roll Dice" option */
    rb->lcd_putsxy((LCD_WIDTH/2), LCD_HEIGHT-14, "ROLL DICE");
}

void draw_cursor()
{
    int i, xoffset, yoffset;
    rb->lcd_set_foreground(LCD_WHITE);

    /* Upper section combos */
    xoffset = (LCD_WIDTH/2) - 10;
    for (i = 5; i >= 0; i--) {
        if (cursor == opts + i) {
            rb->lcd_fillrect(xoffset, 30, PIP_SIZE, PIP_SIZE);
            return;
        } else {
            xoffset -= 20;
        }
    }
    
    /* Lower section combos */
    yoffset = 55;
    for (i = 6; i <= 12; i++) {
        if (cursor == opts + i) {
            int xpos = (LCD_WIDTH/2) + PIP_SIZE;
            rb->lcd_fillrect(xpos, yoffset, PIP_SIZE, PIP_SIZE);
            return;
        } else {
            yoffset += 20;
        }
    }
    
    /* Player's dice */
    yoffset = 25;
    for (i = 13; i <= 17; i++) {
        if (cursor == opts + i) {
            int xpos = (LCD_WIDTH/2) + (LCD_WIDTH/4) + (FACE_SIZE/2) + 5;
            rb->lcd_fillrect(xpos, yoffset, PIP_SIZE, PIP_SIZE);
            return;
        } else {
            yoffset += 35;
        }
    }
    
    /* "Roll Dice" option */
    if (cursor == opts + (NOPTS - 1)) {
        int xpos = (LCD_WIDTH/2) - PIP_SIZE - 5;
        rb->lcd_fillrect(xpos, LCD_HEIGHT-10, PIP_SIZE, PIP_SIZE);
    }
}

void enable_combos()
{
    Option *o;
    for (o = opts; o < opts + NOPTS; o++)
        o->disabled = false;
}

void enable_dice()
{
    Die *d;
    for (d = dice; d < dice + NDICE; d++)
        d->held = false;

    Option *o;
    for (o = opts + 13; o <= opts + 17; o++)
        o->disabled = false;
}

void roll_dice()
{
    Die *d;
    for (d = dice; d < dice + NDICE; d++) {
        if (!d->held)
            d->value = rb->rand() % 6 + 1; /* Random number 1-6 */
    }
}

int get_dice_sum()
{
    Die *d;
    int sum = 0;
    for (d = dice; d < dice + NDICE; d++)
        sum += d->value;
    return sum;
}

void calculate_score(int combo, int *yahtzees)
{
    Die *d;

    /* Upper section combos */
    if (combo < THREE_OF_A_KIND) {
        for (d = dice; d < dice + NDICE; d++) {
            if (d->value == combo + 1)
                game.utotal += d->value;
        }
        game.bonus = (game.utotal >= 63) ? 35 : 0;

    /* Lower section combos */
    } else {
        int i, seq = 0;
        int freq[6] = { 0, 0, 0, 0, 0, 0 };
        bool pair = false, triple = false;
        
        for (d = dice; d < dice + NDICE; d++)
            freq[d->value-1]++;
        
        switch(combo) {
        case THREE_OF_A_KIND:
            for (i = 0; i < 6; i++) {
                if (freq[i] >= 3) {
                    game.ltotal += get_dice_sum(); 
                    break;
                }
            }
            break;

        case FOUR_OF_A_KIND:
            for (i = 0; i < 6; i++) {
                if (freq[i] >= 4) {
                    game.ltotal += get_dice_sum(); 
                    break;
                }
            }
            break;

        case FULL_HOUSE:
            for (i = 0; i < 6; i++) {
                if (freq[i] == 2)
                    pair = true;
                else if (freq[i] == 3)
                    triple = true;
            }
            if (pair && triple)
                game.ltotal += 25;
            break;

        case SM_STRAIGHT:
            for (i = 0; i < 6; i++) {
                seq = (freq[i] >= 1) ? seq + 1 : 0; 
                if (seq == 4) {
                    game.ltotal += 30;
                    break;
                }   
            }
            break;

        case LG_STRAIGHT:
            for (i = 0; i < 6; i++) {
                seq = (freq[i] == 1) ? seq + 1 : 0; 
                if (seq == 5) {
                    game.ltotal += 40;
                    break;
                }   
            }
            break;

        case YAHTZEE:
            for (i = 0; i < 6; i++) {
                if (freq[i] == 5) {
                    game.ltotal += (*yahtzees == 0) ? 50 : 100;
                    *yahtzees += 1;
                    break;
                }
            }
            break;

        case CHANCE:
            game.ltotal += get_dice_sum();
            break;
        }
    }
}

enum plugin_status plugin_start(const void* parameter)
{
    (void)parameter;
    int button, i, rolls = 0, yahtzees = 0;
    bool start = true, done = false;
   
    rb->srand(*rb->current_tick);   /* Seed the random number generator */
    backlight_force_on();           /* Don't let the backlight turn off! */
    cursor = opts + (NOPTS - 1);    /* Set cursor to "Roll Dice" option */

    while (!done) {

        /* Draw everything to the screen */
        rb->lcd_clear_display();
        draw_text();
        draw_combos();
        draw_dice();
        draw_cursor();
        rb->lcd_update();

        /* Block on player input */
        button = rb->button_get(true);
        switch (button) {
        case BTN_SELECT:

            /* Dice combo selection */
            for (i = 0; i <= 12; i++) {
                if (cursor == (opts + i) && !cursor->disabled) {
                    calculate_score(i, &yahtzees);
                    cursor->disabled = true;
                    
                    /* Reset the dice for next turn */
                    enable_dice();  
                    roll_dice();    
                    rolls = 0;
                    cursor = opts + (NOPTS - 1);
                    break;
                }
            }
            
            /* Player's dice selection */
            for (i = 13; i <= 17; i++) {
                if (cursor == (opts + i)) { 
                    dice[i-13].held = !dice[i-13].held;
                    cursor->disabled = !cursor->disabled;
                    break;
                }
            }
            
            /* "Roll Dice" option selection */
            if (cursor == (opts + (NOPTS - 1))) {
                if (start) {
                    enable_combos();
                    enable_dice();
                    start = false;
                }
                if (rolls < 3) {
                    roll_dice();
                    rolls++;
                }
            }
            break;

        case BTN_LEFT:
        case BTN_UP:
            cursor = (cursor == opts) ? opts + (NOPTS - 1) : cursor - 1;
            break;

        case BTN_RIGHT:
        case BTN_DOWN:
            cursor = (cursor == opts + (NOPTS - 1)) ? opts : cursor + 1;
            break;

        case BTN_MENU:
            done = true;
            break;

        default:
            break;
        }
    }

    backlight_use_settings(); /* Return backlight control to Rockbox */
    return PLUGIN_OK;
}

