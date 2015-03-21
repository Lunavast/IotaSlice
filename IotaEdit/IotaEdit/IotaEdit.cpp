//
//  IotaEdit.cpp
//  IotaEdit
//
//  Created by Matthias Melcher on 23.03.13.
//  Copyright (c) 2013 Matthias Melcher. All rights reserved.
//

/**
 Minimum Commands:
    # annotation
    display text
    displayAt x, y, text
    print text
    xMoveAbs x
    xMove x
    xHome
    fire pattern
    fireJet n
    delay n
 Advanced:
    waitForButton n
    onButtonGoto label, label, label
    goto label
    :label
 */

/*
 
xHome 999999
yHome 999999
yGoto 28500
xGoto 100
fire 1023
fire 1023
fire 1023
fire 1023
fire 1023
fire 1023
fire 1023
fire 1023
fire 1023
fire 1023
fire 1
fire 2
fire 4
fire 8
fire 16
fire 32
fire 64
fire 128
fire 256
fire 512
fire 1024
fire 2048
*/

#include "IotaEdit.h"
#include <fltk3/fltk3.h>
#include "ItTextEditor.h"


Fl_Window *gMainWindow;
Fl_MenuBar *gMainMenu;
ItTextEditor *gTextEditor;
char *gCurrentFileName = 0;

typedef const char *string;
typedef string &stringRef;


int32_t readInt(FILE *f)
{
  uint8_t ub;
  int32_t ui = 0;
  for (;;) {
    if (feof(f)) break;
    fread(&ub, 1, 1, f);
    ui = ui<<7 | (ub&127);
    if ((ub&128)==0)
      break;
  }
  return ui;
}

void writeInt(FILE *f, int32_t x)
{
  uint8_t v;
  // bits 34..28
  if (x&(0xffffffff<<28)) {
    v = ((x>>28) & 0x7f) | 0x80;
    fputc(v, f);
  }
  // bits 27..21
  if (x&(0xffffffff<<21)) {
    v = ((x>>21) & 0x7f) | 0x80;
    fputc(v, f);
  }
  // bits 20..14
  if (x&(0xffffffff<<14)) {
    v = ((x>>14) & 0x7f) | 0x80;
    fputc(v, f);
  }
  // bits 13..7
  if (x&(0xffffffff<<7)) {
    v = ((x>>7) & 0x7f) | 0x80;
    fputc(v, f);
  }
  // bits 6..0
  v = x & 0x7f;
  fputc(v, f);
}

char *tmpBuf = 0;
int nTmpBuf = 0, NTmpBuf = 0;

void addText(const char *text)
{
  int n = (int)strlen(text);
  if (nTmpBuf+n+1>NTmpBuf) {
    NTmpBuf += 8096;
    tmpBuf = (char*)realloc(tmpBuf, NTmpBuf);
  }
  memcpy(tmpBuf+nTmpBuf, text, n);
  nTmpBuf += n;
  tmpBuf[nTmpBuf] = 0;
}

void addNewLine()
{
  addText("\n");
}

void addTextArg(FILE *f)
{
  uint8_t ub;
  char txt[8];
  addText("\"");
  for (;;) {
    if (feof(f)) break;
    fread(&ub, 1, 1, f);
    if (ub==0)
      break;
    if (ub<32) {
      txt[0] = '\\'; txt[1] = 'a'-1+ub; txt[1] = 0;
    } else if (ub=='\\') {
      txt[0] = '\\'; txt[1] = '\\'; txt[1] = 0;
    } else {
      txt[0] = (char)ub; txt[1] = 0;
    }
    addText(txt);
  }
  addText("\"");
}

void addTextEOL(FILE *f)
{
  uint8_t ub;
  char txt[8];
  for (;;) {
    if (feof(f)) break;
    fread(&ub, 1, 1, f);
    if (ub==0)
      break;
    if (ub=='\\') {
      txt[0] = '\\'; txt[1] = '\\'; txt[1] = 0;
    } else {
      txt[0] = (char)ub; txt[1] = 0;
    }
    addText(txt);
  }
  addText("\n");
}

