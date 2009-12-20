#include "plugin.h"
#include "lib/helper.h"
#include "lib/highscore.h"
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

#elif (CONFIG_KEYPAD == SANSA_E200_PAD) || \
      (CONFIG_KEYPAD == SANSA_FUZE_PAD)   
#define BTN_SELECT  BUTTON_SELECT
#define BTN_MENU    BUTTON_POWER
#define BTN_UP      BUTTON_SCROLL_BACK
#define BTN_DOWN    BUTTON_SCROLL_FWD
#define BTN_LEFT    BUTTON_LEFT
#define BTN_RIGHT   BUTTON_RIGHT
      
#else
#error No keymap defined!
#endif

#define FILEPATH PLUGIN_GAMES_DIR "/yahtzee.score" 
#define NUM_SCORES 5

static const int FACE_SIZE = 30;
static const int PIP_SIZE = 6;
static const unsigned LCD_RED    = LCD_RGBPACK(255,0,0);
static const unsigned LCD_YELLOW = LCD_RGBPACK(255,255,0);
static const unsigned LCD_GREEN  = LCD_RGBPACK(0,255,0);
static const unsigned LCD_GREY   = LCD_RGBPACK(105,105,105);

struct Scorecard {
    int utotal;     /* Upper section's total score */
    int bonus;      /* 35 if upper section's score is 63 or over */
    int ltotal;     /* Lower section's total score */
    int score;      /* Upper total + bonus + lower total */
} game;

struct highscore scores[NUM_SCORES];

typedef struct Option {
    char *label;    /* Textual explanation of dice combos */
    bool disabled;  /* If true, option can't be selected by the player */
} Option;

Option *cursor;     /* Tracks player input */
Option opts[] = {
    { "Ones",             true },    /* 0  */
    { "Twos",             true },    /* 1  */
    { "Threes",           true },    /* 2  */
    { "Fours",            true },    /* 3  */
    { "Fives",            true },    /* 4  */
    { "Sixes",            true },    /* 5  */
    { "3 of a Kind",      true },    /* 6  */
    { "4 of a Kind",      true },    /* 7  */
    { "Full House (25)",  true },    /* 8  */
    { "Sm Straight (30)", true },    /* 9  */
    { "Lg Straight (40)", true },    /* 10 */
    { "Yahtzee (50)",     true },    /* 11 */
    { "Chance",           true },    /* 12 */
    { "",                 true },    /* 13 - Die 1 */
    { "",                 true },    /* 14 - Die 2 */
    { "",                 true },    /* 15 - Die 3 */
    { "",                 true },    /* 16 - Die 4 */
    { "",                 true },    /* 17 - Die 5 */
    { "",                 false }    /* 18 - Roll! */
};
#define NUM_OPTS (sizeof opts / sizeof opts[0])
enum {
    THREE_OF_A_KIND = 6,
    FOUR_OF_A_KIND  = 7,
    FULL_HOUSE      = 8,
    SM_STRAIGHT     = 9,
    LG_STRAIGHT     = 10,
    YAHTZEE         = 11,
    CHANCE          = 12,
};

typedef struct Die {
    int value;      /* The value of the die (ex: 1, 3, 6) */
    bool held;      /* If true, exclude this die from re-rolls */
} Die;

Die dice[] = {      /* Array of dice for the player to roll */
    { 0, true },
    { 0, true },
    { 0, true },
    { 0, true },
    { 0, true }
};
#define NUM_DICE (sizeof dice / sizeof dice[0])

MENUITEM_STRINGLIST(menu, "Yahtzee Menu", NULL, 
    "Play Game", 
    "High Scores", 
    "Quit"
);
enum { 
    MENU_PLAY   = 0, 
    MENU_SCORES = 1, 
    MENU_QUIT   = 2 
};

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

    default: /* No pips */
        break;
    }
}

void draw_dice()
{
    Die *d;
    int ypos, xpos = (LCD_WIDTH/2) + (LCD_WIDTH/4) + 1; 
    
    for (d = dice, ypos = 13; d < dice + NUM_DICE; d++, ypos += 35) 
        draw_die(xpos, ypos, d->value, false, d->held);
}

