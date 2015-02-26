//
// "$Id: $"
//
// Minimal Content Management System with FLTK
//
// Copyright 2002-2011 by Matthias Melcher.
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA.
//
// Please report all bugs and problems to "flCMS@matthiasm.com".
//

#ifndef IT_TEXT_EDITOR_H
#define IT_TEXT_EDITOR_H


#include <fltk3/TextEditor.h>

class ItTextEditor : public fltk3::TextEditor
{
public:
  ItTextEditor(int x, int y, int w, int h, const char *l=0);
  void add_modify_callback();
  void remove_modify_callback();
protected:
  static fltk3::TextDisplay::StyleTableEntry styletable[];
  static int compare_keywords(const void *a, const void *b);
  static void style_parse(const char *text, char *style, int length);
  static void style_unfinished_cb(int, void*);
  static void style_update(int pos, int nInserted, int nDeleted,
                           int /*nRestyled*/, const char * /*deletedText*/,
                           void *cbArg);
};


#endif

//
// End of "$Id: $".
//