void addIntArg(FILE *f)
{
  char txt[16];
  int32_t v = readInt(f);
  sprintf(txt, "%d", v);
  addText(txt);
}

// --- functions to read a text from a string and write into a 3dp file

void skipSpace(stringRef src)
{
  for (;;) {
    char c = *src;
    if (c!=' ' && c!='\t') break;
    if (c==0) break;
    src++;
  }
}

void skipComma(stringRef src)
{
  skipSpace(src);
  if (*src==',') {
    src++;
  } else {
    printf("Comma expected!\n");
  }
}

void skipEOL(stringRef src)
{
  skipSpace(src);
  if (*src=='\n') {
    src++;
  } else {
    printf("End of Line expected!\n");
  }
}

void writeIntArg(FILE *f, stringRef src)
{
  skipSpace(src);
  int32_t v = 0, sign = 1;
  if (*src=='-') {
    src++; sign = -1;
  } else if (*src=='+') {
    src++;
  }
  if (*src<0 || *src>'9') {
    printf("Integer expected!\n");
    return;
  }
  for (;;) {
    if (*src<'0' || *src>'9') break;
    if (*src==0) break;
    v = v*10 + ((*src)-'0');
    src++;
  }
  writeInt(f, sign*v);
}

void writeTextArg(FILE *f, stringRef src)
{
  skipSpace(src);
  if (*src!='"') {
    printf("String expected, opening quotes missing!\n");
    return;
  }
  src++;
  for (;;) {
    char c = *src++;
    if (c=='\n' || c==0) {
      printf("String expected, closing quotes missing!\n");
      return;
    } else if (c=='\\') {
      c = *src++;
      if (c=='\\') {
        fputc('\\', f);
        fputc('\\', f);
      } else if (c>='a' && c<='a'+30) {
        fputc(c-'a'-1, f);
      } else {
        fputc(c, f);
      }
    } else if (c=='"') {
      fputc(0, f);
      break;
    } else {
      fputc(c, f);
    }
  }
}

void writeTextEOL(FILE *f, stringRef src)
{
  for (;;) {
    char c = *src++;
    if (c=='\n' || c==0) {
      fputc(0, f); break;
    } else {
      fputc(c, f);
    }
  }
}



static void mainMenuFileOpenCB(Fl_Widget*, void*)
{
  char *fn = Fl_file_chooser("Open a 3dp file", "*.3dp", gCurrentFileName);
  if (fn==NULL) {
    return;
  }
  gCurrentFileName = strdup(fn);
  gTextEditor->buffer()->text("");
  // load code
  FILE *f = fopen(gCurrentFileName, "rb");
  if (f==NULL) {
    Fl_message("Can't open file");
    return;
  }
  //  17 03 8f 5d
  int32_t v0 = readInt(f);
  int32_t v1 = readInt(f);
  int32_t v2 = readInt(f);
  int32_t version = readInt(f);
  if (v0!=23 || v1!=3 || v2!=2013) {
    Fl_message("Unknown file format");
    fclose(f);
    return;
  }
  if (version>1) {
    fprintf(stderr, "File version %d is greater than supported version 1\n", version);
  }
  nTmpBuf = 0;
  int err = 0;
  for (;;) {
    if (feof(f)) break;
    int32_t v = readInt(f);
    switch(v) {
      case   0: break; // ignore NOP
      case 128: addNewLine(); break; // empty line
      case 129: addText("#"); addTextEOL(f); break;
      case 133: addText("display "); addTextArg(f); addNewLine(); break;
      case 134: addText("displayAt "); addIntArg(f), addText(", "); addIntArg(f), addText(", "); addTextArg(f); addNewLine(); break;
      case 135: addText("print "); addTextArg(f); addNewLine(); break;
      case 144: addText("xGoto "); addIntArg(f); addNewLine(); break;
      case 145: addText("xMove "); addIntArg(f); addNewLine(); break;
      case 146: addText("xHome "); addNewLine(); break;
      case 147: addText("yGoto "); addIntArg(f); addNewLine(); break;
      case 148: addText("yMove "); addIntArg(f); addNewLine(); break;
      case 149: addText("yHome "); addNewLine(); break;
      case 150: addText("z1Goto "); addIntArg(f); addNewLine(); break;
      case 151: addText("z1Move "); addIntArg(f); addNewLine(); break;
      case 152: addText("z1Home "); addNewLine(); break;
      case 153: addText("z2Goto "); addIntArg(f); addNewLine(); break;
      case 154: addText("z2Move "); addIntArg(f); addNewLine(); break;
      case 155: addText("z2Home "); addNewLine(); break;
      case 156: addText("rStart "); addIntArg(f); addNewLine(); break;
      case 157: addText("rStop"); addNewLine(); break;
      case 158: addText("spread "); addIntArg(f); addNewLine(); break;
      case   1: addText("fire "); addIntArg(f); addNewLine(); break;
      case 138: addText("fireNozzle "); addIntArg(f); addNewLine(); break;
      case 160: addText("delay "); addIntArg(f); addNewLine(); break;
      case 161: addText("repeat "); addIntArg(f); addText(" "); break;
        // motorOn bitmask, motorOff bitmask
      case 192: addText("setFireRepeat "); addIntArg(f); addNewLine(); break;
      case 193: addText("setFireAdvance "); addIntArg(f); addNewLine(); break;
      default: addText("UNKNOWN"); addNewLine(); err = 1; break;
    }
  }
  if (err) {
    Fl_message("Error in file contents.\nProceed with caution.\n(Version missmatch?)");
  }
  fclose(f);
  gTextEditor->buffer()->text(tmpBuf);
}


