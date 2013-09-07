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
#ifndef STATE_H_
#define STATE_H_

#include "item.h"
#include <string>

using namespace std;

namespace ll {

/**
 * State item class
 */
class state : public item {
private:
   int _id;       // state id
   string _name;  // state name
/*
 * maybe add:
 * colour: to set on games with that state
 * make menu: true for favourites, false for broken
 */

public:
   state(const int id, const char* name) :
      _id(id), _name(name) { }

   virtual ~state() { }
   
   /** Returns the state id */
   const int id() const
   { return _id; }

   /** Returns the state name */
   const char* text() const
   { return _name.c_str(); }

   SDL_Surface* draw(TTF_Font* font, SDL_Color color, SDL_Color hover_color) const;
   SDL_Surface* draw(TTF_Font* font, SDL_Color color, SDL_Color hover_color, SDL_Color emphasis_color, SDL_Color emphasis_hover_color, SDL_Color broken_color, SDL_Color broken_hover_color) const;
   SDL_Surface* snapshot() { return NULL; }
};

} // end namespace

#endif
