
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


#include "ItTextEditor.h"
#include <string.h>
#include <stdlib.h>


Fl_TextDisplay::StyleTableEntry ItTextEditor::
styletable[] = {	// Style table
  { Fl_FOREGROUND_COLOR, Fl_HELVETICA,        12 }, // A - Plain
  { Fl_FOREGROUND_COLOR, Fl_COURIER,          12 }, // B - Preformatted
  { Fl_BLUE,             Fl_HELVETICA,        12 }, // C - HTML Links
  { Fl_BLUE,             Fl_HELVETICA,        12 }, // C - HTML Links
  { Fl_BLUE,             Fl_HELVETICA,        12 }, // C - HTML Links
  { Fl_BLUE,             Fl_HELVETICA,        12 }, // C - HTML Links
  { Fl_BLUE,             Fl_HELVETICA,        12 }, // C - HTML Links
  { Fl_BLUE,             Fl_HELVETICA,        12 }, // C - HTML Links
};

#if 0
case   2: addText("# "); addTextArg(f); addNewLine(); break;
case   3: addText("display "); addTextArg(f); addNewLine(); break;
case   4: addText("displayAt "); addIntArg(f), addText(", "); addIntArg(f), addText(", "); addTextArg(f); addNewLine(); break;
case   5: addText("print "); addTextArg(f); addNewLine(); break;
case   6: addText("xMoveAbs "); addIntArg(f); addNewLine(); break;
case   7: addText("xMove "); addIntArg(f); addNewLine(); break;
case   8: addText("xHome "); addNewLine(); break;
case   9: addText("yMoveAbs "); addIntArg(f); addNewLine(); break;
case  10: addText("yMove "); addIntArg(f); addNewLine(); break;
case  11: addText("yHome "); addNewLine(); break;
case  12: addText("z1MoveAbs "); addIntArg(f); addNewLine(); break;
case  13: addText("z1Move "); addIntArg(f); addNewLine(); break;
case  14: addText("z1Home "); addNewLine(); break;
case  15: addText("z2MoveAbs "); addIntArg(f); addNewLine(); break;
case  16: addText("z2Move "); addIntArg(f); addNewLine(); break;
case  17: addText("z2Home "); addNewLine(); break;
case  18: addText("rStart "); addIntArg(f); addNewLine(); break;
case  19: addText("rStop"); addNewLine(); break;
case  20: addText("fire "); addIntArg(f); addNewLine(); break;
case  21: addText("fireNozzle "); addIntArg(f); addNewLine(); break;
case  22: addText("fireRepeat "); addIntArg(f); addNewLine(); break;
case  23: addText("delay "); addIntArg(f); addNewLine(); break;
default: addText("UNKNOWN"); addNewLine(); err = 1; break;
#endif

ItTextEditor::ItTextEditor(int x, int y, int w, int h, const char *l)
: Fl_TextEditor(x, y, w, h, l)
{
  buffer(new Fl_TextBuffer);
  
  char *style = new char[mBuffer->length() + 1];
  char *text = mBuffer->text();
  
  memset(style, 'A', mBuffer->length());
  style[mBuffer->length()] = '\0';
  
  highlight_data(new Fl_TextBuffer(mBuffer->length()), styletable,
                 sizeof(styletable) / sizeof(styletable[0]),
                 'A', style_unfinished_cb, this);
  
  style_parse(text, style, mBuffer->length());
  
  mStyleBuffer->text(style);
  delete[] style;
  free(text);
  
  mBuffer->add_modify_callback(style_update, this);
}


void ItTextEditor::add_modify_callback()
{
  buffer()->add_modify_callback(Fl_TextEditor::buffer_modified_cb, this);
}

void ItTextEditor::remove_modify_callback()
{
  buffer()->remove_modify_callback(Fl_TextEditor::buffer_modified_cb, this);
}

// 'compare_keywords()' - Compare two keywords...
int ItTextEditor::compare_keywords(const void *a, const void *b) {
  return (strcmp(*((const char **)a), *((const char **)b)));
}

#define FILL_STYLE(STYLE, N, NEXT_STYLE) \
memset(style, STYLE, N); style += N; text += N-1; length -= N-1; col += N;\
current=NEXT_STYLE; goto next;