void draw_combos()
{
    int i, xpos, offset = (LCD_WIDTH/2) + 15; 

    /* Upper section - multiples of 1 through 6 */
    for (i = 5, xpos = offset; i >= 0; i--, xpos -= 20) 
        draw_die(xpos, 10, i+1, true, opts[i].disabled);
    
    /* 3 of a kind */
    for (i = 0, xpos = offset; i < 3; i++, xpos -= 20) 
        draw_die(xpos, 50, 1, true, opts[THREE_OF_A_KIND].disabled);
    
    /* 4 of a kind */
    for (i = 0, xpos = offset; i < 4; i++, xpos -= 20) 
        draw_die(xpos, 70, 1, true, opts[FOUR_OF_A_KIND].disabled);

    /* Full house */
    for (i = 0, xpos = offset; i < 3; i++, xpos -= 20) 
        draw_die(xpos, 90, 2, true, opts[FULL_HOUSE].disabled);
    for (i = 0; i < 2; i++, xpos -= 20) 
        draw_die(xpos, 90, 1, true, opts[FULL_HOUSE].disabled);

    /* Small straight */
    for (i = 4, xpos = offset; i > 0; i--, xpos -= 20) 
        draw_die(xpos, 110, i, true, opts[SM_STRAIGHT].disabled);

    /* Large straight */
    for (i = 5, xpos = offset; i > 0; i--, xpos -= 20) 
        draw_die(xpos, 130, i, true, opts[LG_STRAIGHT].disabled);

    /* Yahtzee */
    for (i = 0, xpos = offset; i < 5; i++, xpos -= 20) 
        draw_die(xpos, 150, 1, true, opts[YAHTZEE].disabled);

    /* Chance */
    for (i = 0, xpos = offset; i < 5; i++, xpos -= 20) 
        draw_die(xpos, 170, 7, true, opts[CHANCE].disabled);
}

int draw_text(int rolls)
{
    int width, height;
    char score[20], roll[20];
    rb->lcd_set_foreground(LCD_RED);

    /* Selected combo label */
    rb->lcd_getstringsize(cursor->label, &width, &height);
    rb->lcd_putsxy(10, LCD_HEIGHT - (height * 2) - 2, cursor->label);

    /* Player's score */
    rb->snprintf(score, sizeof(score), "Score: %d", game.score);
    rb->lcd_getstringsize(score, &width, &height);
    rb->lcd_putsxy(10, LCD_HEIGHT - height - 2, score);

    /* "Roll" option */
    if (rolls > 0)
        rb->snprintf(roll, sizeof(roll), "ROLL %d", rolls);
    else
        rb->strcpy(roll, "ROLL");
    rb->lcd_getstringsize(roll, &width, &height);
    rb->lcd_putsxy(LCD_WIDTH-width - 10, LCD_HEIGHT - height - 2, roll);
    return (LCD_WIDTH - width - 10);
}

void draw_cursor(int tpos)
{
    int i, xpos, ypos;
    rb->lcd_set_foreground(LCD_YELLOW);

    /* Upper section combos */
    xpos = (LCD_WIDTH/2) + 20;
    for (i = 5; i >= 0; i--) {
        if (cursor == opts + i) {
            rb->lcd_fillrect(xpos, 28, PIP_SIZE, PIP_SIZE);
            return;
        } else {
            xpos -= 20;
        }
    }
    
    /* Lower section combos */
    for (i = 6, ypos = 55; i <= 12; i++) {
        if (cursor == opts + i) {
            xpos = (LCD_WIDTH/2) + PIP_SIZE + 27;
            rb->lcd_fillrect(xpos, ypos, PIP_SIZE, PIP_SIZE);
            return;
        } else {
            ypos += 20;
        }
    }
    
    /* Player's dice */
    for (i = 13, ypos = 25; i <= 17; i++) {
        if (cursor == opts + i) {
            xpos = (LCD_WIDTH/2) + (LCD_WIDTH/4) + FACE_SIZE + 4;
            rb->lcd_fillrect(xpos, ypos, PIP_SIZE, PIP_SIZE);
            return;
        } else {
            ypos += 35;
        }
    }
    
    /* "Roll" option */
    if (cursor == opts + (NUM_OPTS - 1)) {
        xpos = (LCD_WIDTH/2) - PIP_SIZE - 5;
        rb->lcd_fillrect(tpos-PIP_SIZE-5, LCD_HEIGHT-10, PIP_SIZE, PIP_SIZE);
    }
}

