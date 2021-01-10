#include <algorithm>
#include <cstring>
#include <random>

#include "babble.hpp"

#include "assets.hpp"
#include "bit-reader.hpp"

using namespace blit;

class Tile
{
public:
    char letter = 'A';
    bool selected = false;
};

class Word
{
public:
    Word(const char *text)
    {
        memcpy(this->text, text, 8);
    }
    char text[8];
    bool found = false;
};

static const int max_games = 0xFF;
struct SaveData
{
    struct
    {
        uint8_t score;
        uint8_t time;
    } games[max_games];
};

SaveData high_scores;

const Font tall_font(tall_font_data);
const Font large_font(large_font_data);
Surface *game_sprites;

int num_games = 0;

int selected_tile = 0, selected_menu_option = 1;

int current_game;
Tile tiles[7];
std::string current_word;
std::vector<Word> words;
int targets[3];

std::string message;
Pen message_bg;

const int game_length = 90000;
int timer;
uint32_t start_time, end_time;
int score = 0;
int level = 0;

bool game_ended = false;

static void skip_game(BitReader &bits)
{
    bits.skip_bits(6 * 3 + 5 * 7); // targets, full word

    int num_words = bits.read_bits(6);

    for(int i = 0; i < num_words; i++)
    {
        int c = bits.read_bits(3);
        while(c != 7)
            c = bits.read_bits(3);
    }
}

static void count_num_games()
{
    BitReader bits(word_data, word_data_length);

    while(!bits.eof())
    {
        skip_game(bits);
        num_games++;
    }
}

static void init_word()
{
    // grab a random game
    current_game = blit::random() % num_games;

    BitReader bits(word_data, word_data_length);

    for(int i = 0; i < current_game; i++)
        skip_game(bits);

    targets[0] = bits.read_bits(6);
    targets[1] = bits.read_bits(6);
    targets[2] = bits.read_bits(6);

    char full_word[8]{0};

    for(int i = 0; i < 7; i++)
        full_word[i] = 'a' + bits.read_bits(5);

    int num_words = bits.read_bits(6);
    
    words.clear();
    words.push_back(Word(full_word));

    for(int i = 0; i < num_words; i++)
    {
        int c = bits.read_bits(3);
        char word[8];
        int j = 0;
        while(c != 7)
        {
            word[j++] = full_word[c];
            c = bits.read_bits(3);
        }
        word[j] = 0;

        words.push_back(Word(word));
    }

    std::mt19937 rand(blit::random());
    std::shuffle(std::begin(full_word), std::end(full_word) - 1, rand);

    for(int i = 0; i < 7; i++)
        tiles[i].letter = full_word[i];
}

static void init_game()
{
    for(auto &t : tiles)
        t.selected = false;

    current_word = "";

    for(auto &w : words)
        w.found = false;

    message = "Press Y to guess";
    message_bg = Pen();

    start_time = now();
    score = level = 0;

    game_ended = false;
}

void init()
{
    set_screen_mode(ScreenMode::hires);

    game_sprites = Surface::load(game_sheet);

    if(!read_save(high_scores))
        memset(&high_scores, 0, sizeof(high_scores));

    // count lines in the word data
    count_num_games();

    init_word();
    init_game();
}

static void render_selection(Rect item_rect)
{
    Rect selection_sprite(2, 0, 2, 2);

    screen.sprite(selection_sprite, item_rect.tl() + Point( -6,  -6));
    screen.sprite(selection_sprite, item_rect.tr() + Point(-10,  -6), SpriteTransform::HORIZONTAL);
    screen.sprite(selection_sprite, item_rect.bl() + Point( -6, -10), SpriteTransform::VERTICAL);
    screen.sprite(selection_sprite, item_rect.br() + Point(-10, -10), SpriteTransform::HORIZONTAL | SpriteTransform::VERTICAL);
}

// mirrors a "corner" sprite to draw the whole shape
static void mirrored_sprite(Rect bounds, Point pos)
{
    screen.sprite(bounds, pos);
    screen.sprite(bounds, pos + Point(bounds.w * 8, 0), SpriteTransform::HORIZONTAL);
    screen.sprite(bounds, pos + Point(0, bounds.h * 8), SpriteTransform::VERTICAL);
    screen.sprite(bounds, pos + Point(bounds.w * 8, bounds.h * 8), SpriteTransform::HORIZONTAL | SpriteTransform::VERTICAL);
}

