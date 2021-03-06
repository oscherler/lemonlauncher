/*
 * Copyright 2007 Josh Kropf
 *
 * This file is part of Lemon Launcher.
 *
 * Lemon Launcher is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Lemon Launcher is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Lemon Launcher; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "lemonmenu.h"
#include "game.h"
#include "options.h"
#include "error.h"

#include <cstring>
#include <sqlite3.h>
#include <sstream>
#include <algorithm>
#include <typeinfo>
#include <SDL/SDL_rotozoom.h>

#define UPDATE_SNAP_EVENT 1
#define JOYSTICK_REPEAT_EVENT 2

using namespace ll;
using namespace std;

/**
 * Function executed after a timeout for finding snapshot images
 */
static Uint32 snap_timer_callback(Uint32 interval, void *param);

/**
 * Function executed to repeat joystick input
 */
static Uint32 joystick_repeat_timer_callback(Uint32 interval, void *param);

/**
 * Asyncronous function for launching a game
 */
int launch_game(void* data);

struct sqlite_exception { };

template <typename A>
void assert_sqlite( A assertion )
{
   if (!assertion) throw sqlite_exception();
}

/**
 * Compares the text property of two item pointers and returns true if the left
 * is less than the right.
 */
bool cmp_item(item* left, item* right)
{ return strcmp(left->text(), right->text()) < 0; }

lemon_menu::lemon_menu(lemonui* ui) :
   _db(NULL), _top(NULL), _current(NULL), _show_hidden(false),
   _snap_timer(0), _snap_delay(g_opts.get_int(KEY_SNAPSHOT_DELAY)),
   _joystick_repeat_delay(250), _joystick_repeat_period(50)
{
   // locate games.db file in confdir
   string db_file("games.db");
   g_opts.resolve(db_file);
   
   if (sqlite3_open(db_file.c_str(), &_db))
      throw bad_lemon(sqlite3_errmsg(_db));
   
   _layout = ui;
   change_view(favorite);
   
   // axis, direction, delay, period, timer
   _joystick_repeat_config_x = (joystick_repeat_config) {
      0, 0,
      _joystick_repeat_delay, _joystick_repeat_period,
      0
   };
   _joystick_repeat_config_y = (joystick_repeat_config) {
      0, 0,
      _joystick_repeat_delay, _joystick_repeat_period,
      0
   };
}

lemon_menu::~lemon_menu()
{
   delete _top; // delete top menu will propigate to children
   
   if (_db)
      sqlite3_close(_db);
}

void lemon_menu::render()
{
   _layout->render(_current);  // pass off rendering to layout class
}

