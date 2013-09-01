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

#include <SDL/SDL_image.h>
#include "state.h"
#include "menu.h"
#include "options.h"
#include "error.h"
#include "log.h"

using namespace ll;
using namespace std;

SDL_Surface* state::draw(TTF_Font* font, SDL_Color color, SDL_Color hover_color) const
{
   return TTF_RenderText_Blended(font, text(), (this == ((menu*)parent())->selected()) ? hover_color : color);
}

// this method is getting ridiculous. TODO: pass it a 2D array of colors
SDL_Surface* state::draw(TTF_Font* font, SDL_Color color, SDL_Color hover_color, SDL_Color emphasis_color, SDL_Color emphasis_hover_color, SDL_Color broken_color, SDL_Color broken_hover_color) const
{
   return draw(font, color, hover_color);
}