void set_combos(bool disabled)
{
    Option *o;
    for (o = opts; o < opts + NUM_OPTS; o++)
        o->disabled = disabled;
}

void set_dice(bool disabled)
{
    Die *d;
    for (d = dice; d < dice + NUM_DICE; d++)
        d->held = disabled;

    Option *o;
    for (o = opts + 13; o <= opts + 17; o++)
        o->disabled = disabled;
}

void roll_dice()
{
    Die *d;
    for (d = dice; d < dice + NUM_DICE; d++) {
        if (!d->held)
            d->value = rb->rand() % 6 + 1; /* Random number 1-6 */
    }
}

int get_dice_sum()
{
    Die *d;
    int sum = 0;
    for (d = dice; d < dice + NUM_DICE; d++)
        sum += d->value;
    return sum;
}

void calculate_score(int combo, bool *bonus_applied, int *yahtzees)
{
    Die *d;

    /* Upper section combos */
    if (combo < THREE_OF_A_KIND) {
        for (d = dice; d < dice + NUM_DICE; d++) {
            if (d->value == combo + 1)
                game.utotal += d->value;
        }
        if (!game.utotal >= 63 && !*bonus_applied) {
            game.bonus = 35;
            *bonus_applied = true;   
        }

    /* Lower section combos */
    } else {
        int i, seq = 0;
        int freq[6];        /* Number of occurences of each die value */
        bool pair, triple;  /* Used in testing for a Full House */
        
        for (i = 0; i < 6; freq[i++] = 0); /* Fill freq[] with zeroes */
        for (d = dice; d < dice + NUM_DICE; d++)
            freq[d->value-1]++;
        
        switch(combo) {
        
        /* At least three dice showing the same face */
        case THREE_OF_A_KIND:
            for (i = 0; i < 6; i++) {
                if (freq[i] >= 3) {
                    game.ltotal += get_dice_sum(); 
                    break;
                }
            }
            break;

        /* At least four dice showing the same face */
        case FOUR_OF_A_KIND: 
            for (i = 0; i < 6; i++) {
                if (freq[i] >= 4) {
                    game.ltotal += get_dice_sum(); 
                    break;
                }
            }
            break;

        /* A three-of-a-kind and a pair */
        case FULL_HOUSE: 
            pair = false, triple = false;
            for (i = 0; i < 6; i++) {
                if (freq[i] == 2)
                    pair = true;
                else if (freq[i] == 3)
                    triple = true;
            }
            if (pair && triple)
                game.ltotal += 25;
            break;

        /* Four sequential dice (ex: 1-2-3-4) */
        case SM_STRAIGHT: 
            for (i = 0; i < 6; i++) {
                seq = (freq[i] >= 1) ? seq + 1 : 0; 
                if (seq == 4) {
                    game.ltotal += 30;
                    break;
                }   
            }
            break;

        /* Five sequential dice (ex: 1-2-3-4-5) */
        case LG_STRAIGHT: 
            for (i = 0; i < 6; i++) {
                seq = (freq[i] == 1) ? seq + 1 : 0; 
                if (seq == 5) {
                    game.ltotal += 40;
                    break;
                }   
            }
            break;

        /* All five dice showing the same face */
        case YAHTZEE: 
            for (i = 0; i < 6; i++) {
                if (freq[i] == 5) {
                    game.ltotal += (*yahtzees == 0) ? 50 : 100;
                    *yahtzees += 1;
                    break;
                }
            }
            break;

        /* Any combination */
        case CHANCE: 
            game.ltotal += get_dice_sum();
            break;
        }
    }
    game.score = game.utotal + game.bonus + game.ltotal;
}