void lemon_menu::main_loop()
{
   log << info << "main_loop: starting render loop" << endl;

   render();
   reset_snap_timer();

   const int exit_key = g_opts.get_int(KEY_KEYCODE_EXIT);
   const int up_key = g_opts.get_int(KEY_KEYCODE_UP);
   const int down_key = g_opts.get_int(KEY_KEYCODE_DOWN);
   const int pgup_key = g_opts.get_int(KEY_KEYCODE_PGUP);
   const int pgdown_key = g_opts.get_int(KEY_KEYCODE_PGDOWN);
   const int select_key = g_opts.get_int(KEY_KEYCODE_SELECT);
   const int back_key = g_opts.get_int(KEY_KEYCODE_BACK);
   const int toggle_favorite_key = g_opts.get_int(KEY_KEYCODE_FAVORITE);
   const int alphamod = g_opts.get_int(KEY_KEYCODE_ALPHAMOD);
   const int viewmod = g_opts.get_int(KEY_KEYCODE_VIEWMOD);
   const int x_axis = g_opts.get_int(JOY_AXIS_LEFT_RIGHT);
   const int y_axis = g_opts.get_int(JOY_AXIS_UP_DOWN);
   const int joy_select_button = g_opts.get_int(JOY_BUTTON_SELECT);
   const int joy_back_button = g_opts.get_int(JOY_BUTTON_BACK);

   int x_axis_num = abs(x_axis) - 1;
   int x_axis_reverse = (x_axis > 0) - (x_axis < 0);
   int y_axis_num = abs(y_axis) - 1;
   int y_axis_reverse = (y_axis > 0) - (y_axis < 0);
   int prev_joy_x = 0;
   int prev_joy_y = 0;
   int hyst_out = 16383, hyst_in = 8191;

   _joystick_repeat_config_x.axis = x_axis_num;
   _joystick_repeat_config_y.axis = y_axis_num;

   _running = true;
   while (_running) {
      SDL_Event event;
      SDL_WaitEvent(&event);

      SDLKey key = event.key.keysym.sym;
      SDLMod mod = event.key.keysym.mod;

      switch (event.type) {
      case SDL_QUIT:
         _running = false;

         break;
      case SDL_KEYUP:
         if (key == exit_key) {
            _running = false;
         } else if (key == select_key) {
            handle_activate();
         } else if (key == back_key) {
            handle_up_menu();
         } else if (key == toggle_favorite_key) {
            handle_toggle_favorite();
         }

         break;
      case SDL_KEYDOWN:
         if (key == up_key) {
            handle_up();
         } else if (key == down_key) {
            handle_down();
         } else if (key == pgup_key) {
            if (mod & alphamod)
               handle_alphaup();
            else if (mod & viewmod)
               handle_viewdown();
            else
               handle_pgup();
         } else if (key == pgdown_key) {
            if (mod & alphamod)
               handle_alphadown();
            else if (mod & viewmod)
               handle_viewup();
            else
               handle_pgdown();
         }

         break;
      case SDL_JOYAXISMOTION:
         // correct value for Xin-Mo Dual Arcade -2..+1 glitch
         int corrected, reverse;
         if (event.jaxis.axis == x_axis_num)
            reverse = x_axis_reverse;
         else if (event.jaxis.axis == y_axis_num)
            reverse = y_axis_reverse;
         
         if( event.jaxis.value > 16383 )
            corrected = reverse * 32767;
         else if( event.jaxis.value < -16384 )
            corrected = -reverse * 32768;
         else
            corrected = 0;
         
         if (event.jaxis.axis == y_axis_num) {
            if (corrected > hyst_out && prev_joy_y != 1) {
               // joystick moved up
               prev_joy_y = 1;
               handle_up();

               _joystick_repeat_config_y.direction = 1;
               start_joystick_repeat_timer(&_joystick_repeat_config_y, false);
            } else if (corrected < -hyst_out && prev_joy_y != -1) {
               // joystick moved down
               prev_joy_y = -1;
               handle_down();

               _joystick_repeat_config_y.direction = -1;
               start_joystick_repeat_timer(&_joystick_repeat_config_y, false);
            } else if (corrected > -hyst_in && corrected < hyst_in) {
               // joystick moved back to the center (vertically)
               prev_joy_y = 0;
               stop_joystick_repeat_timer(&_joystick_repeat_config_y);
            }
         } else if (event.jaxis.axis == x_axis_num) {
            if (corrected > hyst_out && prev_joy_x != 1) {
               // joystick moved to the left
               prev_joy_x = 1;
               handle_viewup();

               _joystick_repeat_config_x.direction = 1;
               start_joystick_repeat_timer(&_joystick_repeat_config_x, false);
            } else if (corrected < -hyst_out && prev_joy_x != -1) {
               // joystick moved to the right
               prev_joy_x = -1;
               handle_viewdown();

               _joystick_repeat_config_x.direction = -1;
               start_joystick_repeat_timer(&_joystick_repeat_config_x, false);
            } else if (corrected > -hyst_in && corrected < hyst_in) {
               // joystick moved back to the center (horizontally)
               prev_joy_x = 0;
               stop_joystick_repeat_timer(&_joystick_repeat_config_x);
            }
         }
         break;
      case SDL_JOYBUTTONUP:
         log << info << event.jbutton.button + 1 << endl;
         if (event.jbutton.button + 1 == joy_select_button) {
            handle_activate();
         } else if (event.jbutton.button + 1 == joy_back_button) {
            handle_up_menu();
         }
         break;
      case SDL_USEREVENT:
         if (event.user.code == UPDATE_SNAP_EVENT) {
            update_snap();
         } else if (event.user.code == JOYSTICK_REPEAT_EVENT) {
            joystick_repeat_config *config = (joystick_repeat_config *) event.user.data1;
            // log << info << "repeat: " << config->axis << " " << config->direction << endl;
            if (config->axis == x_axis_num) {
               if (config->direction == 1)
                  handle_viewup();
               else if (config->direction == -1)
                  handle_viewdown();
               start_joystick_repeat_timer(&_joystick_repeat_config_x, true);
            } else if (config->axis == y_axis_num) {
               if (config->direction == 1)
                  handle_up();
               else if (config->direction == -1)
                  handle_down();
               start_joystick_repeat_timer(&_joystick_repeat_config_y, true);
            }
         }

         break;
      }
   }

   reset_snap_timer();
}

void lemon_menu::handle_up()
{
   // ignore event if already at the top of menu
   if (_current->select_previous()) {
      reset_snap_timer();
      render();
   }
}

void lemon_menu::handle_down()
{
   // ignore event if already at the bottom of menu
   if (_current->select_next()) {
      reset_snap_timer();
      render();
   }
}

void lemon_menu::handle_pgup()
{
   // ignore event if already at the top of menu
   if (_current->select_previous(_layout->page_size())) {
      reset_snap_timer();
      render();
   }
}