// 'style_parse()' - Parse text and produce style data.
void ItTextEditor::style_parse(const char *text, char *style, int length) {
  char		current;
  int		col;
  int		last;
  
  for (current = *style, col = 0, last = 0; length > 0; length --, text ++) {
    if (col==0) {
      if (strncmp(text, "<pre>", 5)==0) { // preformatted text
        FILL_STYLE('B', 5, 'B');
      } else if (strncmp(text, "</pre>", 6)==0) { // preformatted text
        FILL_STYLE('B', 6, 'A');
      } else if (strncmp(text, " - ", 3)==0) { // indented text
        FILL_STYLE('E', 3, 'A');
      } else if (strncmp(text, " * ", 3)==0) { // bullet list
        FILL_STYLE('E', 3, 'A');
      } else if (strncmp(text, " ** ", 4)==0) { // bullet list
        FILL_STYLE('E', 4, 'A');
      } else if (strncmp(text, " *** ", 5)==0) { // bullet list
        FILL_STYLE('E', 5, 'A');
      }
    }
    if (strncmp(text, "[[", 2)==0) { // start link
      FILL_STYLE('E', 2, 'C');
    } else if (strncmp(text, "]]", 2)==0) { // end link
      FILL_STYLE('E', 2, 'A');
    } else if (strncmp(text, "<tt>", 4)==0) { // start preformatted
      FILL_STYLE('E', 4, 'B');
    } else if (strncmp(text, "</tt>", 5)==0) { // end preformatted
      FILL_STYLE('E', 5, 'A');
    } else if (strncmp(text, "<", 1)==0 && text[-1]!='\\') { // start html
      FILL_STYLE('H', 1, 'H');
    } else if (strncmp(text, ">", 1)==0 && text[-1]!='\\' && current=='H') { // end html
      FILL_STYLE('H', 1, 'A');
    } else if (strncmp(text, "||", 2)==0) { // table
      FILL_STYLE('E', 2, 'A');
    } else if (strncmp(text, "\\&", 2)==0) { // control character &
      FILL_STYLE('E', 2, 'A');
    } else if (strncmp(text, "//", 2)==0) { // italics
      if (current=='D') {
        FILL_STYLE('E', 2, 'A');
      } else if (text[-1]!=':') {
        FILL_STYLE('E', 2, 'D');
      }
    } else if (strncmp(text, "**", 2)==0) { // bold
      if (current=='F') {
        FILL_STYLE('E', 2, 'A');
      } else {
        FILL_STYLE('E', 2, 'F');
      }
    } else if (strncmp(text, "##", 2)==0) { // target name
      if (current=='G') {
        FILL_STYLE('E', 2, 'A');
      } else {
        FILL_STYLE('E', 2, 'G');
      }
    }
    *style++ = current;
    col++;
  next:
    if (*text == '\n') {
      col = 0;
    }
  }
}

// 'style_unfinished_cb()' - Update unfinished styles.
void ItTextEditor::style_unfinished_cb(int, void*) { }

// 'style_update()' - Update the style buffer...
void ItTextEditor::style_update(int pos, int nInserted, int nDeleted,
                                     int /*nRestyled*/, const char * /*deletedText*/,
                                     void *cbArg) {
  ItTextEditor	*editor = (ItTextEditor *)cbArg;
  int		start,				// Start of text
  end;				// End of text
  char		last,				// Last style on line
  *style,				// Style data
  *text;				// Text data
  
  
  // If this is just a selection change, just unselect the style buffer...
  if (nInserted == 0 && nDeleted == 0) {
    editor->mStyleBuffer->unselect();
    return;
  }
  
  // Track changes in the text buffer...
  if (nInserted > 0) {
    // Insert characters into the style buffer...
    style = new char[nInserted + 1];
    memset(style, 'A', nInserted);
    style[nInserted] = '\0';
    
    editor->mStyleBuffer->replace(pos, pos + nDeleted, style);
    delete[] style;
  } else {
    // Just delete characters in the style buffer...
    editor->mStyleBuffer->remove(pos, pos + nDeleted);
  }
  
  // Select the area that was just updated to avoid unnecessary
  // callbacks...
  editor->mStyleBuffer->select(pos, pos + nInserted - nDeleted);
  
  // Re-parse the changed region; we do this by parsing from the
  // beginning of the line of the changed region to the end of
  // the line of the changed region...  Then we check the last
  // style character and keep updating if we have a multi-line
  // comment character...
  start = editor->mBuffer->line_start(pos);
  end   = editor->mBuffer->line_end(pos + nInserted);
  text  = editor->mBuffer->text_range(start, end);
  style = editor->mStyleBuffer->text_range(start, end);
  if (start==end)
    last = 0;
  else
    last  = style[end - start - 1];
  
  style_parse(text, style, end - start);
  
  editor->mStyleBuffer->replace(start, end, style);
  editor->redisplay_range(start, end);
  
  if (start==end || last != style[end - start - 1]) {
    // The last character on the line changed styles, so reparse the
    // remainder of the buffer...
    free(text);
    free(style);
    
    end   = editor->mBuffer->length();
    text  = editor->mBuffer->text_range(start, end);
    style = editor->mStyleBuffer->text_range(start, end);
    
    style_parse(text, style, end - start);
    
    editor->mStyleBuffer->replace(start, end, style);
    editor->redisplay_range(start, end);
  }
  
  free(text);
  free(style);
}


//
// End of "$Id: $".
//