// similar to above but repeats the edges in the middle to make it bigger
static void mirrored_sprite_extend(Rect bounds, Point pos, Point extend)
{
    int right_start = (bounds.w + extend.x) * 8;
    int bottom_start = (bounds.h + extend.y) * 8;

    screen.sprite(bounds, pos);
    screen.sprite(bounds, pos + Point(right_start, 0), SpriteTransform::HORIZONTAL);
    screen.sprite(bounds, pos + Point(0, bottom_start), SpriteTransform::VERTICAL);
    screen.sprite(bounds, pos + Point(right_start, bottom_start), SpriteTransform::HORIZONTAL | SpriteTransform::VERTICAL);

    // fill in the gaps
    int x_edge = bounds.x + bounds.w - 1;
    for(int x = 0; x < extend.x; x++)
    {
        screen.sprite(Rect(x_edge, bounds.y, 1, bounds.h), pos + Point((bounds.w + x) * 8, 0));
        screen.sprite(Rect(x_edge, bounds.y, 1, bounds.h), pos + Point((bounds.w + x) * 8, bottom_start), SpriteTransform::VERTICAL);
    }

    int y_edge = bounds.y + bounds.h - 1;
    for(int y = 0; y < extend.y; y++)
    {
        screen.sprite(Rect(bounds.x, y_edge, bounds.w, 1), pos + Point(0, (bounds.h + y) * 8));
        screen.sprite(Rect(bounds.x, y_edge, bounds.w, 1), pos + Point(right_start, (bounds.h + y) * 8), SpriteTransform::HORIZONTAL);
    }

    // middle
    for(int y = 0; y < extend.y; y++)
    {
        for(int x = 0; x < extend.x; x++)
            screen.sprite(Point(x_edge, y_edge), pos + Point((bounds.w + x) * 8, (bounds.h + y) * 8));
    }

}

void render(uint32_t time_ms)
{
    screen.alpha = 0xFF;
    screen.pen = Pen(255, 255, 255);
    screen.clear();

    screen.sprites = game_sprites;
    screen.pen = Pen(0, 0, 0);

    // words
    int x = 8, y = 8;
    int max_y = 100;
    
    for(auto &word : words)
    {
        if(word.found)
            screen.text(word.text, tall_font, Point(x, y));
        else
            screen.text(std::string(strlen(word.text), '_'), tall_font, Point(x, y));

        y += screen.measure_text(word.text, tall_font).h;

        if(y >= max_y)
        {
            y = 8;
            x += screen.measure_text("_______", tall_font).w + 8;
        }
    }

    // message
    screen.pen = message_bg;
    const Rect message_rect(8, 120, screen.bounds.w - 16, 24);
    screen.rectangle(message_rect);

    screen.pen = Pen(0, 0, 0);
    screen.text(message, tall_font, Rect(message_rect.x + 4, message_rect.y, message_rect.w - 8, message_rect.h), true, center_v);

    // letter tiles
    const int x_padding = 4;
    const int tiles_x = 8, tiles_y = 152;
    const Rect tile_sprite(0, 0, 2, 2), placeholder_sprite(4, 0, 1, 1);
    const int placeholders_y = 216;

    for(int i = 0; i < 7; i++)
    {
        screen.alpha = tiles[i].selected ? 102/*~40%*/ : 255;

        Point pos(i * (40 + x_padding) + tiles_x, tiles_y);
        mirrored_sprite_extend(tile_sprite, pos, Point(1, 1));

        char c = tiles[i].letter + ('A' - 'a'); // convert to uppercase
        screen.text(std::string_view(&c, 1), large_font, Rect(pos, Size(40, 40)), true, TextAlign::center_center);

        screen.alpha = 255;

        // selection indicator
        if(i == selected_tile && !game_ended)
            render_selection(Rect(pos, Size(40, 40)));

        // current guess
        pos.y = placeholders_y;

        for(int x = 0; x < 5; x++)
            screen.sprite(placeholder_sprite, pos + Point(x * 8, 0));

        if(static_cast<unsigned>(i) < current_word.size())
        {
            char c = current_word[i] + ('A' - 'a'); // convert to uppercase
            screen.text(std::string_view(&c, 1), large_font, Rect(pos, Size(40, 8)), true, TextAlign::bottom_center);
        }
    }

    // timer
    const Rect timer_sprite(10, 0, 3, 3);
    const int timer_x = screen.bounds.w - 8 - 48;

    mirrored_sprite(timer_sprite, Point(timer_x, 8));
    screen.text(std::to_string(timer / 1000), large_font, Rect(timer_x, 8, 48, 48), true, TextAlign::center_center);

    // score
    char buf[30];
    snprintf(buf, 30, "%i/%i", score, static_cast<int>(words.size()));
    screen.text(buf, large_font, Rect(timer_x, 8 + 44, 48, 48), true, TextAlign::center_right);

    if(level < 3)
    {
        snprintf(buf, 30, "%i to level %i", targets[level] - score, level + 1);
        screen.text(buf, tall_font, Rect(timer_x - 16, 8 + 44 + 22, 48 + 16, 48), true, TextAlign::center_right);
    }

    if(high_scores.games[current_game].score)
    {
        snprintf(buf, 30, "%i to beat", high_scores.games[current_game].score);
        screen.text(buf, tall_font, Rect(timer_x - 16, 8 + 44 + 22 + 14, 48 + 16, 48), true, TextAlign::center_right);
    }

    // game over
    if(game_ended)
    {
        screen.pen = Pen(0, 0, 0, 127);
        screen.rectangle(Rect(Point(0, 0), screen.bounds));

        int game_end_y = screen.bounds.h / 4, game_end_h = screen.bounds.h / 2;
        screen.pen = Pen(255, 255, 255);
        screen.rectangle(Rect(0, game_end_y, screen.bounds.w, game_end_h));

        screen.pen = Pen(0, 0, 0);
        screen.text(level == 0 ? "Did not find enough words" : "You win", large_font, Point(8, game_end_y + 8));

        char buf[50];
        int time = (game_length - timer) / 1000;
        snprintf(buf, 50, "Got %i / %i words in %i second%s", score, static_cast<int>(words.size()), time, time == 1 ? "" : "s");
        screen.text(buf, tall_font, Point(8, game_end_y + 32));

        // stars
        const Rect star_sprite(5, 0, 4, 4);

        for(int i = 0; i < 3; i++)
        {
            screen.alpha = level > i ? 255 : 127;
            screen.sprite(star_sprite, Point(8 + i * 40, game_end_y + 48));
        }

        screen.alpha = 255;

        // option buttons, reusing bits of the tile sprite
        int buttons_y = game_end_y + 88;

        int button_w = 64;

        auto button = [buttons_y, button_w, &tile_sprite](int x, std::string_view text, int index)
        {
            int button_h = 24;
            mirrored_sprite_extend(tile_sprite, Point(x, buttons_y), Point(4, -1)); // results in some overdraw...

            screen.text(text, tall_font, Rect(x, buttons_y, button_w, button_h), true, TextAlign::center_center);

            if(selected_menu_option == index && now() - end_time >= 1000)
                render_selection(Rect(x, buttons_y, button_w, button_h));
        };

        button(screen.bounds.w - (button_w + 8) * 2, "New word", 0);
        button(screen.bounds.w - (button_w + 8), "Play again", 1);
    }
}