void lemon_menu::handle_pgdown()
{
   // ignore event if already at the bottom of menu
   if (_current->select_next(_layout->page_size())) {
      reset_snap_timer();
      render();
   }
}

void lemon_menu::handle_alphaup()
{
   // up in the alphabet is the previous letter
   if (_current->select_previous_alpha()) {
      reset_snap_timer();
      render();
   }
}

void lemon_menu::handle_alphadown()
{
   // down in the alphabet is the next letter
   if (_current->select_next_alpha()) {
      reset_snap_timer();
      render();
   }
}

void lemon_menu::handle_viewup()
{
   if (_view != all) {
      change_view((view_t)(_view+1));
      reset_snap_timer();
      render();
   }
}

void lemon_menu::handle_viewdown()
{
   if (_view != favorite) {
      change_view((view_t)(_view-1));
      reset_snap_timer();
      render();
   }
}

void lemon_menu::handle_activate()
{
   // ignore when this isn't any children
   if (!_current->has_children()) return;

   item* item = _current->selected();
   if (typeid(menu) == typeid(*item)) {
      handle_down_menu();
   } else if (typeid(game) == typeid(*item)) {
      handle_run();
   }
}

void lemon_menu::handle_toggle_favorite()
{
   item* item = _current->selected();
   if (typeid(game) != typeid(*item))
      return;
   
   game* g = (game*)item;
   g->toggle_favorite();

   ll::log << debug << "handle_toggle_favorite: " << g->text() << ": " << g->is_favorite() << endl;

   // create query to toggle game favorite status
   string query("UPDATE games SET favourite = ? WHERE filename = ?");
   ll::log << debug << query << endl;
   
   sqlite3_stmt *stmt;
   try {
      assert_sqlite(sqlite3_prepare_v2(_db, query.c_str(), -1, &stmt, NULL) == SQLITE_OK);
      assert_sqlite(sqlite3_bind_int(stmt, 1, g->is_favorite()) == SQLITE_OK);
      assert_sqlite(sqlite3_bind_text(stmt, 2, g->rom(), -1, SQLITE_TRANSIENT) == SQLITE_OK);
      assert_sqlite(sqlite3_step(stmt) == SQLITE_DONE);
   } catch (sqlite_exception ex) {
         const char *errmsg = sqlite3_errmsg(_db);
         sqlite3_finalize(stmt);
         throw bad_lemon(errmsg);
   }

   sqlite3_finalize(stmt);

   // force upate if we're in the favorites menu
   if(_view == favorite) {
      // get index of currently selected item
      int selected = _current->selected_index();
      
      change_view(_view);
      
      // restore selection
      _current->select_index(selected);

      reset_snap_timer();
   }
   
   render();
}

void lemon_menu::handle_run()
{
   game* g = (game*)_current->selected();
   log << info << "handle_run: launching game " << g->text() << endl;
   
   string cmd(g_opts.get_string(KEY_MAME_PATH));
   size_t pos = cmd.find("%r");
   if (pos == string::npos)
      throw bad_lemon("mame path missing %r specifier");

   cmd.replace(pos, 2, g->rom());
   ll::log << debug << "handle_run: " << cmd << endl;

   // This bit of code here has been a big pain.  On linux in full screen (X11)
   // lemon launcher has to be minimized before launching mame or else things
   // tend to lock up.  On windows lemon launcher is automagically minimized
   // and must be explicitly foregrounded after mame exits.  And on Mac OS X
   // sdlmame complains it can't open the screen.
   //
   // That said, I think I have it sorted.  Simply destroying lemon launchers
   // screen and then re-creating it after mame exits seems to get rid of the
   // irregularities.  Even on Windows!

   // destroy buffers and screen
   _layout->destroy_screen();
   
   // launch mame and hope for the best
   int exit_code = system(cmd.c_str());
   
   // create screen and render
   _layout->setup_screen();
   render();
   
   // increment the games play counter if emulator returned success
   // mark the game as broken otherwise
   string query;
   sqlite3_stmt *stmt;

   g->set_broken(exit_code != 0);
   if (g->is_broken()) {
      // mark game as broken
      query = string("UPDATE games SET broken = 1 WHERE filename = ?");
   } else {
      // update number of times game has been played
      query = string("UPDATE games SET count = count+1, broken = 0 WHERE filename = ?");
   }
   
   try {
      assert_sqlite(sqlite3_prepare_v2(_db, query.c_str(), -1, &stmt, NULL) == SQLITE_OK);
      assert_sqlite(sqlite3_bind_text(stmt, 1, g->rom(), -1, SQLITE_TRANSIENT) == SQLITE_OK);
      assert_sqlite(sqlite3_step(stmt) == SQLITE_DONE);
   } catch (sqlite_exception ex) {
      const char *errmsg = sqlite3_errmsg(_db);
      sqlite3_finalize(stmt);
      throw bad_lemon(errmsg);
   }

   sqlite3_finalize(stmt);
}