void reset_game()
{
    Die *d;

    /* Clear the scorecard */    
    game.utotal = 0;    
    game.bonus = 0;
    game.ltotal = 0;
    game.score = 0;
    
    /* Disable combos and rolled dice */
    set_combos(true);
    set_dice(true);
    
    /* Clear the faces of rolled dice */
    for (d = dice; d < dice + NUM_DICE; d->value = 0, d++);
    
    cursor = opts + (NUM_OPTS - 1); /* Set cursor to "Roll" option */
}

void enter_game_loop()
{
    int button, i, rank, xpos, rolls = 0, yahtzees = 0;
    bool bonus_applied, combos_left, start = true;
    Option *o;

    reset_game();

    while (true) {

        /* If there no unused dice combos left, exit the loop */
        for (o = opts, combos_left = false; o <= opts + CHANCE; o++) {
            if (!o->disabled) {
                combos_left = true;
                break;
            }
        }
        if (!combos_left && !start)
            goto done;

        /* Draw everything to the screen */
        rb->lcd_clear_display();
        draw_combos();
        draw_dice();
        xpos = draw_text(rolls);
        draw_cursor(xpos);
        rb->lcd_update();

        /* Block on player input */
        button = rb->button_get(true);
        switch (button) {
        case BTN_SELECT:

            /* Dice combo selection */
            for (i = 0; i <= 12; i++) {
                if (cursor == (opts + i) && !cursor->disabled) {
                    calculate_score(i, &bonus_applied, &yahtzees);
                    cursor->disabled = true;
                    
                    /* Reset the dice for next turn */
                    rolls = 0;
                    roll_dice();
                    set_dice(false);      
                    cursor = opts + (NUM_OPTS - 1);
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
            
            /* "Roll" option selection */
            if (cursor == (opts + (NUM_OPTS - 1))) {
                if (start) {
                    set_combos(false);
                    set_dice(false);
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
            if (!start)
                cursor = (cursor == opts) ? opts + (NUM_OPTS - 1) : cursor - 1;
            break;

        case BTN_RIGHT:
        case BTN_DOWN:
            if (!start)
                cursor = (cursor == opts + (NUM_OPTS - 1)) ? opts : cursor + 1;
            break;

        case BTN_MENU:
            return; /* Exit game without updating high scores */
            break; 

        default: /* Undefined key - do nothing */
            break;
        }
    }  

done: 
    /* Update high scores and exit */
    rank = highscore_update(game.score, -1, "", scores, NUM_SCORES);  
    if (rank != -1) {
        rb->splash(HZ*3, "NEW HIGH SCORE!");
        highscore_show(rank, scores, NUM_SCORES, false);
    }
}

void open_menu(bool *quit)
{
    int result, selected = 0;

    rb->lcd_clear_display();
    rb->lcd_update();
    rb->button_clear_queue();

    result = rb->do_menu(&menu, &selected, NULL, false);
    switch(result) {
    case MENU_PLAY:
        enter_game_loop();
        break;
        
    case MENU_SCORES:
        highscore_show(NUM_SCORES, scores, NUM_SCORES, false);
        break;
    
    case MENU_QUIT:
        *quit = true;
        break;
    }
}

enum plugin_status plugin_start(const void* parameter)
{
    (void)parameter;
    bool quit = false;
    
    highscore_load(FILEPATH, scores, NUM_SCORES);
    
    rb->lcd_set_backdrop(NULL);     /* Remove the Rockbox background */
    rb->lcd_set_background(LCD_BLACK);
    rb->srand(*rb->current_tick);   /* Seed the random number generator */
    backlight_force_on();           /* Don't let the backlight turn off! */

    while (!quit) 
        open_menu(&quit);

    highscore_save(FILEPATH, scores, NUM_SCORES);
    
    backlight_use_settings();       /* Return backlight control to Rockbox */
    return PLUGIN_OK;               /* Exit plugin */
}