static void update_score(int time_remaining)
{
    auto &cur_game_score = high_scores.games[current_game];
    if(score > cur_game_score.score || (score == cur_game_score.score && time_remaining < cur_game_score.time))
    {
        cur_game_score.score = score;
        cur_game_score.time = time_remaining;
    }
        
    write_save(high_scores);
}

void update(uint32_t time_ms)
{
    if(game_ended)
    {
        // delay to prevent acidentally restarting
        if(now() - end_time < 1000)
            return;

        if(buttons.released & Button::DPAD_LEFT)
            selected_menu_option = selected_menu_option == 0 ? 1 : selected_menu_option - 1;
        else if(buttons.released & Button::DPAD_RIGHT)
            selected_menu_option = selected_menu_option == 1 ? 0 : selected_menu_option + 1;

        if(buttons.released & Button::A)
        {
            if(selected_menu_option == 0)
            {
                // new word
                init_word();
                init_game();
            }
            else if(selected_menu_option == 1)
            {
                // play again
                init_game();
            }
        }

        return;
    }

    // update time
    timer = game_length - (now() - start_time);
    if(timer <= 0)
    {
        game_ended = true;
        end_time = now();

        update_score(timer / 1000);
    }

    if(buttons.released & Button::DPAD_LEFT)
        selected_tile = selected_tile == 0 ? 6 : selected_tile - 1;
    else if(buttons.released & Button::DPAD_RIGHT)
        selected_tile = selected_tile == 6 ? 0 : selected_tile + 1;

    // select/deselect tile
    if(buttons.released & Button::A)
    {
        auto &tile = tiles[selected_tile];
        if(!tile.selected)
        {
            tile.selected = true;
            current_word += tile.letter;
        }
        else if(current_word.back() == tile.letter)
        {
            tile.selected = false;
            current_word.pop_back();
        }
    }

    // remove last
    if(buttons.released & Button::B)
    {
        if(!current_word.empty())
        {
            char c = current_word.back();

            for(auto &tile : tiles)
            {
                if(tile.selected && tile.letter == c)
                {
                    tile.selected = false;
                    break;
                }
            }

            current_word.pop_back();
        }
    }

    // guess
    if(buttons.released & Button::Y)
    {
        bool found = false;
        for(auto &word : words)
        {
            if(!word.found && word.text == current_word)
            {
                found = true;
                word.found = true;
                score++;

                // level up
                if(score >= targets[level])
                {
                    level++;
                    if(level == 3) // final target
                    {
                        game_ended = true;
                        end_time = now();
                        update_score(timer / 1000);
                    }
                }
                break;
            }
        }

        if(found)
        {
            message = "Found \"" + current_word + "\"";
            message_bg = Pen(170, 255, 170);
        }
        else
        {
            message = "That is not a word we are looking for";
            message_bg = Pen(255, 170, 170);
        }

        current_word = "";
        for(auto &tile : tiles)
            tile.selected = false;
    }
}