void saveFile(const char *fn)
{
  // save code
  FILE *f = fopen(gCurrentFileName, "wb");
  if (f==NULL) {
    Fl_message("Can't open file");
    return;
  }
  writeInt(f, 23);  // Magic
  writeInt(f, 3);
  writeInt(f, 2013);
  writeInt(f, 1);   // File Version
  
  const char *src = gTextEditor->buffer()->text();
  const char *text = src;
  for (;;) {
    // skip whitespace
    for (;;) {
      char c = *src;
      if (c!=' ' && c!='\t')
        break;
      if (c==0) break;
      src++;
    }
    if (*src==0) break;
    // interprete the command
    if (strncmp(src, "\n", 1)==0) {
      writeInt(f, 128); src++;
    } else if (strncmp(src, "#", 1)==0) {
      writeInt(f, 129); src++; writeTextEOL(f, src);
    } else if (strncmp(src, "display ", 8)==0) {
      writeInt(f, 133); src+=8; writeTextArg(f, src); skipEOL(src);
    } else if (strncmp(src, "displayAt ", 10)==0) {
      writeInt(f, 134); src+=10; writeIntArg(f, src); skipComma(src); writeIntArg(f, src); skipComma(src); writeTextArg(f, src); skipEOL(src);
    } else if (strncmp(src, "print ", 6)==0) {
      writeInt(f, 135); src+=6; writeTextArg(f, src); skipEOL(src);
    } else if (strncmp(src, "xGoto ", 6)==0) {
      writeInt(f, 144); src+=6; writeIntArg(f, src); skipEOL(src);
    } else if (strncmp(src, "xMove ", 6)==0) {
      writeInt(f, 145); src+=6; writeIntArg(f, src); skipEOL(src);
    } else if (strncmp(src, "xHome ", 6)==0) {
      writeInt(f, 146); src+=6; writeIntArg(f, src); skipEOL(src);
    } else if (strncmp(src, "yGoto ", 6)==0) {
      writeInt(f, 147); src+=6; writeIntArg(f, src); skipEOL(src);
    } else if (strncmp(src, "yMove ", 6)==0) {
      writeInt(f, 148); src+=6; writeIntArg(f, src); skipEOL(src);
    } else if (strncmp(src, "yHome ", 6)==0) {
      writeInt(f, 149); src+=6; writeIntArg(f, src); skipEOL(src);
    } else if (strncmp(src, "z1Goto ", 7)==0) {
      writeInt(f, 150); src+=7; writeIntArg(f, src); skipEOL(src);
    } else if (strncmp(src, "z1Move ", 7)==0) {
      writeInt(f, 151); src+=7; writeIntArg(f, src); skipEOL(src);
    } else if (strncmp(src, "z1Home ", 7)==0) {
      writeInt(f, 152); src+=7; writeIntArg(f, src); skipEOL(src);
    } else if (strncmp(src, "z2Goto ", 7)==0) {
      writeInt(f, 153); src+=7; writeIntArg(f, src); skipEOL(src);
    } else if (strncmp(src, "z2Move ", 7)==0) {
      writeInt(f, 154); src+=7; writeIntArg(f, src); skipEOL(src);
    } else if (strncmp(src, "z2Home ", 7)==0) {
      writeInt(f, 155); src+=7; writeIntArg(f, src); skipEOL(src);
    } else if (strncmp(src, "rStart ", 7)==0) {
      writeInt(f, 156); src+=7; writeIntArg(f, src); skipEOL(src);
    } else if (strncmp(src, "rStop ", 6)==0) {
      writeInt(f, 157); src+=6; skipEOL(src);
    } else if (strncmp(src, "spread ", 7)==0) {
      writeInt(f, 158); src+=7; writeIntArg(f, src); skipEOL(src);
    } else if (strncmp(src, "fire ", 5)==0) {
      writeInt(f, 1); src+=5; writeIntArg(f, src); skipEOL(src);
    } else if (strncmp(src, "fireNozzle ", 11)==0) {
      writeInt(f, 138); src+=11; writeIntArg(f, src); skipEOL(src);
    } else if (strncmp(src, "delay ", 6)==0) {
      writeInt(f, 160); src+=6; writeIntArg(f, src); skipEOL(src);
    } else if (strncmp(src, "repeat ", 7)==0) {
      writeInt(f, 161); src+=7; writeIntArg(f, src); skipSpace(src);
    } else if (strncmp(src, "setFireRepeat ", 14)==0) {
      writeInt(f, 192); src+=14; writeIntArg(f, src); skipEOL(src);
    } else if (strncmp(src, "setFireAdvance ", 15)==0) {
      writeInt(f, 193); src+=15; writeIntArg(f, src); skipEOL(src);
    } else {
      fprintf(stderr, "Syntax error: %s\n", src);
      int ix = (int)(src - text);
      int first = gTextEditor->buffer()->word_start(ix);
      int last = gTextEditor->buffer()->word_end(ix);
      gTextEditor->buffer()->select(first, last);
      break;
    }
  }
  fclose(f);
  free((void*)text);
}