void lemon_menu::handle_up_menu()
{
   if (_current != _top) {
      _current = (menu*)_current->parent();
      reset_snap_timer();
      render();
   }
}

void lemon_menu::handle_down_menu()
{
   _current = (menu*)_current->selected();
   reset_snap_timer();
   render();
}

void lemon_menu::update_snap()
{
   if (_current->has_children()) {
      item* item = _current->selected();
      _layout->snap(item->snapshot());
      render();
   }
}

void lemon_menu::reset_snap_timer()
{
   if (_snap_timer)
      SDL_RemoveTimer(_snap_timer);

   // schedule timer to run in 500 milliseconds
   _snap_timer = SDL_AddTimer(_snap_delay, snap_timer_callback, NULL);
}

void lemon_menu::start_joystick_repeat_timer(joystick_repeat_config *config, bool repeating)
{
   // schedule timer to run
   config->timer = SDL_AddTimer(repeating ? config->period : config->delay, joystick_repeat_timer_callback, config);
}

void lemon_menu::stop_joystick_repeat_timer(joystick_repeat_config *config)
{
   if (config->timer)
      SDL_RemoveTimer(config->timer);
}

void lemon_menu::change_view(view_t view)
{
   _view = view;
   
   // recurisvely free top menu / children
   if (_top != NULL)
      delete _top;
   
   // create new top menu
   _current = _top = new menu(view_names[_view]);
   
   string query("SELECT filename, name, params, genre, favourite, broken FROM games");
   string where, order;
   
   switch (_view) {
   case favorite:
      order.append("name");
      where.append("favourite = 1");
      break;
      
   case most_played:
      order.append("count DESC,name");
      where.append("count > 0");
      break;
      
   case genre:
      order.append("genre,name");
      break;
      
   case all:
      order.append("name");
      break;
   }
   
   if (!_show_hidden) {
      if (where.length() != 0) where.append(" AND ");
      where.append("hide = 0 AND missing = 0");
   }
   
   // assemble query
   query.append(" WHERE ").append(where);
   query.append(" ORDER BY ").append(order);
   
   log << debug << "change_view: " << query.c_str() << endl;
   
   sqlite3_stmt *stmt;
   int rc;
   try {
      assert_sqlite(sqlite3_prepare_v2(_db, query.c_str(), -1, &stmt, NULL) == SQLITE_OK);
      while((rc = sqlite3_step(stmt)) == SQLITE_ROW)
      {
         insert_game(stmt);
      }
      assert_sqlite(rc == SQLITE_DONE);
   } catch (sqlite_exception ex) {
         const char *errmsg = sqlite3_errmsg(_db);
         sqlite3_finalize(stmt);
         throw bad_lemon(errmsg);
   }

   sqlite3_finalize(stmt);
}

void lemon_menu::insert_game(sqlite3_stmt *stmt)
{
   menu* top = this->top();
   
   game* g = new game(
      (char *)sqlite3_column_text(stmt, 0), // filename
      (char *)sqlite3_column_text(stmt, 1), // name
      (char *)sqlite3_column_text(stmt, 2), // params
      sqlite3_column_int(stmt, 4),          // favourite
      sqlite3_column_int(stmt, 5)           // broken
   );
   
   switch (this->view()) {
   case favorite:
   case most_played:
   case all:
      top->add_child(g);
      
      break;
      
   case genre:
      char* genre_name = (char *)sqlite3_column_text(stmt, 3);
      menu* m = NULL;
      if (!top->has_children()) {
         // if top menu doesn't have a menu yet, create one for the genre
         m = new menu(genre_name);
         top->add_child(m);
      } else {
         // get last child of the top level menu
         vector<item*>::iterator i = top->last();
         --i; // move iterator to the last item in list
         
         m = (menu*)*i;

         // create new child menu of the genre strings don't match
         if (strcmp(m->text(), genre_name) != 0) {
            m = new menu(genre_name);
            top->add_child(m);
         }
      }
      
      m->add_child(g);

      break;
   }
}

Uint32 snap_timer_callback(Uint32 interval, void *param)
{
   SDL_Event evt;
   evt.type = SDL_USEREVENT;
   evt.user.code = UPDATE_SNAP_EVENT;

   SDL_PushEvent(&evt);

   return 0;
}

Uint32 joystick_repeat_timer_callback(Uint32 interval, void *param)
{
   SDL_Event evt;
   evt.type = SDL_USEREVENT;
   evt.user.code = JOYSTICK_REPEAT_EVENT;
   evt.user.data1 = param;

   SDL_PushEvent(&evt);

   // continue firing the timer at the rate given in the config
   return 0;
}