static void mainMenuFileSaveAsCB(Fl_Widget*, void*)
{
  char *fn = Fl_file_chooser("Save a 3dp file", "*.3dp", gCurrentFileName);
  if (fn==NULL) {
    return;
  }
  gCurrentFileName = strdup(fn);
  saveFile(gCurrentFileName);
}


static void mainMenuFileSaveCB(Fl_Widget*, void*)
{
  if (gCurrentFileName)
    saveFile(gCurrentFileName);
}


static void mainMenuFileQuitCB(Fl_Widget*, void*)
{
  // TODO: offer to save file
  gMainWindow->hide();
}


static Fl_MenuItem gMainMenuItems[] = {
  { "File", 0, 0, 0, Fl_SUBMENU },
  {   "Open...",    Fl_META+'o', mainMenuFileOpenCB, 0 },
  {   "Open SD Card run.3dp", Fl_META+'O', 0, 0 },
  {   "Save",       Fl_META+'s', mainMenuFileSaveCB, 0 },
  {   "Save As...", Fl_META+'S', mainMenuFileSaveAsCB, 0 },
  {   "Quit",       Fl_META+'q', mainMenuFileQuitCB, 0 },
  {   0 },
  { 0 }
};


int main(int argc, char **argv)
{
  gMainWindow = new Fl_Window(600, 800, "IotaEdit V0.1");
  gMainMenu = new Fl_MenuBar(0, 0, gMainWindow->w(), 25);
  gMainMenu->menu(gMainMenuItems);
  gTextEditor = new ItTextEditor(0, gMainMenu->b(), gMainWindow->w(), gMainWindow->h()-gMainMenu->b());
  gMainWindow->show(argc, argv);
  Fl_run();
}
