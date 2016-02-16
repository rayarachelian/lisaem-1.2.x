/**************************************************************************************\
*                                                                                      *
*              The Lisa Emulator Project  V1.2.6      DEV 2007.12.04                   *
*                             http://lisaem.sunder.net                                 *
*                                                                                      *
*                  Copyright (C) 1998, 2007 Ray A. Arachelian                          *
*                                All Rights Reserved                                   *
*                                                                                      *
*           This program is free software; you can redistribute it and/or              *
*           modify it under the terms of the GNU General Public License                *
*           as published by the Free Software Foundation; either version 2             *
*           of the License, or (at your option) any later version.                     *
*                                                                                      *
*           This program is distributed in the hope that it will be useful,            *
*           but WITHOUT ANY WARRANTY; without even the implied warranty of             *
*           MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
*           GNU General Public License for more details.                               *
*                                                                                      *
*           You should have received a copy of the GNU General Public License          *
*           along with this program;  if not, write to the Free Software               *
*           Foundation, Inc., 59 Temple Place #330, Boston, MA 02111-1307, USA.        *
*                                                                                      *
*                   or visit: http://www.gnu.org/licenses/gpl.html                     *
*                                                                                      *
\**************************************************************************************/


// RAW_BITMAP_ACCESS should be used in most cases as it proves much higher performance
#ifdef NO_RAW_BITMAP_ACCESS
 #ifdef USE_RAW_BITMAP_ACCESS
   #undef	USE_RAW_BITMAP_ACCESS
 #endif
#endif

#ifdef VERSION
    static char *my_version=VERSION;
#endif
#ifdef BUILTBY
 static char *my_built_by=BUILTBY;
#endif

long emulation_tick=40;
long emulation_time=25;


// Some weird new changes to wxWidgets require newly created bitmaps to be filled,
// otherwise the wx/rawbmp.h code won't work right (since the alpha mask in the bitmap
// is set to the background, so we get no updates when we do the blit.  Yuck!)
//
// Worse yet, on some systems this needs to be WHITE creating annoying white flashes
// or white screens when LisaEm is off.  {Insert print "Motherfucker!" X 10000000}

#define FILLERBRUSH  *wxBLACK_BRUSH
#define FILLERPEN    *wxBLACK_PEN


// How many changes on the display before refreshing the entire display instead of the updated area?
// want to keep this value small, otherwise a clear desktop or such will be very slow.  The idea here
// is to avoid full repaints on small changes, such as the mouse moving.

#define DIRECT_BLITS_THRESHHOLD 64

// How much more blue to add to simulate the blueish phosphor of the Lisa CRT
#define EXTRABLUE 15  
// minimum skinned window size, this is smaller on purpose so that it will work with 12"
// notebook displays
#define IWINSIZEX 1020
#define IWINSIZEY 720

#define IWINSIZE IWINSIZEX,IWINSIZEY

// the size of the skin
#define ISKINSIZEX 1485
#define ISKINSIZEY 1031
#define ISKINSIZE  ISKINSIZEX,ISKINSIZEY

#define FLOPPY_LEFT 1099
#define FLOPPY_TOP 481

#define POWER_LEFT 0
#define POWER_TOP 738

// padding for skinless mode
#define WINXDIFF 30
#define WINYDIFF 65

// binary AND filter for how often to update
// the skinless background filter.  has to be 3,7,15,31,63
// larger the value the longer the delay between updates.
#define SKINLESS_UPDATE_FILTER 3



#include <wx/wx.h>
#include <wx/defs.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/dcbuffer.h>
#include <wx/wxhtml.h>
#include <wx/fs_zip.h>
#include <wx/wfstream.h>
#include <wx/log.h>
#include <wx/filedlg.h>
#include <wx/fileconf.h>
#include <wx/statusbr.h>
#include <wx/scrolwin.h>
#include <wx/sound.h>
#include <wx/config.h>
#include <wx/clipbrd.h>
#include <wx/datetime.h>
#include <wx/stopwatch.h>
#include <wx/display.h>
#include <wx/gdicmn.h>
#include <wx/pen.h>
#include <wx/scrolwin.h>
#include <wx/notebook.h>
#include <wx/aboutdlg.h>
#include <wx/stdpaths.h>
#include <wx/choicdlg.h>

#ifdef USE_RAW_BITMAP_ACCESS
#include <wx/rawbmp.h>
#endif

#include "machine.h"
#include "keyscan.h"

#include "LisaConfig.h"
#include "LisaConfigFrame.h"

// sounds, images, etc.
#include "lisaem_static_resources.h"

#ifdef __WXOSX__
#include <CoreFoundation/CoreFoundation.h>
#include <sys/param.h>
// was 32
#define DEPTH 32
#else
#define DEPTH 24
#endif


#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

extern "C"
{
 #include "vars.h"
 int32 reg68k_external_execute(int32 clocks);
 void unvars(void);

}

#if wxUSE_UNICODE
 #define CSTRCONV (wchar_t*)
#else
 #define CSTRCONV (char*)
#endif


// fwd references.
extern "C" int ImageWriter_LisaEm_Init(int iwnum);
extern "C" void iw_formfeed(int iw);
extern "C" void ImageWriterLoop(int iw,uint8 c);
extern "C" void iw_shutdown(void);

extern "C" void emulate (void);
extern "C" void XLLisaRedrawPixels(void);          // proto to supress warning below
extern "C" void LisaRedrawPixels(void);

extern "C" void resume_run(void);
extern "C" void pause_run(void);

extern "C" void force_refresh(void);

extern "C" void iw_enddocuments(void);

void iw_check_finish_job(void);


void turn_skins_on(void);
void turn_skins_off(void);

void powerbutton(void);
void setvideomode(int mode);
void save_global_prefs(void);

int asciikeyboard=1;

#ifndef MIN
  #define MIN(x,y) ( (x)<(y) ? (x):(y) )
#endif

#ifndef MAX
  #define MAX(x,y) ( (x)>(y) ? (x):(y) )
#endif


/*********************************************************************************************************************************/


 extern "C" int cpu68k_init(void);
 extern "C" void cpu68k_reset(void);
 extern "C" uint8 evenparity(uint8 data);



int effective_lisa_vid_size_y=500; //364;
int effective_lisa_vid_size_x=720;

wxPaintEvent nada;

void black(void);


// these need to be inside some sort of skin.rc file.
int screen_origin_x=140;
int screen_origin_y=130;



wxConfigBase      *myConfig;             // default configuration (just a pointer to the active config)
wxString           myconfigfile;         // config filename
wxFileStream      *pConfigIS;            // config file


LisaConfigFrame  *my_LisaConfigFrame=NULL;

void updateThrottleMenus(float throttle);
/*************************************************************************************************************************************/


// Declare the application class
class LisaEmApp : public wxApp
{
public:
    // Called on application startup
    virtual bool OnInit();
    #ifndef __WXOSX__
    void OnQuit(wxCommandEvent& event);
    #endif
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#define FLOPPY_NEEDS_REDRAW    0x80
#define FLOPPY_ANIMATING       0x40
#define FLOPPY_INSERT_0        0x00
#define FLOPPY_INSERT_1        0x01
#define FLOPPY_INSERT_2        0x02
#define FLOPPY_PRESENT         0x03
#define FLOPPY_EMPTY           0x04
#define FLOPPY_ANIM_MASK       0x07

#define POWER_NEEDS_REDRAW     0x80
#define POWER_PUSHED           0x40
#define POWER_ON_MASK          0x01
#define POWER_ON               0x01
#define POWER_OFF              0x00

#define REPAINT_INVALID_WINDOW 0x80
#define REPAINT_POWER_TO_SKIN  0x04
#define REPAINT_FLOPPY_TO_SKIN 0x02
#define REPAINT_VIDEO_TO_SKIN  0x01
#define REPAINT_NOTHING        0x00


class LisaWin : public wxScrolledWindow
{
public:
     LisaWin(wxWindow *parent);
     ~LisaWin();

     int dirtyscreen;      // indicated dirty lisa vidram
     int doubley; 
     uint8 brightness;

     int refresh_bytemap;  // flag indicating contrast change
     int floppystate;      // animation state of floppy
     int powerstate;       // animation state of power button


     void RePaint_AAGray(void);
     void RePaint_AntiAliased(void);

     void RePaint_DoubleY(void);
     void RePaint_SingleY(void);
     //void RePaint_Scaled(void);
     void RePaint_3A(void);
     void RePaint_2X3Y(void);

     void (LisaWin::*RePainter)(void);   // pointer method to one of the above


     void SetVideoMode(int mode);

     void OnPaint_skinless(wxRect &rect);
     void OnPaint_skins(wxRect &rect);
     void OnPaint(wxPaintEvent &event);

     void OnErase(wxEraseEvent &event);

	 long mousemoved;

     void OnMouseMove(wxMouseEvent &event);
     void OnKeyDown(wxKeyEvent& event);
     void OnKeyUp(wxKeyEvent& event);
     void OnChar(wxKeyEvent& event);

     void LogKeyEvent(const wxChar *name, wxKeyEvent& event,int keydir);
     void ContrastChange(void);

     int lastkeystroke;


     int repaintall;
     int ox,oy,ex,ey;
     int dwx,dwy;

     int rawcodes[128];
     int rawidx;

     uint8 bright[16];       // brightness levels for ContrastChange and repaint routines.
     
private:
     int lastkeyevent;
     wxScrolledWindow *myparent;
     wxCursor *m_dot_cursor;
     int lastcontrast;
     static inline wxChar GetChar(bool on, wxChar c) { return on ? c : _T('-'); }

     DECLARE_EVENT_TABLE()
};



BEGIN_EVENT_TABLE(LisaWin, wxScrolledWindow)

    EVT_KEY_DOWN(LisaWin::OnKeyDown)
    EVT_KEY_UP(LisaWin::OnKeyUp)
    EVT_CHAR(LisaWin::OnChar)

    EVT_ERASE_BACKGROUND(LisaWin::OnErase)
    EVT_PAINT(LisaWin::OnPaint)
    EVT_MOTION(LisaWin::OnMouseMove)
    EVT_LEFT_DOWN(LisaWin::OnMouseMove)
    EVT_LEFT_UP(LisaWin::OnMouseMove)
    EVT_RIGHT_DOWN(LisaWin::OnMouseMove)
    EVT_RIGHT_UP(LisaWin::OnMouseMove)

END_EVENT_TABLE()


// Event table for LisaEmFrame
enum
{
    ID_SCREENSHOT=10001,           // anti-aliased screenshot
    ID_SCREENSHOT2,                // screenshot with screen
    ID_SCREENSHOT3,                // raw screenshot - no aliasing

    ID_FUSH_PRNT,

#ifndef __WXMSW__
#ifdef TRACE
    ID_DEBUG,
#endif
#endif

    ID_DEBUGGER,
    ID_POWERKEY,
    ID_APPLEPOWERKEY,

    ID_KEY_APL_DOT,
    ID_KEY_APL_S,
    ID_KEY_APL_ENTER,
    ID_KEY_APL_RENTER,
    ID_KEY_APL_1,
    ID_KEY_APL_2,
    ID_KEY_APL_3,
    ID_KEY_NMI,

	ID_KEY_RESET,

    ID_PROFILEPWR,

    ID_PROFILE_ALL_ON,
    ID_PROFILE_ALL_OFF,

    ID_PROFILE_S1L,
    ID_PROFILE_S1U,
    ID_PROFILE_S2L,
    ID_PROFILE_S2U,
    ID_PROFILE_S3L,
    ID_PROFILE_S3U,

    ID_PROFILE_NEW,

    ID_FLOPPY,
    ID_NewFLOPPY,

    ID_RUN,
    ID_PAUSE,

    ID_KEY_OPT_0,
    ID_KEY_OPT_4,
    ID_KEY_OPT_7,
    ID_KEYBOARD,
    ID_ASCIIKB,
    ID_RAWKB,
    ID_RAWKBBUF,

    ID_EMULATION_TIMER,

    ID_THROTTLE5,
    ID_THROTTLE8,
    ID_THROTTLE10,
    ID_THROTTLE12,
    ID_THROTTLE16,
    ID_THROTTLE32,
    ID_THROTTLEX,

    ID_ET100_75,
    ID_ET50_30,
    ID_ET40_25,
    ID_ET30_20,

    ID_LISAWEB,
    ID_LISAFAQ,

    ID_VID_AA,
    ID_VID_AAG,
    //ID_VID_SCALED,
    ID_VID_DY,
    ID_VID_SY,
    ID_VID_2X3Y,

  // reinstated as per request by Kallikak
    ID_REFRESH_60Hz,
    ID_REFRESH_20Hz,
    ID_REFRESH_12Hz,
    ID_REFRESH_8Hz,
    ID_REFRESH_4Hz,

    ID_HIDE_HOST_MOUSE,

    ID_VID_SKINS_ON,
    ID_VID_SKINS_OFF
};

// Declare our main frame class



// lisaframe::running states
enum
{
	emulation_off=0,
	emulation_running=1,
	emulation_paused=10
};

class LisaEmFrame : public wxFrame
{
public:

    int running;          // is the Lisa running?  0=off, 1=running, 10=paused/locked.

    // Constructor
    LisaEmFrame(const wxString& title);

    void LoadImages(void);
    void UnloadImages(void);

    // Event handlers
    #ifdef __WXOSX__
    void OnQuit(wxCommandEvent& event);
    #endif

    void OnAbout(wxCommandEvent& event);

    void OnLisaWeb(wxCommandEvent& event);
    void OnLisaFaq(wxCommandEvent& event);

    void OnConfig(wxCommandEvent& event);
    void OnOpen(wxCommandEvent& event);
    void OnSaveAs(wxCommandEvent& event);


    void OnRun(wxCommandEvent& event);
    void OnPause(wxCommandEvent& event);

    void OnScreenshot(wxCommandEvent& event);
    void OnDebugger(wxCommandEvent& event);

    void OnPOWERKEY(wxCommandEvent& event);
    void OnAPPLEPOWERKEY(wxCommandEvent& event);
    void OnTraceLog(wxCommandEvent& event);
    void OnKEY_APL_DOT(wxCommandEvent& event);
    void OnKEY_APL_S(wxCommandEvent& event);
    void OnKEY_APL_ENTER(wxCommandEvent& event);
    void OnKEY_APL_RENTER(wxCommandEvent& event);
    void OnKEY_APL_1(wxCommandEvent& event);
    void OnKEY_APL_2(wxCommandEvent& event);
    void OnKEY_APL_3(wxCommandEvent& event);
    void OnKEY_NMI(wxCommandEvent& event);
	void OnKEY_RESET(wxCommandEvent& event);


	void UpdateProfileMenu(void);
	void OnProFilePowerX(int bit);

	void OnProFilePower(wxCommandEvent& event);

    void OnProFilePwrOnAll(wxCommandEvent& event);
	void OnProFilePwrOffAll(wxCommandEvent& event);

    void OnProFileS1LPwr(wxCommandEvent& event);
    void OnProFileS1UPwr(wxCommandEvent& event);
    void OnProFileS2LPwr(wxCommandEvent& event);
    void OnProFileS2UPwr(wxCommandEvent& event);
    void OnProFileS3LPwr(wxCommandEvent& event);
    void OnProFileS3UPwr(wxCommandEvent& event);

	void OnNewProFile(wxCommandEvent& event);





    void OnFLOPPY(wxCommandEvent& event);
    void OnNewFLOPPY(wxCommandEvent& event);

    void OnxFLOPPY(void);
    void OnxNewFLOPPY(void);

    void OnKEY_OPT_0(wxCommandEvent& event);
    void OnKEY_OPT_4(wxCommandEvent& event);
    void OnKEY_OPT_7(wxCommandEvent& event);
    void OnKEYBOARD(wxCommandEvent& event);

    void OnASCIIKB(wxCommandEvent& event);
    void OnRAWKB(wxCommandEvent& event);
    void OnRAWKBBUF(wxCommandEvent& event);
    void reset_throttle_clock(void);

    void OnThrottle5(wxCommandEvent& event);
    void OnThrottle8(wxCommandEvent& event);
    void OnThrottle10(wxCommandEvent& event);
    void OnThrottle12(wxCommandEvent& event);
    void OnThrottle16(wxCommandEvent& event);
    void OnThrottle32(wxCommandEvent& event);
    void OnThrottleX(wxCommandEvent& event);

    void OnET100_75(wxCommandEvent& event);
    void OnET50_30(wxCommandEvent& event);
    void OnET40_25(wxCommandEvent& event);
    void OnET30_20(wxCommandEvent& event);

    void SetStatusBarText(wxString &msg);
    void Update_Status(long elapsed,long idleentry);
    void VidRefresh(long now);
	int EmulateLoop(long idleentry);

    //void OnIdleEvent(wxIdleEvent& event);
    void OnEmulationTimer(wxTimerEvent& event);

	void OnPasteToKeyboard(wxCommandEvent&event);


	void FloppyAnimation(void);
    // menu commands that switch video mode
    void OnVideoAntiAliased(wxCommandEvent& event);
    void OnVideoAAGray(wxCommandEvent& event);
    //void OnVideoScaled(wxCommandEvent& event);
    void OnVideoDoubleY(wxCommandEvent& event);
    void OnVideoSingleY(wxCommandEvent& event);
    void OnVideo2X3Y(wxCommandEvent& event);

    void OnSkinsOn(wxCommandEvent& event);
    void OnSkinsOff(wxCommandEvent& event);

    void OnRefresh60Hz(wxCommandEvent& event);
    void OnRefresh20Hz(wxCommandEvent& event);
    void OnRefresh12Hz(wxCommandEvent& event);
    void OnRefresh8Hz(wxCommandEvent& event);
    void OnRefresh4Hz(wxCommandEvent& event);

    void OnFlushPrint(wxCommandEvent& event);
    void OnHideHostMouse(wxCommandEvent& event);



    //class LisaWin *win;
    int screensizex,screensizey;
    int depth;

    wxStopWatch runtime;               // idle loop stopwatch
    wxStopWatch soundsw;
    int soundplaying;

    uint16 lastt2;
    long    last_runtime_sample;
    long    last_decisecond;
    XTIMER clx;
    XTIMER lastclk;
    XTIMER cpu68k_reference;
    XTIMER last_runtime_cpu68k_clx;
    int dwx,dwy;

    wxString       wspaste_to_keyboard;
    char          paste_to_keyboard[8192];
    uint32         idx_paste_to_kb;
    float          throttle;
    float          clockfactor;
    float          mhzactual;

    wxString floppy_to_insert;
    long lastcrtrefresh;
	long hostrefresh;
	long screen_paint_update;
    long onidle_calls;
    XTIMER cycles_wanted;

    wxTimer* m_emulation_timer;
    int barrier;
    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(LisaEmFrame, wxFrame)


    EVT_MENU(wxID_ABOUT,        LisaEmFrame::OnAbout)


    EVT_MENU(ID_LISAWEB,        LisaEmFrame::OnLisaWeb)
    EVT_MENU(ID_LISAFAQ,        LisaEmFrame::OnLisaFaq)


    EVT_MENU(wxID_PREFERENCES,  LisaEmFrame::OnConfig)
    EVT_MENU(wxID_OPEN,         LisaEmFrame::OnOpen)
    EVT_MENU(wxID_SAVEAS,       LisaEmFrame::OnSaveAs)

    EVT_MENU(ID_SCREENSHOT,     LisaEmFrame::OnScreenshot)
    EVT_MENU(ID_SCREENSHOT2,    LisaEmFrame::OnScreenshot)
    EVT_MENU(ID_SCREENSHOT3,    LisaEmFrame::OnScreenshot)

    EVT_MENU(ID_FUSH_PRNT,      LisaEmFrame::OnFlushPrint)

    EVT_MENU(ID_DEBUGGER,       LisaEmFrame::OnDebugger)
    EVT_MENU(ID_POWERKEY,       LisaEmFrame::OnPOWERKEY)
    EVT_MENU(ID_APPLEPOWERKEY,  LisaEmFrame::OnAPPLEPOWERKEY)


#ifndef __WXMSW__
#ifdef TRACE
    EVT_MENU(ID_DEBUG,          LisaEmFrame::OnTraceLog)
#endif
#endif


    EVT_MENU(ID_KEY_APL_DOT,    LisaEmFrame::OnKEY_APL_DOT)
    EVT_MENU(ID_KEY_APL_S,      LisaEmFrame::OnKEY_APL_S)
    EVT_MENU(ID_KEY_APL_ENTER,  LisaEmFrame::OnKEY_APL_ENTER)
    EVT_MENU(ID_KEY_APL_RENTER, LisaEmFrame::OnKEY_APL_RENTER)
    EVT_MENU(ID_KEY_APL_1,      LisaEmFrame::OnKEY_APL_1)
    EVT_MENU(ID_KEY_APL_2,      LisaEmFrame::OnKEY_APL_2)
    EVT_MENU(ID_KEY_APL_3,      LisaEmFrame::OnKEY_APL_3)
    EVT_MENU(ID_KEY_NMI,        LisaEmFrame::OnKEY_NMI)
    EVT_MENU(ID_KEY_RESET,      LisaEmFrame::OnKEY_RESET)

    EVT_MENU(ID_PROFILEPWR,     LisaEmFrame::OnProFilePower)

    EVT_MENU(ID_PROFILE_ALL_ON, LisaEmFrame::OnProFilePwrOnAll)
	EVT_MENU(ID_PROFILE_ALL_OFF,LisaEmFrame::OnProFilePwrOffAll)

    EVT_MENU(ID_PROFILE_S1L,    LisaEmFrame::OnProFileS1LPwr)
    EVT_MENU(ID_PROFILE_S1U,    LisaEmFrame::OnProFileS1UPwr)
    EVT_MENU(ID_PROFILE_S2L,    LisaEmFrame::OnProFileS2LPwr)
    EVT_MENU(ID_PROFILE_S2U,    LisaEmFrame::OnProFileS2UPwr)
    EVT_MENU(ID_PROFILE_S3L,    LisaEmFrame::OnProFileS3LPwr)
    EVT_MENU(ID_PROFILE_S3U,    LisaEmFrame::OnProFileS3UPwr)

    EVT_MENU(ID_PROFILE_NEW,    LisaEmFrame::OnNewProFile)


    EVT_MENU(ID_FLOPPY,         LisaEmFrame::OnFLOPPY)
    EVT_MENU(ID_NewFLOPPY,      LisaEmFrame::OnNewFLOPPY)

    EVT_MENU(ID_KEY_OPT_0,      LisaEmFrame::OnKEY_OPT_0)
    EVT_MENU(ID_KEY_OPT_4,      LisaEmFrame::OnKEY_OPT_4)
    EVT_MENU(ID_KEY_OPT_7,      LisaEmFrame::OnKEY_OPT_7)

    EVT_MENU(ID_KEYBOARD,       LisaEmFrame::OnKEYBOARD)
    EVT_MENU(ID_ASCIIKB,        LisaEmFrame::OnASCIIKB)
    EVT_MENU(ID_RAWKB,          LisaEmFrame::OnRAWKB)
    EVT_MENU(ID_RAWKBBUF,       LisaEmFrame::OnRAWKBBUF)

    EVT_MENU(ID_THROTTLE5,      LisaEmFrame::OnThrottle5)
    EVT_MENU(ID_THROTTLE8,      LisaEmFrame::OnThrottle8)
    EVT_MENU(ID_THROTTLE10,     LisaEmFrame::OnThrottle10)
    EVT_MENU(ID_THROTTLE12,     LisaEmFrame::OnThrottle12)
    EVT_MENU(ID_THROTTLE16,     LisaEmFrame::OnThrottle16)
    EVT_MENU(ID_THROTTLE32,     LisaEmFrame::OnThrottle32)
    EVT_MENU(ID_THROTTLEX,      LisaEmFrame::OnThrottleX)


    EVT_MENU(ID_ET100_75,       LisaEmFrame::OnET100_75)
    EVT_MENU(ID_ET50_30,        LisaEmFrame::OnET50_30)
    EVT_MENU(ID_ET40_25,        LisaEmFrame::OnET40_25)
    EVT_MENU(ID_ET30_20,        LisaEmFrame::OnET30_20)

    EVT_MENU(wxID_PASTE,        LisaEmFrame::OnPasteToKeyboard)


    EVT_MENU(ID_VID_AA,         LisaEmFrame::OnVideoAntiAliased)
    EVT_MENU(ID_VID_AAG,        LisaEmFrame::OnVideoAAGray)

    //EVT_MENU(ID_VID_SCALED,     LisaEmFrame::OnVideoScaled)



    EVT_MENU(ID_VID_DY,         LisaEmFrame::OnVideoDoubleY)
    EVT_MENU(ID_VID_SY,         LisaEmFrame::OnVideoSingleY)
    EVT_MENU(ID_VID_2X3Y,       LisaEmFrame::OnVideo2X3Y)

    EVT_MENU(ID_VID_SKINS_ON,   LisaEmFrame::OnSkinsOn)
    EVT_MENU(ID_VID_SKINS_OFF,  LisaEmFrame::OnSkinsOff)

  // reinstated as per request by Kallikak, added 4Hz to help with really slow machines
    EVT_MENU(ID_REFRESH_60Hz,   LisaEmFrame::OnRefresh60Hz)
    EVT_MENU(ID_REFRESH_20Hz,   LisaEmFrame::OnRefresh20Hz)
    EVT_MENU(ID_REFRESH_12Hz,   LisaEmFrame::OnRefresh12Hz)
    EVT_MENU(ID_REFRESH_8Hz,    LisaEmFrame::OnRefresh8Hz)
    EVT_MENU(ID_REFRESH_4Hz,    LisaEmFrame::OnRefresh4Hz)

    EVT_MENU(ID_HIDE_HOST_MOUSE,LisaEmFrame::OnHideHostMouse)

    EVT_MENU(ID_RUN,            LisaEmFrame::OnRun)
    EVT_MENU(ID_PAUSE,          LisaEmFrame::OnPause)


    //EVT_IDLE(LisaEmFrame::OnIdleEvent)
    EVT_TIMER(ID_EMULATION_TIMER, LisaEmFrame::OnEmulationTimer)

    #ifdef __WXOSX__
    EVT_MENU(wxID_EXIT,         LisaEmFrame::OnQuit)
    #else
    EVT_MENU(wxID_EXIT,         LisaEmApp::OnQuit)
    #endif




END_EVENT_TABLE()


// want to have these as globals so that they can be accessed by other fn's
// and passed around like a cheap 40oz bottle at a frat.

wxFileConfig     *pConfig =NULL;


wxMenu *fileMenu     = NULL;
wxMenu *editMenu     = NULL;
wxMenu *keyMenu      = NULL;
wxMenu *DisplayMenu  = NULL;
wxMenu *throttleMenu = NULL;
wxMenu *profileMenu  = NULL;

wxMenu *helpMenu     = NULL;


LisaConfig       *my_lisaconfig=NULL;

LisaWin          *my_lisawin=NULL;
LisaEmFrame      *my_lisaframe=NULL;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//char *getResourcesDir(void)
//{
//    static char ret[1024];
//
//    wxStandardPathsBase& stdp = wxStandardPaths::Get();
//    strncpy(ret,stdp.GetResourcesDir().c_str(),1024);
//
//    return ret;
//}
//
//
//
//char *getDocumentsDir(void)
//{
//    static char ret[1024];
//
//    wxStandardPathsBase& stdp = wxStandardPaths::Get();
//    strncpy(ret,stdp.GetDocumentsDir().c_str(),1024);
//
//    return ret;
//}
//
// does not exist in wx2.7.x //
/*char *getExecutablePath(void)
{
    static char ret[1024];

    wxStandardPathsBase& stdp = wxStandardPaths::Get();
    strncpy(ret,stdp.GetExecutablePath().c_str(),1024);

    return ret;
}
*/

#ifndef USE_RAW_BITMAP_ACCESS
wxImage *display_img = NULL;
#endif

wxMenuBar        *menuBar = NULL;

wxBitmap         *my_lisabitmap=NULL;
wxMemoryDC       *my_memDC=NULL;

wxSound          *my_lisa_sound=NULL;       // sounds generated by Lisa Speaker

wxSound          *my_floppy_eject=NULL;
wxSound          *my_floppy_insert=NULL;
wxSound          *my_floppy_motor1=NULL;
wxSound          *my_floppy_motor2=NULL;
wxSound          *my_lisa_power_switch01=NULL;
wxSound          *my_lisa_power_switch02=NULL;
wxSound          *my_poweroffclk=NULL;




// byte bitmap used to accelerate writes to videoram
wxBitmap  *my_bytemap=NULL;    wxMemoryDC *my_bytemapDC =NULL;
wxBitmap  *my_alias  =NULL;    wxMemoryDC *my_aliasDC   =NULL;

wxImage   *display_image =NULL;

wxBitmap  *my_skin   =NULL;    wxMemoryDC *my_skinDC    =NULL; // bug with wxX11 turns background black, this attempts
wxBitmap  *my_skin0  =NULL;    wxMemoryDC *my_skin0DC   =NULL; // to split the img in two hoping that it will help.
wxBitmap  *my_skin1  =NULL;    wxMemoryDC *my_skin1DC   =NULL;
wxBitmap  *my_skin2  =NULL;    wxMemoryDC *my_skin2DC   =NULL;
wxBitmap  *my_skin3  =NULL;    wxMemoryDC *my_skin3DC   =NULL;

wxBitmap  *my_floppy0=NULL;    wxMemoryDC *my_floppy0DC =NULL;
wxBitmap  *my_floppy1=NULL;    wxMemoryDC *my_floppy1DC =NULL;
wxBitmap  *my_floppy2=NULL;    wxMemoryDC *my_floppy2DC =NULL;
wxBitmap  *my_floppy3=NULL;    wxMemoryDC *my_floppy3DC =NULL;
wxBitmap  *my_floppy4=NULL;    wxMemoryDC *my_floppy4DC=NULL;

wxBitmap  *my_poweron  =NULL;  wxMemoryDC *my_poweronDC =NULL;
wxBitmap  *my_poweroff =NULL;  wxMemoryDC *my_poweroffDC=NULL;


// display lens
static wxCoord screen_y_map[502];
static wxCoord screen_to_mouse[364*3];    // 2X,3Y mode is the largest we can do
static int     yoffset[502];              // lookup table for pointer into video display (to prevent multiplication)

//static int     screen_focus[364*3];       // how close to the ideal pixel we are - is this still used?


void buildscreenymap(void)
{
 float f,yf, ratio=364.0/500.0;
 int i,yy;
 wxCoord y;
 

 for (y=0; y<364; y++) yoffset[y]=y*90;

 for (f=0.0,i=0; i<502; i++,f+=1.0)
     {
       yf=(f*ratio);
       yy=(int)(yf); y=(wxCoord)yy;

       //screen_focus[i]=((int)(yf*10)) % 10;

       y=(y<499) ? y:499;
       screen_to_mouse[i]=y;
          screen_y_map[y]=i;
     }

}


void buildscreenymap_raw(void)
{
 wxCoord y;
 for (y=0; y<364; y++)
     {
       yoffset[y]=y*90;
       //screen_focus[y]=0;
       screen_to_mouse[y]=y;
          screen_y_map[y]=y;
     }

}

void buildscreenymap_3A(void)
{
 wxCoord y;

 for (y=0; y<431; y++)
     {
       yoffset[y]=y*76;
       //screen_focus[y]=0;
       screen_to_mouse[y]=y;
          screen_y_map[y]=y;
     }

}


void buildscreenymap_2Y(void)
{
 wxCoord y;
 for (y=0; y<364; y++)   yoffset[y]=y*90;
 for (y=0; y<364*2; y++)
     {
       //screen_focus[y]=0;
       screen_to_mouse[y]=y>>1;
          screen_y_map[y>>1]=y;
     }
}


void buildscreenymap_3Y(void)
{
 wxCoord y;
 for (y=0; y<364; y++)   yoffset[y]=y*90;
 for (y=0; y<364*3; y++)
     {
       //screen_focus[y]=0;

       screen_to_mouse[y]=y/3;
          screen_y_map[y/3]=y-1;
     }

}


// hack to fix issues on linux - somehow this isn't quite what I'd want either.
// it's not too different from --without-rawbitmap due to the conversions, and slightly worse.
void correct_my_skin(void)
{
#ifdef __WXGTK__
 #ifdef USE_RAW_BITMAP_ACCESS

  wxImage *display_image;
  if (!my_skin) return;

  display_image= new wxImage(  my_skin->ConvertToImage());
  delete my_skin;
  my_skin=new wxBitmap(*display_image);
  delete display_image;
  if ( my_skinDC==NULL)  my_skinDC=new class wxMemoryDC;
  my_skinDC->SelectObjectAsSource(*my_skin);
 #endif
#endif
}


// hack to fix issues on linux
void correct_my_lisabitmap(void)
{
#ifdef __WXGTK__
 #ifdef USE_RAW_BITMAP_ACCESS
 
  wxImage *display_image;
  if (!my_lisabitmap) return;

  display_image= new wxImage(  my_lisabitmap->ConvertToImage());
  delete my_lisabitmap;
  my_lisabitmap=new wxBitmap(*display_image);
  delete display_image;
  if ( my_memDC==NULL)  my_memDC=new class wxMemoryDC;
  my_memDC->SelectObjectAsSource(*my_lisabitmap);
 #endif
#endif
}



///////////////
void LisaWin::SetVideoMode(int mode)
{
  //int i;
  black();

  if ( has_lisa_xl_screenmod)   mode= 0x3a;               // sanity fixes
  if (!has_lisa_xl_screenmod && mode==0x3a) mode=0;
  if (mode!=0x3a) lisa_ui_video_mode=mode;


  DisplayMenu->Check(ID_VID_AA    ,mode==0);  DisplayMenu->Enable(ID_VID_AA    ,mode!=0x3a);
  DisplayMenu->Check(ID_VID_AAG   ,mode==4);  DisplayMenu->Enable(ID_VID_AAG   ,mode!=0x3a);
  //DisplayMenu->Check(ID_VID_SCALED,mode==5);  DisplayMenu->Enable(ID_VID_SCALED,mode!=0x3a);
  DisplayMenu->Check(ID_VID_DY    ,mode==1);  DisplayMenu->Enable(ID_VID_DY    ,mode!=0x3a);
  DisplayMenu->Check(ID_VID_SY    ,mode==2);  DisplayMenu->Enable(ID_VID_SY    ,mode!=0x3a);
  DisplayMenu->Check(ID_VID_2X3Y  ,mode==3);  DisplayMenu->Enable(ID_VID_2X3Y  ,mode!=0x3a);


  switch (mode)
  {

   case 0:   buildscreenymap()    ;  screen_origin_x=140; screen_origin_y=130; effective_lisa_vid_size_x=720; effective_lisa_vid_size_y=500;
             RePainter=&LisaWin::RePaint_AntiAliased;                                                                    break;

   case 1:   buildscreenymap_2Y() ;  screen_origin_x=  0; screen_origin_y=  0; effective_lisa_vid_size_x=720; effective_lisa_vid_size_y=364*2;
             RePainter=&LisaWin::RePaint_DoubleY;                                                                        break;

   case 2:   buildscreenymap_raw();  screen_origin_x=140; screen_origin_y=198; effective_lisa_vid_size_x=720; effective_lisa_vid_size_y=364;
             RePainter=&LisaWin::RePaint_SingleY;                                                                        break;

   case 3:   buildscreenymap_3Y() ;  screen_origin_x=  0; screen_origin_y=  0; effective_lisa_vid_size_x=720*2; effective_lisa_vid_size_y=364*3;
             RePainter=&LisaWin::RePaint_2X3Y;                                                                           break;

   case 4:   buildscreenymap()    ;  screen_origin_x=140; screen_origin_y=130; effective_lisa_vid_size_x=720; effective_lisa_vid_size_y=500;
             RePainter=&LisaWin::RePaint_AAGray;                                                                         break;

 //case 5:   buildscreenymap()    ;  screen_origin_x=140; screen_origin_y=130; effective_lisa_vid_size_x=720; effective_lisa_vid_size_y=500;
 //          RePainter=&LisaWin::RePaint_Scaled;                                                                         break;


   case 0x3a: buildscreenymap_3A();

             lisa_vid_size_x=608;
             lisa_vid_size_y=431;
             effective_lisa_vid_size_x=608;
             effective_lisa_vid_size_y=431;

             screen_origin_x=140+56;
             screen_origin_y=130+34;

             lisa_vid_size_x=608;
             lisa_vid_size_y=431;

             lisa_vid_size_xbytes=76;
             has_lisa_xl_screenmod=1;

             RePainter=&LisaWin::RePaint_3A;
             break;
  }

   delete my_lisabitmap;
   delete my_memDC;

   my_memDC     =new class wxMemoryDC;
   my_lisabitmap=new class wxBitmap(effective_lisa_vid_size_x, effective_lisa_vid_size_y,DEPTH);
   my_memDC->SelectObjectAsSource(*my_lisabitmap);
   my_memDC->SetBrush(FILLERBRUSH);      my_memDC->SetPen(FILLERPEN);
   my_memDC->DrawRectangle(0 ,   0,   effective_lisa_vid_size_x, effective_lisa_vid_size_y);
   if (!my_memDC->IsOk()) ALERT_LOG(0,"my_memDC is not ok.");
   if (!my_lisabitmap->IsOk()) ALERT_LOG(0,"my_lisabitmap is not ok.");


   videoramdirty=32768;
//   ALERT_LOG(0,"Done setting video mode to :%02x",mode);

   if (skins_on)
	   {
         SetMinSize(wxSize(720,384));
         SetMaxSize(wxSize(ISKINSIZE));
         SetScrollbars(ISKINSIZEX/100, ISKINSIZEY/100,  100,100,  0,0,  true);
         EnableScrolling(true,true);
       }
   else
       {
         int x,y;
         GetSize(&x,&y);
         screen_origin_x=(x  - effective_lisa_vid_size_x)>>1;                 // center display
         screen_origin_y=(y  - effective_lisa_vid_size_y)>>1;                 // on skinless
         screen_origin_x= (screen_origin_x<0 ? 0:screen_origin_x);
         screen_origin_y= (screen_origin_y<0 ? 0:screen_origin_y);
         ox=screen_origin_x; oy=screen_origin_y;
	   }
   Refresh(false,NULL);
   black();
  
}

void LisaEmFrame::FloppyAnimation(void)
{

    if (my_lisawin->floppystate & FLOPPY_ANIMATING)                           // go to next frame
	{

		if ((my_lisawin->floppystate & FLOPPY_ANIM_MASK)!= FLOPPY_PRESENT) 
		{
			my_lisawin->floppystate++;
			my_lisawin->floppystate |= FLOPPY_ANIMATING|FLOPPY_NEEDS_REDRAW;
		} 
		
		if ((my_lisawin->floppystate & FLOPPY_ANIM_MASK)== FLOPPY_PRESENT) 
		{
			my_lisawin->floppystate= FLOPPY_NEEDS_REDRAW | FLOPPY_PRESENT; // refresh, stop counting.
			if (my_floppy_insert!=NULL && sound_effects_on) my_floppy_insert->Play(romless ? wxSOUND_SYNC:wxSOUND_ASYNC);
		}
	}
    else
	{
		if (my_floppy_eject!=NULL && sound_effects_on && (my_lisawin->floppystate & FLOPPY_ANIM_MASK)== FLOPPY_INSERT_2)
			my_floppy_eject->Play(wxSOUND_ASYNC);

		if ((my_lisawin->floppystate & FLOPPY_ANIM_MASK)==FLOPPY_INSERT_0) 
		{
			my_lisawin->floppystate=FLOPPY_NEEDS_REDRAW | FLOPPY_EMPTY;
		} 
		else 
		{
			my_lisawin->floppystate--;  // go to previous frame
			my_lisawin->floppystate |= FLOPPY_NEEDS_REDRAW;
		}

	}
    Refresh();
    wxMilliSleep(200);
}




void LisaEmFrame::VidRefresh(long now)
{


    if (videoramdirty)
    {
  		  wxRect* rect=NULL;
		  if (running) check_running_lisa_os();

          my_lisawin->dirtyscreen=2;
		  // suspect that RefreshRect(rect,false) erases the background anyway on wxMac 2.8.x
		  rect=new wxRect(screen_origin_x,           screen_origin_y,
					  effective_lisa_vid_size_x, effective_lisa_vid_size_y);
          Refresh(false, rect);
          delete rect;
 		  lastcrtrefresh=now;                                      // and how long ago the last refresh happened
                                                                   // cheating a bit here to smooth out mouse movement.
    }

	screen_paint_update++;                                   // used to figure out effective host refresh rate
	lastrefresh=cpu68k_clocks;
    seek_mouse_event();
}

int LisaEmFrame::EmulateLoop(long idleentry)
{

    long now=runtime.Time();

    while (now-idleentry<emulation_time && running)          // don't stay in OnIdleEvent for too long, else UI gets unresponsive
    {
		long cpuexecms=(long)((float)(cpu68k_clocks-cpu68k_reference)*clockfactor); //68K CPU Execution in MS

		if ( cpuexecms<=now  )                               // balance 68K CPU execution vs host time to honor throttle
		{
            if (!cycles_wanted) cycles_wanted=(XTIMER)(float)(emulation_time/clockfactor);
            clx=clx+cycles_wanted;                           // add in any leftover cycles we didn't execute.
                                                             // but prevent it falling behind or jumping ahead
                                                             // too far.
            clx=MIN(clx,2*cycles_wanted  );
            clx=MAX(clx,  cycles_wanted/2);

            clx=reg68k_external_execute(clx);                // execute some 68K code
            // clx=reg68k_external_execute(clx);

			now=runtime.Time();                              // find out how long we took

            if (pc24 & 1)                                    // lisa rebooted or just odd addr error?
			{                                                // moved here to avoid stack leak
                ALERT_LOG(0,"ODD PC!")
				if (lisa_ram_safe_getlong(context,12) & 1)   // oddaddr vector is odd as well?
				{
					save_pram();                             // lisa has rebooted.
					profile_unmount();
					return 1;
				}
			}

			get_next_timer_event();                          // handle any pending IRQ's/timer prep work
		    
//20071115                                                             // if we need to, refresh the display
//#ifdef __WXOSX__
//		VidRefresh(now);                                     // OS X, esp slower PPC's suffer if we use the if statement
//#else
		if (now-lastcrtrefresh>hostrefresh) VidRefresh(now); // but if we don't, Linux under X11 gets too slow.
        else seek_mouse_event();
        
//#endif
		}                                                    // loop if we didn't go over our time quota
		else
			break;                                           // else force exit, time quota is up
    } // exec loop  ////////////////////////////////////////////////////////////////////////////////////////////////////////////

	return 0;	                                             // Lisa did not reboot, return 0
}



void LisaEmFrame::Update_Status(long elapsed,long idleentry)
{
	wxString text;
	float hosttime=(float)(elapsed - last_runtime_sample);
	mhzactual= 0.001 *
		( (float)(cpu68k_clocks-last_runtime_cpu68k_clx))  / hosttime;

  //text.Printf("%1.2fMHz  %x%x:%x%x:%x%x.%x @%d/%08x vid:%1.2fHz%c  loop:%1.2fHz, mouse:%1.2fHz slice:%ldms, 68K:%lldcycles (%3.2f ms), %lld left, %lld wanted (%lld%%)",
    text.Printf(_T("%1.2fMHz  %x%x:%x%x:%x%x.%x @%d/%08x"),
				mhzactual,
				lisa_clock.hours_h,lisa_clock.hours_l,
				lisa_clock.mins_h,lisa_clock.mins_l,
				lisa_clock.secs_h,lisa_clock.secs_l,
				lisa_clock.tenths,
                context,pc24); //,

				//(float)screen_paint_update*1000/hosttime,                 (videoramdirty ? 'D':' '),
				//(float)onidle_calls       *1000/hosttime,
				//(float)my_lisawin->mousemoved *1000/hosttime,
				//elapsed-idleentry,
                //cpu68k_clocks-last_runtime_cpu68k_clx,
                //((clockfactor==0) ?(0.001 *     ((float)(cpu68k_clocks-last_runtime_cpu68k_clx))/mhzactual)
				//                  :(clockfactor*((float)(cpu68k_clocks-last_runtime_cpu68k_clx)))           ),
				//clx, cycles_wanted,
				//100*clx/cycles_wanted
			   //);

	SetStatusBarText(text);
	// these are independant of the execution loops on purpose, so that way we get a 2nd opinion as it were.
	last_runtime_sample=elapsed;
	last_runtime_cpu68k_clx=cpu68k_clocks;
    my_lisawin->mousemoved=0;
	screen_paint_update=0;                              // reset statistics counters
	onidle_calls=0;
	idleentry--;                                       // eat warning when debug version isn't used.
}

void LisaEmFrame::OnEmulationTimer(wxTimerEvent& WXUNUSED(event))
{

  long now=runtime.Time();
  long idleentry=now;

  // we run the timer as fast as possible.  there's a chance that it will call this method
  // while another instance is in progress.  the barrier prevents this.  Since each call will take
  // a slightly different amount of time, I can't predict a good value for this, but want to call it
  // as often as possible for the higher MHz throttles, so this is needed.

  if (barrier)   {ALERT_LOG(0,"Re-entry detected!"); return;}
  barrier=1;

  onidle_calls++;

  if ((my_lisawin->floppystate & FLOPPY_ANIM_MASK) != FLOPPY_PRESENT &&
	  (my_lisawin->floppystate & FLOPPY_ANIM_MASK) != FLOPPY_EMPTY )	  FloppyAnimation();

   if (running==emulation_running)
   {
	 long int elapsed=0;

  //   if (floppy_6504_wait>0 && floppy_6504_wait<128) floppy_6504_wait--;                 
  
     if (!last_runtime_sample )                              // initialize slice timer
     {
        last_runtime_sample=now;
        last_decisecond=now;
        lastcrtrefresh=now;
		last_runtime_cpu68k_clx=cpu68k_clocks;
     }


	 if (cpu68k_clocks<10 && floppy_to_insert.Len())         // deferred floppy insert.
	 {
           const wxCharBuffer s = floppy_to_insert.fn_str();
           int i=floppy_insert((char *)(const char *)s);
           floppy_to_insert=_T("");
           if (i) eject_floppy_animation();
	 }


    clktest = cpu68k_clocks;
 	clx=cpu68k_clocks;

	long ticks=( now - last_decisecond );                    // update COPS 1/10th second clock.
	while (ticks>100) {ticks-=100; decisecond_clk_tick(); last_decisecond=now;}


    if (EmulateLoop(now) )                                   // 68K execution
	{            
		ALERT_LOG(0,"REBOOTED?");                            // Did we reboot?
		lisa_rebooted();
        barrier=0;
		return;
	}

	elapsed=runtime.Time();                                  // get time after exist of execution loop


	if ( (elapsed - last_runtime_sample) > 1000  && running) // update status bar every 1000ms, and check print jobs too
	{
		   static int ctr;
           Update_Status(elapsed,idleentry);
		   if ((ctr++)>9) {iw_check_finish_job(); ctr=0;}
    }

    // anything to paste to the keyboard?
    if (wspaste_to_keyboard.Len()<8102 && wspaste_to_keyboard.Len()>idx_paste_to_kb && (copsqueuelen>=0 && copsqueuelen<MAXCOPSQUEUE-8) )
        {DEBUG_LOG(0,"pasting to kb"); keystroke_cops( paste_to_keyboard[idx_paste_to_kb++] );}

   }
   else // we are not running, or we are paused, so yield and sleep a bit
   {
	  
	  last_runtime_sample=0;
	  lastcrtrefresh=0;
   }



   #ifndef __WXOSX__
     {
	  static unsigned int x;
	  x++;
	  if (x>15)  {
		          x=0;
		          videoramdirty=32768; 
			      Refresh(false,NULL);
			     }
     }
   #endif


barrier=0;
}
///////////////////////




void LisaEmFrame::OnPasteToKeyboard(wxCommandEvent& WXUNUSED(event))
{
    wxTextDataObject data;
    if (wxTheClipboard->Open()) 
    {
        if (wxTheClipboard->IsSupported(wxDF_TEXT))  wxTheClipboard->GetData(data);
        wxTheClipboard->Close();

        wxTheClipboard->UsePrimarySelection();

        if (wxTheClipboard->IsSupported(wxDF_TEXT)) 
        {
           
            wspaste_to_keyboard = (wxString)(data.GetText());
            strncpy(paste_to_keyboard,wspaste_to_keyboard.fn_str(),8192 );
            idx_paste_to_kb = 0;
        }
    }
}

LisaWin::LisaWin(wxWindow *parent)
        : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                   wxRAISED_BORDER|wxVSCROLL|wxHSCROLL|wxWS_EX_PROCESS_IDLE
#ifdef wxALWAYS_SHOW_SB
|wxALWAYS_SHOW_SB
#endif
#ifdef wxMAXIMIZE
|wxMAXIMIZE
#endif
                                   )
{
       mousemoved=0;

	   floppystate = FLOPPY_NEEDS_REDRAW | FLOPPY_EMPTY; // set floppy state and force refresh
       powerstate = POWER_NEEDS_REDRAW;                  // set power state and force refresh
       //repaintall = REPAINT_NOTHING;

       repaintall = REPAINT_INVALID_WINDOW;

       rawidx=0;

       SetExtraStyle(wxWS_EX_PROCESS_IDLE );

       // deprecated // SetInitialBestSize(wxSize(IWINSIZE));

	   //wxScreenDC theScreen;          // I know I do this in the frame too, but when this runs,
	   int screensizex,screensizey;   // my_lisaframe is still null.
	   //theScreen.GetSize(&screensizex,&screensizey);

	   wxDisplaySize(&screensizex,&screensizey);

	   // Grrr! OS X sometimes returns garbage here
	   if (screensizex< 0 || screensizex>8192 || screensizey<0 || screensizey>2048)
	         {
		      ALERT_LOG(0,"Got Garbage lisawin Screen Size: %d,%d",screensizex,screensizey);
		      screensizex=1024; screensizey=768;
	         }
	   else
		     {
  		      if (my_lisaframe) {my_lisaframe->screensizex=screensizex;my_lisaframe->screensizey=screensizey;}
//			  ALERT_LOG(0,"LisaWin - Screen size is:%d,%d",screensizex,screensizey);
		     }


       if (skins_on)
       {
         SetMinSize(wxSize(720,384)); //IWINSIZE previously
         SetSize(wxSize(IWINSIZE));
         SetMaxSize(wxSize(ISKINSIZE));
         SetScrollbars(ISKINSIZEX/100, ISKINSIZEY/100,  100,100,  0,0,  true);
         EnableScrolling(true,true);

         screen_origin_x=140;
         screen_origin_y=130;
       }
       else  //------------------------ skinless ----------------------------------------------------
       {
         switch (lisa_ui_video_mode)
         {
               default:
                case 0:   effective_lisa_vid_size_x=720;   effective_lisa_vid_size_y=500;      break;
                case 1:   effective_lisa_vid_size_x=720;   effective_lisa_vid_size_y=364*2;    break;
                case 2:   effective_lisa_vid_size_x=720;   effective_lisa_vid_size_y=364;      break;
                case 3:   effective_lisa_vid_size_x=720*2; effective_lisa_vid_size_y=364*3;    break;
                case 4:   effective_lisa_vid_size_x=720;   effective_lisa_vid_size_y=500;      break;
                case 0x3a:effective_lisa_vid_size_x=608;   effective_lisa_vid_size_y=431;      break;
         }



         int x,y;

         y=myConfig->Read(_T("/lisawin/sizey"),(long)effective_lisa_vid_size_y);
         x=myConfig->Read(_T("/lisawin/sizex"),(long)effective_lisa_vid_size_x);
         if (x<=0 || x>4096 || y<=0 || y>2048) {x=effective_lisa_vid_size_x;y=effective_lisa_vid_size_y;}


         if (x>screensizex || y>screensizey)         // if the saved/defaults are too large correct them
            {
              x=MIN(effective_lisa_vid_size_x,screensizex-100);
              y=MIN(effective_lisa_vid_size_y,screensizey-150);
            }

//         ALERT_LOG(0,"Setting lisawin size to:%d,%d effective:%d,%d",x,y,
//                     effective_lisa_vid_size_x,effective_lisa_vid_size_y);

         SetMinSize(wxSize(720,384));
         SetClientSize(wxSize(x,y));                                                         // LisaWin //
         SetMaxSize(wxSize(ISKINSIZE));                                                      // LisaWin //
         GetClientSize(&dwx,&dwy);
         // Sometimes GetClientSize/SetClientSize return DIFFERNT values.  On OS X this causes the
         // Lisa window to GROW over time! m@+h3rf*(<R!!!  dwx/dwy is used to figure out the difference,
		 // and then adjust to the size we want.  This is some sick shit that anyone has to code this way.
         dwx-=x; dwy-=y;
         SetClientSize(wxSize(x-dwx,y-dwy));                                                 // LisaWin //
         SetMinSize(wxSize(720,384));
         ox=(x  - effective_lisa_vid_size_x)/2;                                              // center display
         oy=(y  - effective_lisa_vid_size_y)/2;                                              // on skinless
         ox= (ox<0 ? 0:ox);
         oy= (oy<0 ? 0:oy);

         EnableScrolling(false,false);                                                       // LisaWin //

       } // --------------------------------------------------------------------------------------------------


       RePainter=&LisaWin::RePaint_AntiAliased;  //RePaint_AAGray;

       /* Draw the VNC-style dot cursor we use later */
#if defined(__WXX11__) || defined(__WXGTK__) || defined(__WXOSX__)

#ifdef __WXOSX__
       /* LSB (0x01) is leftmost. First byte is top row. 1 is black/visible*/
       char cursor_bits[] = {0x00, 0x0e, 0x0e, 0x0e, 0x00};
       char mask_bits[]   = {0x1f, 0x1f, 0x1f, 0x1f, 0x1f};
#else
       char cursor_bits[] = {0xe0, 0xee, 0xee, 0xee, 0xe0};
       char mask_bits[]   = {0xe0, 0xe0, 0xe0, 0xe0, 0xe0};
#endif

       wxBitmap bmp = wxBitmap(cursor_bits, 8, 5);
       bmp.SetMask(new wxMask(wxBitmap(mask_bits, 8, 5)));

       wxImage img = bmp.ConvertToImage();
       img.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_X, _T("2"));
       img.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_Y, _T("2"));

       m_dot_cursor = new wxCursor(img);
#endif
}

LisaWin::~LisaWin(void)
{
#if defined(__WXX11__) || defined(__WXGTK__) || defined(__WXOSX__)
    delete m_dot_cursor;
#endif
}


void LisaEmFrame::OnVideoAntiAliased(wxCommandEvent& WXUNUSED(event))
{
    if (screensizex<IWINSIZEX || screensizey<IWINSIZEY)
    {
        wxString msg;
        msg.Printf(_T("Your display is only (%d,%d).  This mode needs at least (%d,%d), will shut off skins."),
                   screensizex,screensizey,  IWINSIZEX,IWINSIZEY);
        wxMessageBox(msg,_T("The display is too small"));
        turn_skins_off();

        if (screensizey<IWINSIZEY) {my_lisawin->SetVideoMode(2); return;}  // even still too small, go raw bits mode.
    }
    my_lisawin->SetVideoMode(0);
}


void LisaEmFrame::OnVideoAAGray(wxCommandEvent& WXUNUSED(event))
{

    if (screensizex<IWINSIZEX || screensizey<IWINSIZEY)
    {
        wxString msg;
		msg.Printf(_T("Your display is only (%d,%d).  This mode needs at least (%d,%d), will shut off skins."),
                   screensizex,screensizey,  IWINSIZEX,IWINSIZEY);
        wxMessageBox(msg,wxT("The display is too small"));
        turn_skins_off();
        if (screensizey<IWINSIZEY) {my_lisawin->SetVideoMode(2); return;}
    }

    my_lisawin->SetVideoMode(4);
}
//void LisaEmFrame::OnVideoScaled(wxCommandEvent& WXUNUSED(event))          {my_lisawin->SetVideoMode(5);}
void LisaEmFrame::OnVideoSingleY(wxCommandEvent& WXUNUSED(event))           {my_lisawin->SetVideoMode(2);}


void LisaEmFrame::OnVideoDoubleY(wxCommandEvent& WXUNUSED(event))
{

    if (screensizex<720+40 || screensizey<364*2+50)
    {
     wxString msg;
		msg.Printf(_T("Your display is only (%d,%d).  This mode needs at least (%d,%d)."),
                screensizex,screensizey,  720+40,364*2+50 );
        wxMessageBox(msg,wxT("The display is too small"));

     // oops!  The display size changed on us since the last time. i.e. notebook with small screen
     // previously connected to large monitor, now on native LCD
     if (lisa_ui_video_mode==1 || lisa_ui_video_mode==3)
        {
          my_lisawin->SetVideoMode(2); turn_skins_off();
        }

     return;
    } /// display too small ///////////////////////////////////////////////////////////////


    if (skins_on)
    {
     if (yesnomessagebox("This mode doesn't work with the Lisa Skin.  Shut off the skin?",
                         "Remove Skin?")==0) return;
    }
    my_lisawin->SetVideoMode(1);
    turn_skins_off();
}


void LisaEmFrame::OnVideo2X3Y(wxCommandEvent& WXUNUSED(event))
{
    if (screensizex<720*2+40 ||screensizey<364*3+100)
    {
     wxString msg;
     msg.Printf(_T("Your display is only (%d,%d).  This mode needs at least (%d,%d)."),
                screensizex,screensizey,  720*2+40,364*3+100 );
        wxMessageBox(msg,wxT("The display is too small"));

     // oops!  The display size changed on us since the last time. i.e. notebook with small screen
     // previously connected to large monitor, now on native LCD
     if (lisa_ui_video_mode==1 || lisa_ui_video_mode==3)
        {
          my_lisawin->SetVideoMode(2);
          turn_skins_off();
        }


     return;
    }

    if (skins_on)
    {
     if (yesnomessagebox("This mode doesn't work with the Lisa Skin.  Shut off the skin?",
                         "Remove Skin?")==0) return;
    }

    my_lisawin->SetVideoMode(3);
    turn_skins_off();
}



void LisaEmFrame::OnSkinsOn(wxCommandEvent& WXUNUSED(event))
{
 if (lisa_ui_video_mode==1 || lisa_ui_video_mode==3)
 {
   if (yesnomessagebox("The current display mode doesn't work with the Lisa Skin. Change modes?",
                       "Change Display Mode?")==0) return;

    setvideomode(0);
  }

  skins_on_next_run=1;
  skins_on=1;
  turn_skins_on();
}


void LisaEmFrame::OnSkinsOff(wxCommandEvent& WXUNUSED(event))
{
 skins_on_next_run=0;
 skins_on=0;
 turn_skins_off();
}

void LisaEmFrame::OnRefresh60Hz(wxCommandEvent& WXUNUSED(event)) {hostrefresh=1000/60; refresh_rate=1*REFRESHRATE; refresh_rate_used=refresh_rate;save_global_prefs();}
void LisaEmFrame::OnRefresh20Hz(wxCommandEvent& WXUNUSED(event)) {hostrefresh=1000/20; refresh_rate=3*REFRESHRATE; refresh_rate_used=refresh_rate;save_global_prefs();}
void LisaEmFrame::OnRefresh12Hz(wxCommandEvent& WXUNUSED(event)) {hostrefresh=1000/12; refresh_rate=5*REFRESHRATE; refresh_rate_used=refresh_rate;save_global_prefs();}
void LisaEmFrame::OnRefresh8Hz( wxCommandEvent& WXUNUSED(event)) {hostrefresh=1000/ 8; refresh_rate=7*REFRESHRATE; refresh_rate_used=refresh_rate;save_global_prefs();}
void LisaEmFrame::OnRefresh4Hz( wxCommandEvent& WXUNUSED(event)) {hostrefresh=1000/ 4; refresh_rate=9*REFRESHRATE; refresh_rate_used=refresh_rate;save_global_prefs();}

void LisaEmFrame::OnHideHostMouse(wxCommandEvent& WXUNUSED(event)) {hide_host_mouse=!hide_host_mouse; save_global_prefs();}



extern "C" long get_wx_millis(void)
{
 return my_lisaframe->runtime.Time();
}




void save_global_prefs(void)
{
  int x,y;

  if (!myConfig)     return;                                 // not initialized yet
  if (!DisplayMenu)  return;
  if (!my_lisaframe) return;
  if (!fileMenu)     return;

  myConfig->Write(_T("/soundeffects"),sound_effects_on);
  myConfig->Write(_T("/displayskins"),skins_on_next_run);
  myConfig->Write(_T("/displaymode"), (long)lisa_ui_video_mode);
  myConfig->Write(_T("/asciikeyboard"), (long)asciikeyboard);
  myConfig->Write(_T("/lisaconfigfile"),myconfigfile);
  myConfig->Write(_T("/throttle"),(long)my_lisaframe->throttle);

  myConfig->Write(_T("/emutime"),(long)emulation_time);
  myConfig->Write(_T("/emutick"),(long)emulation_tick);



  myConfig->Write(_T("/refreshrate"),(long)refresh_rate);
  myConfig->Write(_T("/hidehostmouse"),(long)hide_host_mouse);

  my_lisawin->GetClientSize(&x,&y);
  myConfig->Write(_T("/lisawin/sizey"),(long)y-my_lisawin->dwy);
  myConfig->Write(_T("/lisawin/sizex"),(long)x-my_lisawin->dwx);
  my_lisaframe->GetClientSize(&x,&y);
  myConfig->Write(_T("/lisaframe/sizey"),(long)y-my_lisaframe->dwy);
  myConfig->Write(_T("/lisaframe/sizex"),(long)x-my_lisaframe->dwx);


  myConfig->Flush();



  DisplayMenu->Check(ID_VID_AA    ,lisa_ui_video_mode==0);  DisplayMenu->Enable(ID_VID_AA    ,lisa_ui_video_mode!=0x3a);
  DisplayMenu->Check(ID_VID_AAG   ,lisa_ui_video_mode==4);  DisplayMenu->Enable(ID_VID_AAG   ,lisa_ui_video_mode!=0x3a);
//DisplayMenu->Check(ID_VID_SCALED,lisa_ui_video_mode==5);  DisplayMenu->Enable(ID_VID_SCALED,lisa_ui_video_mode!=0x3a);
  DisplayMenu->Check(ID_VID_DY    ,lisa_ui_video_mode==1);  DisplayMenu->Enable(ID_VID_DY    ,lisa_ui_video_mode!=0x3a);
  DisplayMenu->Check(ID_VID_SY    ,lisa_ui_video_mode==2);  DisplayMenu->Enable(ID_VID_SY    ,lisa_ui_video_mode!=0x3a);
  DisplayMenu->Check(ID_VID_2X3Y  ,lisa_ui_video_mode==3);  DisplayMenu->Enable(ID_VID_2X3Y  ,lisa_ui_video_mode!=0x3a);

  DisplayMenu->Check(ID_VID_SKINS_ON, !!skins_on);
  DisplayMenu->Check(ID_VID_SKINS_OFF, !skins_on);

  if ( refresh_rate!=9*REFRESHRATE  && refresh_rate!=7*REFRESHRATE  &&
       refresh_rate!=5*REFRESHRATE  && refresh_rate!=3*REFRESHRATE  &&
       refresh_rate!=  REFRESHRATE                                    ) refresh_rate =  REFRESHRATE;

  // reinstated as per request by Kallikak
  my_lisaframe->hostrefresh=1000/60;
  DisplayMenu->Check(ID_REFRESH_4Hz, refresh_rate==9*REFRESHRATE);  if (refresh_rate==9*REFRESHRATE) my_lisaframe->hostrefresh=1000/ 4;
  DisplayMenu->Check(ID_REFRESH_8Hz, refresh_rate==7*REFRESHRATE);  if (refresh_rate==7*REFRESHRATE) my_lisaframe->hostrefresh=1000/ 8;
  DisplayMenu->Check(ID_REFRESH_12Hz,refresh_rate==5*REFRESHRATE);  if (refresh_rate==7*REFRESHRATE) my_lisaframe->hostrefresh=1000/12;
  DisplayMenu->Check(ID_REFRESH_20Hz,refresh_rate==3*REFRESHRATE);  if (refresh_rate==7*REFRESHRATE) my_lisaframe->hostrefresh=1000/20;
  DisplayMenu->Check(ID_REFRESH_60Hz,refresh_rate==1*REFRESHRATE);  if (refresh_rate==7*REFRESHRATE) my_lisaframe->hostrefresh=1000/60;

#ifdef DEBUG
#ifdef TRACE
  fileMenu->Check(ID_DEBUG,!!debug_log_enabled);
#endif
#endif

  DisplayMenu->Check(ID_HIDE_HOST_MOUSE,!!hide_host_mouse);

  keyMenu->Check(ID_ASCIIKB,  asciikeyboard== 1);
  keyMenu->Check(ID_RAWKB,    asciikeyboard== 0);
  keyMenu->Check(ID_RAWKBBUF, asciikeyboard==-1);

  updateThrottleMenus(my_lisaframe->throttle);
}


void LisaEmFrame::SetStatusBarText(wxString &msg) {SetStatusText(msg,0);}

DECLARE_APP(LisaEmApp)           // Implements LisaEmApp& GetApp()
IMPLEMENT_APP(LisaEmApp)         // Give wxWidgets the means to create a LisaEmApp object


// Initialize the application
bool LisaEmApp::OnInit()
{
    wxString argv1;
    wxString Ext;

	wxString defaultconfig = wxGetHomeDir();
    #ifdef __UNIX__
    if (defaultconfig.Last() != wxT('/') )  defaultconfig  <<wxT("/lisaem.conf");
    #else
    if (defaultconfig.Last() != wxT('\\') )  defaultconfig  <<wxT("\\lisaem.conf");
    #endif

    myConfig = wxConfig::Get();         // this one is the global configuration for the app.
                                        // get the path to the last opened Lisaconfig and load that.

    myconfigfile=myConfig->Read(_T("/lisaconfigfile"),defaultconfig);

    // override config file on startup if passed a parameter.
    if (argc>1)
    {
      argv1.Printf(_T("%s"),argv[1]);
      const char *ext=strstr(
                  (const char *)(argv1.fn_str()),
                  (const char *)(".lisaem"));

      if (ext!=NULL && strlen(ext)==7)  // ensure the proper extension is contained.
         {
            argv1.Printf(_T("%s"),argv[1]);
            if (wxFile::Exists(argv1) ) myconfigfile=argv1;
         }
    }


    pConfig=new wxFileConfig(_T("LisaEm"),
                             _T("sunder.NET"),
                             (myconfigfile),     //local
                             (myconfigfile),     //global
                             wxCONFIG_USE_LOCAL_FILE,
                             wxConvAuto() );   // or wxConvUTF8

    // this is a global setting  - must be right before the LisaFrame!
    skins_on          =(int)myConfig->Read(_T("/displayskins"),(long)1);
    skins_on_next_run=skins_on;

    // Create the main application window
    my_lisaframe=new LisaEmFrame(wxT("LisaEm"));

    refresh_rate      =(long)myConfig->Read(_T("/refreshrate"),(long)REFRESHRATE);
    hide_host_mouse   =(int) myConfig->Read(_T("/hidehostmouse"),(long)0);
    refresh_rate_used=refresh_rate;
    sound_effects_on  =(int)myConfig->Read(_T("/soundeffects"),(long)1);
    lisa_ui_video_mode=(int)myConfig->Read(_T("/displaymode"), (long)0);
    asciikeyboard     =(int)myConfig->Read(_T("/asciikeyboard"),(long)1);
    my_lisaframe->throttle=(float)(myConfig->Read(_T("/throttle"),(long)5));

    emulation_time=(long)myConfig->Read(_T("/emutime"),(long)100);
    emulation_tick=(long)myConfig->Read(_T("/emutick"),(long)75);


//    ALERT_LOG(0,"Initial system start up Throttle:%f clkfactor:%f",
//              my_lisaframe->throttle,my_lisaframe->clockfactor);

    save_global_prefs();

    DEBUG_LOG(0,"CreatingLisaconfig()");
    my_lisaconfig = new LisaConfig();
    DEBUG_LOG(0,"Created Lisaconfig()");

    pConfig->SetRecordDefaults();

    DEBUG_LOG(0,"Loading Config");
    my_lisaconfig->Load(pConfig, floppy_ram);// load it in
    DEBUG_LOG(0,"Saving Config");
    my_lisaconfig->Save(pConfig, floppy_ram);// save it so defaults are created



    my_lisaframe->running=emulation_off;     // CPU isn't yet turned on


    SetVendorName(_T("sunder.NET"));
    SetAppName(_T("LisaEm"));
	my_lisawin->repaintall = REPAINT_INVALID_WINDOW;
    my_lisaframe->Show(true);                // Light it up
	my_lisawin->repaintall = REPAINT_INVALID_WINDOW;

    wxStandardPathsBase& stdp = wxStandardPaths::Get();
	wxString sndfile;
	wxString rscDir = stdp.GetResourcesDir() + wxFileName::GetPathSeparator(wxPATH_NATIVE);
   
    // stare not upon the insanity that predated this code, for it was written by drunkards! (*Burp*)
    sndfile=rscDir + _T("floppy_eject.wav");           my_floppy_eject  =       new wxSound(sndfile);
    sndfile=rscDir + _T("floppy_insert_sound.wav");    my_floppy_insert =       new wxSound(sndfile);
    sndfile=rscDir + _T("floppy_motor1.wav");          my_floppy_motor1 =       new wxSound(sndfile);
    sndfile=rscDir + _T("floppy_motor2.wav");          my_floppy_motor2 =       new wxSound(sndfile);
    sndfile=rscDir + _T("lisa_power_switch01.wav");    my_lisa_power_switch01 = new wxSound(sndfile);
    sndfile=rscDir + _T("lisa_power_switch02.wav");    my_lisa_power_switch02 = new wxSound(sndfile);
    sndfile=rscDir + _T("poweroffclk.wav");            my_poweroffclk =         new wxSound(sndfile);

	my_lisaframe->UpdateProfileMenu();

    black();

    return true;
}





///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Keyboard Scancodes - these are valid for all keyboards, except that 42 is not on the US keyboard
//                      and 48 is not on the EU keyboard.
//
//                               0    1    2    3    4    5    6    7    8    9   10   11  12   13  14
uint8 kbcodes[5][15]=      { {0x68,0x74,0x71,0x72,0x73,0x64,0x61,0x62,0x63,0x50,0x51,0x40,0x41,0x45, 0 },
                             {0x78,0x75,0x77,0x60,0x65,0x66,0x67,0x52,0x53,0x5f,0x44,0x56,0x57,0x42, 0 },
                             {0x7d,0x70,0x76,0x7b,0x69,0x6a,0x6b,0x54,0x55,0x59,0x5a,0x5b,0x42,0x48, 0 },
                             {0x7e,0x43,0x79,0x7a,0x6d,0x6c,0x6e,0x6f,0x58,0x5d,0x5e,0x4c,0x7e,   0, 0 },
                             {0x7c,0x7f,0x5c,0x46,0x4e,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0 } };
//
//                              0    1    2    3   4
uint8 kbcodesn[ 5][5]=     { {0x20,0x21,0x22,0x23, 0},
                             {0x24,0x25,0x26,0x27, 0},
                             {0x28,0x29,0x2a,0x2b, 0},
                             {0x4d,0x2d,0x2e,0x2f, 0},
                             {0x49,0x2c,   0,   0, 0}                                                    };
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Keyboard Legends - need to build these for the other languages /////////////////////////////////////////////////
//                  - also need to build 3x more (shifted, option, option+shift)
//
//                             0        1   2   3   4   5   6   7   8   9   10 11  12  13          14
char *kb_us_r1_txt[5][15]= { {"`",     "1","2","3","4","5","6","7","8","9","0","-","=","BACKSPACE","" },
                             {"TAB",   "q","w","e","r","t","y","u","i","o","p","[","]","\\"       ,"" },
                             {"CAPS",  "a","s","d","f","g","h","j","k","l",";","'","" ,"RETURN"   ,"" },
                             {"SHIFT", "", "z","x","c","v","b","n","m",",",".","/","SHIFT",""     ,"" },
                             {"OPT","CMD","SPACE","ENTER","OPT","","","","","","","","",""        ,"" }  };
//
char *kb_us_n1_txt[ 5][5]= { {"CLR","-","+","*"    ,""  },
                             {  "7","8","9","/"    ,""  },
                             {  "4","5","6",","    ,""  },
                             {  "1","2","3","ENTER",""  },
                             {  "0",".","" ,""     ,""  }                                                };
//
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//borrowed from wxWidgets example code:  keyboard.cpp by  Vadim Zeitlin
void LisaWin::LogKeyEvent(const wxChar* WXUNUSED(name), wxKeyEvent& event, int keydir)
{
    wxString key;
    long keycode = event.GetKeyCode();
    int lisakey=0;

    int forcelisakey=0;

    // on linux for some reason I get keycode=-1 for ALT!!!
    #ifdef __WXX11__
    if (keycode==-1) keycode=WXK_ALT;
    #endif

    //ALERT_LOG(0,"Received %08x keycode on event %d",keycode,keydir);
    switch ( keycode )
    {
//
// need to implement variations of these for the other keyboard languages too.
// also should get rid of this and replace it with some sort of table/array instead.
//

        case WXK_LEFT:
        case WXK_NUMPAD_LEFT:        lisakey=KEYCODE_CURSORL;  forcelisakey=1;       break;

        case WXK_UP :
        case WXK_NUMPAD_UP:          lisakey=KEYCODE_CURSORU;  forcelisakey=1;       break;

        case WXK_RIGHT :
        case WXK_NUMPAD_RIGHT:       lisakey=KEYCODE_CURSORR;  forcelisakey=1;       break;

        case WXK_DOWN:
        case WXK_NUMPAD_DOWN :       lisakey=KEYCODE_CURSORD;  forcelisakey=1;       break;


        case WXK_NUMPAD_INSERT:      lisakey=KEYCODE_LENTER;          break;
        case WXK_NUMPAD_DELETE:      lisakey=KEYCODE_BACKSPACE;       break;

      //case WXK_NUMPAD_EQUAL:       lisakey=KEYCODE_EQUAL;           break;
        case WXK_NUMPAD_MULTIPLY:    lisakey=KEYCODE_STAR_NUM;        break;
        case WXK_NUMPAD_ADD:         lisakey=KEYCODE_PLUS_NUM;        break;
        case WXK_NUMPAD_SEPARATOR:
        case WXK_NUMPAD_SUBTRACT:    lisakey=KEYCODE_MINUS_NUM;       break;
        case WXK_NUMPAD_DECIMAL:     lisakey=KEYCODE_DOTNUM;          break;
        case WXK_NUMPAD_DIVIDE:      lisakey=KEYCODE_FSLASH;          break;

        case WXK_NUMPAD_ENTER:       lisakey=KEYCODE_ENTERNUM;        break;
        case WXK_NUMLOCK:            lisakey=KEYCODE_CLEAR;           break;

        case WXK_NUMPAD0:            lisakey=KEYCODE_0NUM;            break;
        case WXK_NUMPAD1:            lisakey=KEYCODE_1NUM;            break;
        case WXK_NUMPAD2:            lisakey=KEYCODE_2NUM;            break;
        case WXK_NUMPAD3:            lisakey=KEYCODE_3NUM;            break;
        case WXK_NUMPAD4:            lisakey=KEYCODE_4NUM;            break;
        case WXK_NUMPAD5:            lisakey=KEYCODE_5NUM;            break;
        case WXK_NUMPAD6:            lisakey=KEYCODE_6NUM;            break;
        case WXK_NUMPAD7:            lisakey=KEYCODE_7NUM;            break;
        case WXK_NUMPAD8:            lisakey=KEYCODE_8NUM;            break;
        case WXK_NUMPAD9:            lisakey=KEYCODE_9NUM;            break;

        case WXK_BACK:               lisakey=KEYCODE_BACKSPACE;       break;
        case WXK_TAB:                lisakey=KEYCODE_TAB;             break;
        case WXK_RETURN:             lisakey=KEYCODE_RETURN;          break;
        case WXK_ESCAPE:             lisakey=KEYCODE_CLEAR;           break;
        case WXK_SPACE:              lisakey=KEYCODE_SPACE;           break;
        case WXK_DELETE:             lisakey=KEYCODE_BACKSPACE;       break;



        case 'a' :                   lisakey=KEYCODE_A;               break;
        case 'b' :                   lisakey=KEYCODE_B;               break;
        case 'c' :                   lisakey=KEYCODE_C;               break;
        case 'd' :                   lisakey=KEYCODE_D;               break;
        case 'e' :                   lisakey=KEYCODE_E;               break;
        case 'f' :                   lisakey=KEYCODE_F;               break;
        case 'g' :                   lisakey=KEYCODE_G;               break;
        case 'h' :                   lisakey=KEYCODE_H;               break;
        case 'i' :                   lisakey=KEYCODE_I;               break;
        case 'j' :                   lisakey=KEYCODE_J;               break;
        case 'k' :                   lisakey=KEYCODE_K;               break;
        case 'l' :                   lisakey=KEYCODE_L;               break;
        case 'm' :                   lisakey=KEYCODE_M;               break;
        case 'n' :                   lisakey=KEYCODE_N;               break;
        case 'o' :                   lisakey=KEYCODE_O;               break;
        case 'p' :                   lisakey=KEYCODE_P;               break;
        case 'q' :                   lisakey=KEYCODE_Q;               break;
        case 'r' :                   lisakey=KEYCODE_R;               break;
        case 's' :                   lisakey=KEYCODE_S;               break;
        case 't' :                   lisakey=KEYCODE_T;               break;
        case 'u' :                   lisakey=KEYCODE_U;               break;
        case 'v' :                   lisakey=KEYCODE_V;               break;
        case 'w' :                   lisakey=KEYCODE_W;               break;
        case 'x' :                   lisakey=KEYCODE_X;               break;
        case 'y' :                   lisakey=KEYCODE_Y;               break;
        case 'z' :                   lisakey=KEYCODE_Z;               break;

        case 'A' :                   lisakey=KEYCODE_A;               break;
        case 'B' :                   lisakey=KEYCODE_B;               break;
        case 'C' :                   lisakey=KEYCODE_C;               break;
        case 'D' :                   lisakey=KEYCODE_D;               break;
        case 'E' :                   lisakey=KEYCODE_E;               break;
        case 'F' :                   lisakey=KEYCODE_F;               break;
        case 'G' :                   lisakey=KEYCODE_G;               break;
        case 'H' :                   lisakey=KEYCODE_H;               break;
        case 'I' :                   lisakey=KEYCODE_I;               break;
        case 'J' :                   lisakey=KEYCODE_J;               break;
        case 'K' :                   lisakey=KEYCODE_K;               break;
        case 'L' :                   lisakey=KEYCODE_L;               break;
        case 'M' :                   lisakey=KEYCODE_M;               break;
        case 'N' :                   lisakey=KEYCODE_N;               break;
        case 'O' :                   lisakey=KEYCODE_O;               break;
        case 'P' :                   lisakey=KEYCODE_P;               break;
        case 'Q' :                   lisakey=KEYCODE_Q;               break;
        case 'R' :                   lisakey=KEYCODE_R;               break;
        case 'S' :                   lisakey=KEYCODE_S;               break;
        case 'T' :                   lisakey=KEYCODE_T;               break;
        case 'U' :                   lisakey=KEYCODE_U;               break;
        case 'V' :                   lisakey=KEYCODE_V;               break;
        case 'W' :                   lisakey=KEYCODE_W;               break;
        case 'X' :                   lisakey=KEYCODE_X;               break;
        case 'Y' :                   lisakey=KEYCODE_Y;               break;
        case 'Z' :                   lisakey=KEYCODE_Z;               break;

        case '0' :                   lisakey=KEYCODE_0;               break;
        case '1' :                   lisakey=KEYCODE_1;               break;
        case '2' :                   lisakey=KEYCODE_2;               break;
        case '3' :                   lisakey=KEYCODE_3;               break;
        case '4' :                   lisakey=KEYCODE_4;               break;
        case '5' :                   lisakey=KEYCODE_5;               break;
        case '6' :                   lisakey=KEYCODE_6;               break;
        case '7' :                   lisakey=KEYCODE_7;               break;
        case '8' :                   lisakey=KEYCODE_8;               break;
        case '9' :                   lisakey=KEYCODE_9;               break;


        case        '~':
        case        '`':             lisakey=KEYCODE_TILDE;           break;

        case        '_':
        case        '-':             lisakey=KEYCODE_MINUS;           break;

        case        '+':
        case        '=':             lisakey=KEYCODE_PLUS;            break;

        case        '{':
        case        '[':             lisakey=KEYCODE_OBRAK;           break;

        case        '}':
        case        ']':             lisakey=KEYCODE_CBRAK;           break;


        case        '|':
        case        '\\':            lisakey=KEYCODE_BSLASH;          break;

        case        ':':
        case        ';':             lisakey=KEYCODE_COLON;           break;

        case        '"':
        case        '\'':            lisakey=KEYCODE_QUOTE;           break;

        case        '<':
        case        ',':             lisakey=KEYCODE_COMMA;           break;

        case        '>':
        case        '.':             lisakey=KEYCODE_DOT;             break;

        case        '?':
        case        '/':             lisakey=KEYCODE_FSLASHQ;         break;


        case WXK_SHIFT:              lisakey=KEYCODE_SHIFT;           break;
        case WXK_ALT:                lisakey=KEYCODE_COMMAND;         break;
        case WXK_CONTROL:            lisakey=KEYCODE_LOPTION;         break;
    }

int i;

switch (asciikeyboard)
{
 case 1:

                 if (keydir==2 && lastkeystroke==-1)
                    {
                        if (forcelisakey)   {
                                              if (forcelisakey & 8) send_cops_keycode(KEYCODE_COMMAND|KEY_DOWN);
                                              if (forcelisakey & 4) send_cops_keycode(KEYCODE_SHIFT  |KEY_DOWN);
                                              if (forcelisakey & 2) send_cops_keycode(KEYCODE_OPTION |KEY_DOWN);
                                                                    send_cops_keycode(lisakey        |KEY_DOWN);
                                                                    send_cops_keycode(lisakey        |KEY_UP  );
                                              if (forcelisakey & 2) send_cops_keycode(KEYCODE_OPTION |KEY_UP  );
                                              if (forcelisakey & 4) send_cops_keycode(KEYCODE_SHIFT  |KEY_UP  );
                                              if (forcelisakey & 8) send_cops_keycode(KEYCODE_COMMAND|KEY_UP  );
                                            }
                        else
                                            keystroke_cops(keycode);
                    }
                 lastkeystroke=-1;
                 return;


case 0 :         //     old raw unbuffered way - causes repetitions!
                 if (keydir==-1)                lisakey |=KEY_DOWN;
                 if (lisakey & 0x7f)            {//ALERT_LOG(0,"Sending rawkeycode %02x",lisakey);
                                                 send_cops_keycode(lisakey);}
                 lastkeystroke=-1;
                 return;

case -1 :
                 // raw keycodes are a lot more complicated because of timing, the Lisa will almost always
                 // repeat keys when they are unwanted.  This code attempts to buffer the codes and later
                 // on key-up events, let go of them.  It attempts to handle things such as holding shift,
                 // then typing A,B,C, and will send shift-down,a-down,a-up,shift-up,shift-down,b-down, etc...
                 //
                 // in some ways it's an attempt to "cook" raw keycodes, but it doesn't use ASCII translation.



                 // handle repeating keys by watching the host repeat
                 if (keydir==-1 && rawidx>0 && rawcodes[rawidx-1]==lisakey) keydir=1;


                 // key down
                 if (keydir==-1)                     // key down, don't send anything, just buffer.
                 {
                    // see if wxWidgets will send more than one keydown, if it does we have a way!
                    if (rawidx>=127) return;         // avoid overflow
                    if (rawidx) for ( i=0; i<rawidx; i++)
                                     if (lisakey==rawcodes[i])
                                        {
                                         //ALERT_LOG(0,"Suppressing duplicate keydown:%d",lisakey);
                                         return;
                                        }


                    rawcodes[rawidx]=lisakey;
                    //ALERT_LOG(0,"Queueing rawcodes[%d]=%02x ",rawidx,lisakey);
                    rawidx++;
                 }





                 // so this can handle shift-keydown-keyup-shift up, but won't handle any more than that.
                 // also handle shift-keydown-keyup-keydown-up-down-up-shift-up!
                 //
                 // also handle shiftdown,control-down,option-down-key-down-up,keydown-up, control-up, ...etc.
                 //
                 // so want to release all keys the first time any key is released, but don't flush the buffer until
                 // all keys are released!
                 //
                 // * also need to handle shift-down-adown-shift-up-a-up, etc.
                 //
                 // * remaining bug here is that this won't handle repeating keys.

                 if (keydir==1)                      // key released
                 {
                   int i,flag=0;
                   if (rawidx==0) {//ALERT_LOG(0,"Returning since rawdown=0");
                                   return;}           // nothing in the buffer
                   //static int lastkeyup;

                   //if (lisakey==lastkeyup) ALERT_LOG(0,"Multiple key up:%d",lisakey);

                   // prevent keydown syndrome. i.e. when typing there, if you type in like this:
                   // t-down, h-down, t-up, h-up, you get "tht" without this return.  Most people aren't
                   // aware that they're doing this, I certainly wasn't until just down because most kbd
                   // drivers compensate for this!
                   //if (lisakey!=rawcodes[rawidx-1]) return;  // this isn't right. *** want to ignore any keys up no longer in the buffer ***

                   if (lisakey!=rawcodes[rawidx-1] &&
                       lisakey!=KEYCODE_COMMAND    &&
                       lisakey!=KEYCODE_OPTION     &&
                       lisakey!=KEYCODE_SHIFT        )
                      for (i=0; i<rawidx; i++)
                      {
                          if (//rawcodes[i]!=lisakey &&
                              rawcodes[i]!=0                &&
                              rawcodes[i]!=KEYCODE_COMMAND  &&
                              rawcodes[i]!=KEYCODE_OPTION   &&
                              rawcodes[i]!=KEYCODE_SHIFT       )
                              { //ALERT_LOG(0,"all non-meta keys have not yet been released.");
                                return;}
                      }


                   //--------------------------------------------------------------------------
                   // if there's nothing left in the buffer except for Apple,Option, and shift
                   // we don't have anything to send to the Lisa.  i.e. on final release of shift
                   // or option, or Apple key.

                   for (flag=0,i=0; i<rawidx && !flag; i++)
                   {
                       if (rawcodes[i]!=0                &&
                           rawcodes[i]!=KEYCODE_COMMAND  &&
                           rawcodes[i]!=KEYCODE_OPTION   &&
                           rawcodes[i]!=KEYCODE_SHIFT       ) flag=1;
                   }

                   // if the user let go of a modifier key, don't return anything, just remove it
                   if ( (lisakey==KEYCODE_COMMAND ||
                         lisakey==KEYCODE_OPTION  ||
                         lisakey==KEYCODE_SHIFT     ) )
                         {
                               for (i=0; i<rawidx && rawcodes[i]!=lisakey; i++);    //find modifier to delete.
                               if (i<rawidx-1) memmove(&rawcodes[i],&rawcodes[i+1],(128-i)*sizeof(int));

                               rawidx--;

                               // safety kludge - ensure that we let go of the modifiers so we don't leave the
                               // keyboard in a screwey state.
                               if (!rawidx) {send_cops_keycode(KEYCODE_COMMAND | KEY_UP);
                                             send_cops_keycode(KEYCODE_OPTION  | KEY_UP);
                                             send_cops_keycode(KEYCODE_SHIFT   | KEY_UP);
                                            }

                                 return;
                         }
                   //--------------------------------------------------------------------------
                   if (!flag) {//ALERT_LOG(0,"all left is shift/opt/cmd - returning");

                               // safety kludge - ensure that we let go of the modifiers so we don't leave the
                               // keyboard in a screwey state.
                               if (!rawidx) {send_cops_keycode(KEYCODE_COMMAND | KEY_UP);
                                             send_cops_keycode(KEYCODE_OPTION  | KEY_UP);
                                             send_cops_keycode(KEYCODE_SHIFT   | KEY_UP);
                                            }
                               return; }              // if all that's left are modifiers, return.


                   //ALERT_LOG(0,"\nlisakey Keyup:%d",lisakey);
                   for (i=0; i<rawidx; i++)         { //ALERT_LOG(0,"sending down[%d]=%d",i,rawcodes[i]);
                                                      if (rawcodes[i]!=0)
                                                       send_cops_keycode(rawcodes[i]|KEY_DOWN); }

                   for (i=rawidx-1; i>=0; i--)      { //ALERT_LOG(0,"sending up[%d]=%d",i,rawcodes[i]);
                                                      if (rawcodes[i]!=0)
                                                       send_cops_keycode(rawcodes[i]|KEY_UP);    }


                   //ALERT_LOG(0,"done\n");


                   // remove any keystrokes from the buffer we've just sent out other than the meta keys.
                   for (i=0; i<rawidx; i++)
                   {
                      if (KEYCODE_COMMAND!=rawcodes[i] &&
                          KEYCODE_OPTION !=rawcodes[i] &&
                          KEYCODE_SHIFT  !=rawcodes[i] &&
                                       0 !=rawcodes[i]    ) rawcodes[i]=0;
                   }

                   // if they're at the end, shrink the buffer - it's ok to leave holes, just don't want to.
                   while (rawidx>0 && rawcodes[rawidx-1]==0) rawidx--;

                   // safety kludge - ensure that we let go of the modifiers so we don't leave the
                   // keyboard in a screwey state.
                   if (!rawidx) {send_cops_keycode(KEYCODE_COMMAND | KEY_UP);
                                 send_cops_keycode(KEYCODE_OPTION  | KEY_UP);
                                 send_cops_keycode(KEYCODE_SHIFT   | KEY_UP);
                                }
                   return;



                 }
        return;

}


}


void LisaWin::OnKeyDown(wxKeyEvent& event)
{

        long keycode = event.GetKeyCode();

        //ALERT_LOG(0,"Key down:%x, shift:%x, alt:%x, control:%x", keycode,
        //          WXK_SHIFT,
        //          WXK_ALT,
        //          WXK_CONTROL);

        if (keycode==-1) keycode=WXK_ALT;


        if (
             keycode==WXK_LEFT ||                   // avoid cursor keys turning into ascii
             keycode==WXK_UP   ||
             keycode==WXK_RIGHT||
             keycode==WXK_DOWN ||

             keycode==WXK_NUMPAD_LEFT ||
             keycode==WXK_NUMPAD_UP   ||
             keycode==WXK_NUMPAD_RIGHT||
             keycode==WXK_NUMPAD_DOWN   )            {LogKeyEvent(_T("Key down"), event,-1);event.Skip(); return;}


    if (asciikeyboard==1) event.Skip(); else LogKeyEvent(_T("Key down"), event,-1);

}

void LisaWin::OnKeyUp(wxKeyEvent& event)
{
    //ALERT_LOG(0,"Key Up. Throttle:%f clk:%f",my_lisaframe->throttle,my_lisaframe->clockfactor);


        long keycode = event.GetKeyCode();

        if (
             keycode==WXK_LEFT ||                   // avoid cursor keys turning into ascii
             keycode==WXK_UP   ||
             keycode==WXK_RIGHT||
             keycode==WXK_DOWN ||

             keycode==WXK_NUMPAD_LEFT ||
             keycode==WXK_NUMPAD_UP   ||
             keycode==WXK_NUMPAD_RIGHT||
             keycode==WXK_NUMPAD_DOWN   )            {LogKeyEvent(_T("Key up"), event,1);event.Skip(); return;}


    if (asciikeyboard==1) event.Skip(); else LogKeyEvent(_T("Key up"), event,1);

    videoramdirty++;
}

void LisaWin::OnChar(wxKeyEvent& event)
{

   long keycode = event.GetKeyCode();
   if (keycode==-1) keycode=WXK_ALT;

   if (

       keycode==WXK_SHIFT ||                    // avoid inserting extra ascii 1,2,4 on shift,alt,control down.
       keycode==WXK_ALT   ||
       keycode==WXK_CONTROL  ) return;

   wxString x; x.Printf(_T("keychar %c %08lx"),(char)keycode,keycode );
   my_lisaframe->SetStatusBarText(x);
   if (asciikeyboard==1)
      {
        LogKeyEvent(_T("Char"), event, 2);
        event.Skip(false); //else  LogKeyEvent(_T("Char"), event, 2);
      }
}


#define M8(x) (x>255 ? (255): (x))

void LisaWin::ContrastChange(void)
{
  	  if ((contrast>>1)==lastcontrast)      return;

      lastcontrast=contrast>>1;

      int mc=0x88+( 0x7f^(contrast>>1)  );                // my contrast

      if (contrast<0xe0)
      {
        int step=(mc-brightness)/10;
  		/* white */ bright[0]=MIN(240,mc);                                          
  		/* dark0 */ bright[1]=MIN(240,brightness+step*6);                        
  		/* dark1 */ bright[2]=bright[3]=MIN(240,brightness+step*5);               
        /* black */ bright[4]=bright[5]=bright[6]=bright[7]=MIN(240,brightness);    

        /* AAGray*/ bright[8] = bright[9] = bright[10]= bright[11]= bright[12]=
                    bright[13]= bright[14] =bright[15]=                  (mc+brightness)>>1;

     }
  else                                   // contrast is all black
      {
  		bright[0]=0;                  
  		bright[1]=0;                  
  		bright[2]=0;                  
  		bright[3]=0;                  
  		bright[4]=0;                  
  		bright[5]=0;                  
  		bright[6]=0;                  
  		bright[7]=0;                  

        bright[8] = bright[9] = bright[10]= bright[11]= bright[12]=bright[13] = bright[14] =bright[15]=0;

      }

      refresh_bytemap=0;        // we just refreshed the contrast table
      dirtyscreen=1;            // however, the screen bitmap is now dirty
      videoramdirty=32768;

  #ifndef __WXOSX__             // 2007.03.14 disabled for os x since it slows down shutdown a whole lot
      force_refresh();          // win32 still has contrast-trails. :-( 20070216
  #endif

}



// Repainters //////////////////////////////////////////////////////////////////////////////////////////




///////////////// SETRGB 16 bit AntiAliased MACRO //////////////////////////////////////////////////////   
//                                                                                                    //
//  macro to fill in r,g,b values, does 16 pixels at a time, must be a macro because of the Z param   //
//  and we want to get as much speed out of it as possible.                                           //
//                                                                                                    //
//  This would have been a 32 pixel macro if it were possible, however, since a single row is on the  //
//  Lisa's display is either 90 bytes or 76 bytes, we can only evenly divide by 2 bytes (16 bits.)    //
//                                                                                                    //
//                                                                                                    //
//  x,y are x,y coordinates on the bitmap.   They map to the Lisa's display, any other translation    //
//  must be handled in Z, or via other variables.   x gets incremented for each pixel handled.        //
//  x,y must be simple variables, and should not be expressions!                                      //
//                                                                                                    //
//  Z is a chunk of code to set r,g,b provided by the caller.  Z gets executed 16 times, once per     //
//  pixel.  This is needed because we might be using SetRGB on images or rawbitmap accesss, or some   //
//  other method in the future.   Z should be protected by enclosing it in curly braces when passing. //
//  Z may use the uint8 d to actually set the darkness level for a pixel.                             //
//                                                                                                    //
//  Y is identical to Z except that it's code to call when there's no update.  i.e. ++p               //
//                                                                                                    //
//                                                                                                    //
//  The following variables should be declared before calling this macro:                             //
//                                                                                                    //
//         int updated=0;	        // number of times we've made updates.                            //
//                                  // can be used as a threshhold to decide how to redraw the image. //
//         uint32 a1,a2,a3,xx;      // address above, below, for this value, horziontal byte offset   //
//         uint16 vup,vdn,val;      // value above, below, this words read from addr @ a1,a2,a3       //
//         uint16 high,medium,low;  // used for antialiasing. how many neighboring bits               //
//         uint8 d;                 // darkness level to pass to Z                                    //
//                                                                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////


#define SETRGB16_AA(x,y,Z,Y) {                                                                         \
	                                                                                                   \
	 xx=(x)>>3;                                                /*   turn x coord into byte offset */   \
	                                                                                                   \
	 a3=(yoffset[screen_to_mouse[ y           ]]+xx) &32767;   /*   this value we're processing   */   \
	                                                                                                   \
	 val=(lisaram[videolatchaddress+a3]<<8 )|lisaram[videolatchaddress+a3+1];  /*   this word     */   \
                                                                                                       \
     if (videoramdirty>DIRECT_BLITS_THRESHHOLD ||        /*  If full update requested or dirty    */   \
         (dirtyvidram[a3]<<8)|dirtyvidram[a3+1]!=val  )                                                \
      {                                   	                                                           \
       updated++;     	                                 /*  Keep track of update count           */   \
                                                                                                       \
	   dirty_x_min=MIN(x,dirty_x_min);       dirty_x_max=MAX(x+16,dirty_x_max);                        \
	   dirty_y_min=MIN(y,dirty_y_min);  	 dirty_y_max=MAX(y,dirty_y_max);                           \
                                                                                                       \
       d=bright[ ((BIT15 & val ) ? 7:0)  ]; Z; x++;                                                    \
       d=bright[ ((BIT14 & val ) ? 7:0)  ]; Z; x++;                                                    \
       d=bright[ ((BIT13 & val ) ? 7:0)  ]; Z; x++;                                                    \
       d=bright[ ((BIT12 & val ) ? 7:0)  ]; Z; x++;                                                    \
       d=bright[ ((BIT11 & val ) ? 7:0)  ]; Z; x++;                                                    \
       d=bright[ ((BIT10 & val ) ? 7:0)  ]; Z; x++;                                                    \
       d=bright[ ((BIT9  & val ) ? 7:0)  ]; Z; x++;                                                    \
       d=bright[ ((BIT8  & val ) ? 7:0)  ]; Z; x++;                                                    \
       d=bright[ ((BIT7  & val ) ? 7:0)  ]; Z; x++;                                                    \
       d=bright[ ((BIT6  & val ) ? 7:0)  ]; Z; x++;                                                    \
       d=bright[ ((BIT5  & val ) ? 7:0)  ]; Z; x++;                                                    \
       d=bright[ ((BIT4  & val ) ? 7:0)  ]; Z; x++;                                                    \
       d=bright[ ((BIT3  & val ) ? 7:0)  ]; Z; x++;                                                    \
       d=bright[ ((BIT2  & val ) ? 7:0)  ]; Z; x++;                                                    \
       d=bright[ ((BIT1  & val ) ? 7:0)  ]; Z; x++;                                                    \
       d=bright[ ((BIT0  & val ) ? 7:0)  ]; Z; x++;                                                    \
      } 	                                                                                           \
     else                                                                                              \
      {                                                                                                \
	    Y; x++;  Y; x++;  Y; x++;  Y; x++;                                                             \
	    Y; x++;  Y; x++;  Y; x++;  Y; x++;                                                             \
	    Y; x++;  Y; x++;  Y; x++;  Y; x++;                                                             \
	    Y; x++;  Y; x++;  Y; x++;  Y; x++;                                                             \
	  }                                                                                                \
}                                                                                                     //
////////////////// GETRGB MACRO ENDS ///////////////////////////////////////////////////////////////////


void LisaWin::RePaint_AntiAliased(void)
{
   // vars for SETRGB16  
   int updated=0;	
   uint32 a3,xx;
   uint16 val;
   uint8  d;

   dirty_x_min=720; dirty_x_max=-1; dirty_y_min=384*3; dirty_y_max=-1;

#ifdef USE_RAW_BITMAP_ACCESS

   int depth;  
   int width;  
   int height; 
   int ox,oy; // private to this - not member vars
   
   if (skins_on)
      {
	   if (!my_skin) {ALERT_LOG(0,"Null my_skin"); return;}
       depth = my_skin->GetDepth();    width = my_skin->GetWidth();    height= my_skin->GetHeight();
       ox=screen_origin_x;    oy=screen_origin_y;
      }
   else
      {
	    if (!my_lisabitmap->IsOk()) {ALERT_LOG(0,"my_lisabitmap is not ok!");}
		if (!my_lisabitmap) {ALERT_LOG(0,"Null my_lisa_bitmap!"); return;}
       depth = my_lisabitmap->GetDepth();    width = my_lisabitmap->GetWidth();    height= my_lisabitmap->GetHeight();
       ox=0; oy=0;
      }

   typedef wxPixelData<wxBitmap,wxNativePixelFormat> PixelData;
   PixelData data( *( skins_on ? my_skin : my_lisabitmap) );
   if (!data) {ALERT_LOG(0,"No data."); return;}

   PixelData::Iterator p(data);
   p.Reset(data);
   p.MoveTo(data,ox,oy);                            

   for ( int y =0; y< effective_lisa_vid_size_y; y++ )   // effective_lisa_vid_size_y
   {
       PixelData::Iterator rowStart = p;                 // save the x,y coordinates at the start of the line
	   for ( int x = 0; x < effective_lisa_vid_size_x; )
		{ SETRGB16_AA(x,y,  {p.Red()=d; p.Green()=d; p.Blue()=(d+EXTRABLUE); ++p;} , {++p;} );} 
	   p = rowStart; p.OffsetY(data, 1);                 // restore the x,y coords from start of line, then increment y to do y++;
   }

    /////////////////////////////////////////////////////////////////////////////////////////////////
    
#else

  // Since we're working the slower way - with images, we need to rebuild the bitmap from the image.
  // to do this, we discard the old bitmap and recreate it from the display_image.  This is of course
  // slower, which is why we recommend the use of USE_RAW_BITMAP_ACCESS, but USE_RAW_BITMAP_ACCESS
  // might not work everywhere, so we give the option.
  if (skins_on)
  {
    if (!my_skin || !my_lisabitmap) return; 

//    int depth = my_skin->GetDepth();
//    int width = my_skin->GetWidth();
//    int height= my_skin->GetHeight();

 	if (!display_image)   
         display_image= new wxImage(  my_lisabitmap->ConvertToImage());

	for ( int y=0; y < effective_lisa_vid_size_y; y++ )
	    {
		    for ( int x=0; x < effective_lisa_vid_size_x;)
                { SETRGB16_AA(x,y,{display_image->SetRGB(x,y,d,d,d+EXTRABLUE); },{;}); }
	    }
    // delete the old bitmap, then create a new one from the wxImage, and use it.
    delete my_lisabitmap;
	my_lisabitmap=new wxBitmap(*display_image);
	my_memDC->SelectObjectAsSource(*my_lisabitmap);
	   if (!my_memDC->IsOk()) ALERT_LOG(0,"my_memDC is not ok.");
	   if (!my_lisabitmap->IsOk()) ALERT_LOG(0,"my_lisabitmap is not ok.");

    e_dirty_x_min=dirty_x_min;                       // need to do these here so we can update just the rectangle we need.
    e_dirty_x_max=dirty_x_max;                       // it will be repeated again below, but so what, it's only 4 assignments
    e_dirty_y_min=dirty_y_min; //screen_y_map[dirty_y_min];         // and two lookups.
    e_dirty_y_max=dirty_y_max; //screen_y_map[dirty_y_max];

    if (updated)
          my_skinDC->Blit(screen_origin_x + e_dirty_x_min, screen_origin_y + e_dirty_y_min,     // target x,y
                          e_dirty_x_max-e_dirty_x_min+1,   e_dirty_y_max-e_dirty_y_min+1,     // size w,h
                          my_memDC, 0,0, wxCOPY, false);


    // we don't delete display_image since we can reuse it the next time around and save time
  }
  else  // skins are off
  {
    //int depth = my_lisabitmap->GetDepth();
    //int width = my_lisabitmap->GetWidth();
    //int height= my_lisabitmap->GetHeight();

    if (!display_image)   display_image= new wxImage(my_lisabitmap->ConvertToImage());

	for ( int yo = 0 , yi=0; yi < effective_lisa_vid_size_y; yo++,yi++ )
	    {
		   // note neither xi, nor xo are incremented as part of the for-loop header, this is because
		   // the SETRGB16_AA macro expands to 16 iterations, and it increments xi on each iteration.
		   // however, it doesn't do anything with xo, so xo++ is passed as a parameter.
		   for ( int xo = 0, xi=0; xi < effective_lisa_vid_size_x;)
               { SETRGB16_AA(xi,yi,{display_image->SetRGB(xo,yo,d,d,d+EXTRABLUE); xo++;},{xo++;});   }
	    }

    // and this is why this is slower since we need to rebuild the bitmap from the wxImage each time.
	delete my_lisabitmap;
	my_lisabitmap=new wxBitmap(*display_image);
	my_memDC->SelectObjectAsSource(*my_lisabitmap);
  }
#endif
/////////////////////////////////////////////////////////////////////////
	
  e_dirty_x_min=dirty_x_min;                       // need to do these here so we can update just the rectangle we need.
  e_dirty_x_max=dirty_x_max;                       // it will be repeated again below, but so what, it's only 4 assignments
  e_dirty_y_min=dirty_y_min; //screen_y_map[dirty_y_min];         // and two lookups.
  e_dirty_y_max=dirty_y_max; //screen_y_map[dirty_y_max];

   if (updated)
   {
      memcpy(dirtyvidram,&lisaram[videolatchaddress],32768);
      repaintall |= REPAINT_INVALID_WINDOW | REPAINT_VIDEO_TO_SKIN;
	  updated=0;
   }

}

///////////////// SETRGB 16 bit Raw Replacement MACRO //////////////////////////////////////////////////   
//                                                                                                    //
//  macro to fill in r,g,b values, does 16 pixels at a time, must be a macro because of the Z param   //
//  and we want to get as much speed out of it as possible.                                           //
//                                                                                                    //
//  This would have been a 32 pixel macro if it were possible, however, since a single row is on the  //
//  Lisa's display is either 90 bytes or 76 bytes, we can only evenly divide by 2 bytes (16 bits.)    //
//                                                                                                    //
//                                                                                                    //
//  x,y are x,y coordinates on the bitmap.   They map to the Lisa's display, any other translation    //
//  must be handled in Z, or via other variables.   x gets incremented for each pixel handled.        //
//  x,y must be simple variables, and should not be expressions!                                      //
//                                                                                                    //
//  Z is a chunk of code to set r,g,b provided by the caller.  Z gets executed 16 times, once per     //
//  pixel.  This is needed because we might be using SetRGB on images or rawbitmap accesss, or some   //
//  other method in the future.   Z should be protected by enclosing it in curly braces when passing. //
//  Z may use the uint8 d to actually set the darkness level for a pixel.                             //
//                                                                                                    //
//  Y is identical to Z except that it's code to call when there's no update.  i.e. ++p               //
//                                                                                                    //
//                                                                                                    //
//  The following variables should be declared before calling this macro:                             //
//                                                                                                    //
//         int updated=0;	        // number of times we've made updates.                            //
//                                  // can be used as a threshhold to decide how to redraw the image. //
//         uint32 a1,a2,a3,xx;      // address above, below, for this value, horziontal byte offset   //
//         uint16 vup,vdn,val;      // value above, below, this words read from addr @ a1,a2,a3       //
//         uint16 high,medium,low;  // used for antialiasing. how many neighboring bits               //
//         uint8 d;                 // darkness level to pass to Z                                    //
//                                                                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////

#define SETRGB16_RAW_X(x,y,Z) {                                                                        \
	                                                                                                   \
	 xx=(x)>>3;                                                /*   turn x coord into byte offset */   \
	                                                                                                   \
	 a3=(yoffset[y]+xx) &32767;                                /*   this value we're processing   */   \
	 val=(lisaram[videolatchaddress+a3]<<8 )|lisaram[videolatchaddress+a3+1];  /*   this word     */   \
                                                                                                       \
       updated++;     	                                      /*  Keep track of update count      */   \
                                                                                                       \
	   dirty_x_min=MIN(x,dirty_x_min);       dirty_x_max=MAX(x+16,dirty_x_max);                        \
	   dirty_y_min=MIN(y,dirty_y_min);  	 dirty_y_max=MAX(y,dirty_y_max);                           \
                                                                                                       \
	                                                                                                   \
       d=bright[ ((BIT15 & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT14 & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT13 & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT12 & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT11 & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT10 & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT9  & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT8  & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT7  & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT6  & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT5  & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT4  & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT3  & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT2  & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT1  & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT0  & val) ? 7:0) ]; Z; x++;                                                      \
}                                                                                                     //
////////////////// SETRGB MACRO ENDS ///////////////////////////////////////////////////////////////////


///////////////// SETRGB 16 bit Raw Replacement MACRO //////////////////////////////////////////////////   
//                                                                                                    //
//  macro to fill in r,g,b values, does 16 pixels at a time, must be a macro because of the Z param   //
//  and we want to get as much speed out of it as possible.                                           //
//                                                                                                    //
//  This would have been a 32 pixel macro if it were possible, however, since a single row is on the  //
//  Lisa's display is either 90 bytes or 76 bytes, we can only evenly divide by 2 bytes (16 bits.)    //
//                                                                                                    //
//                                                                                                    //
//  x,y are x,y coordinates on the bitmap.   They map to the Lisa's display, any other translation    //
//  must be handled in Z, or via other variables.   x gets incremented for each pixel handled.        //
//  x,y must be simple variables, and should not be expressions!                                      //
//                                                                                                    //
//  Z is a chunk of code to set r,g,b provided by the caller.  Z gets executed 16 times, once per     //
//  pixel.  This is needed because we might be using SetRGB on images or rawbitmap accesss, or some   //
//  other method in the future.   Z should be protected by enclosing it in curly braces when passing. //
//  Z may use the uint8 d to actually set the darkness level for a pixel.                             //
//                                                                                                    //
//  Y is identical to Z except that it's code to call when there's no update.  i.e. ++p               //
//                                                                                                    //
//                                                                                                    //
//  The following variables should be declared before calling this macro:                             //
//                                                                                                    //
//         int updated=0;	        // number of times we've made updates.                            //
//                                  // can be used as a threshhold to decide how to redraw the image. //
//         uint32 a1,a2,a3,xx;      // address above, below, for this value, horziontal byte offset   //
//         uint16 vup,vdn,val;      // value above, below, this words read from addr @ a1,a2,a3       //
//         uint16 high,medium,low;  // used for antialiasing. how many neighboring bits               //
//         uint8 d;                 // darkness level to pass to Z                                    //
//                                                                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////

#define SETRGB16_RAW(x,y,Z,Y) {                                                                        \
	                                                                                                   \
	 xx=(x)>>3;                                                /*   turn x coord into byte offset */   \
	                                                                                                   \
	 a3=(yoffset[screen_to_mouse[ y           ]]+xx) &32767;   /*   this value we're processing   */   \
	 val=(lisaram[videolatchaddress+a3]<<8 )|lisaram[videolatchaddress+a3+1];  /*   this word     */   \
                                                                                                       \
     if (videoramdirty>DIRECT_BLITS_THRESHHOLD ||        /*  If full update requested or          */   \
         (dirtyvidram[a3]<<8)|dirtyvidram[a3+1]!=val  )  /*  value is changed                     */   \
      {                                   	                                                           \
       updated++;     	                                 /*  Keep track of update count           */   \
                                                                                                       \
	   dirty_x_min=MIN(x,dirty_x_min);       dirty_x_max=MAX(x+16,dirty_x_max);                        \
	   dirty_y_min=MIN(y,dirty_y_min);  	 dirty_y_max=MAX(y,dirty_y_max);                           \
                                                                                                       \
	                                                                                                   \
       d=bright[ ((BIT15 & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT14 & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT13 & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT12 & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT11 & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT10 & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT9  & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT8  & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT7  & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT6  & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT5  & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT4  & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT3  & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT2  & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT1  & val) ? 7:0) ]; Z; x++;                                                      \
       d=bright[ ((BIT0  & val) ? 7:0) ]; Z; x++;                                                      \
      } 	                                                                                           \
     else                                                                                              \
      {                                                                                                \
	    Y; x++;  Y; x++;  Y; x++;  Y; x++;                                                             \
	    Y; x++;  Y; x++;  Y; x++;  Y; x++;                                                             \
	    Y; x++;  Y; x++;  Y; x++;  Y; x++;                                                             \
	    Y; x++;  Y; x++;  Y; x++;  Y; x++;                                                             \
	  }                                                                                                \
}                                                                                                     //
////////////////// SETRGB MACRO ENDS ///////////////////////////////////////////////////////////////////

void LisaWin::RePaint_SingleY(void)
{
   // vars for SETRGB16_RAW
   int updated=0;	
   uint32 a3,xx;
   uint16 val;
   uint8  d;

   dirty_x_min=720; dirty_x_max=-1; dirty_y_min=384*3; dirty_y_max=-1;

#ifdef USE_RAW_BITMAP_ACCESS

 int depth;  
 int width;  
 int height; 
 int ox,oy; // private to this - not member vars

   
if (skins_on)
   {
    if (!my_skin) return;
    depth = my_skin->GetDepth();    width = my_skin->GetWidth();    height= my_skin->GetHeight();
    ox=screen_origin_x;    oy=screen_origin_y;
   }
else
   {
    if (!my_lisabitmap) return;
    depth = my_lisabitmap->GetDepth();    width = my_lisabitmap->GetWidth();    height= my_lisabitmap->GetHeight();
    ox=0; oy=0;
   }

   typedef wxPixelData<wxBitmap,wxNativePixelFormat> PixelData;
   PixelData data(skins_on ? *my_skin:*my_lisabitmap);
   if (!data) {DEBUG_LOG(0,"No data."); return;}

   PixelData::Iterator p(data);
   p.Reset(data);

   p.MoveTo(data,ox,oy);                            
   for ( int y = 0; y < effective_lisa_vid_size_y; ++y )
   {
       PixelData::Iterator rowStart = p;              // save the x,y coordinates at the start of the line
	   for ( int x = 0; x < effective_lisa_vid_size_x; )
	       { SETRGB16_RAW(x,y,  {p.Red()=d; p.Green()=d; p.Blue()=(d+EXTRABLUE); ++p;} , {++p;} );}  

       p = rowStart; p.OffsetY(data, 1);              // restore the x,y coords from start of line, then increment y via P.OffsetY to do y++;
   }

#else

  // Since we're working the slower way - with images, we need to rebuild the bitmap from the image.
  // to do this, we discard the old bitmap and recreate it from the display_image.  This is of course
  // slower, which is why we recommend the use of USE_RAW_BITMAP_ACCESS, but USE_RAW_BITMAP_ACCESS
  // might not work everywhere, so we give the option.
  if (skins_on)
  {
    if (!my_skin || !my_lisabitmap) return; 

    //int depth = my_skin->GetDepth();
    //int width = my_skin->GetWidth();
    //int height= my_skin->GetHeight();

 	if (!display_image)   
         display_image= new wxImage(  my_lisabitmap->ConvertToImage());

	for ( int y=0; y < effective_lisa_vid_size_y; y++ )
	    {
		   // note neither xi, nor xo are incremented as part of the for-loop header, this is because
		   // the SETRGB16_AA macro expands to 16 iterations, and it increments xi on each iteration.
		   // however, it doesn't do anything with xo, so xo++ is passed as a parameter.
		    for ( int x=0; x < effective_lisa_vid_size_x;)
                { SETRGB16_RAW(x,y,{display_image->SetRGB(x,y,d,d,d+EXTRABLUE); },{;}); }
	    }

    delete my_lisabitmap;
	my_lisabitmap=new wxBitmap(*display_image);
	my_memDC->SelectObjectAsSource(*my_lisabitmap);
	
    e_dirty_x_min=dirty_x_min;                       // need to do these here so we can update just the rectangle we need.
    e_dirty_x_max=dirty_x_max;                       // it will be repeated again below, but so what, it's only 4 assignments
    e_dirty_y_min=dirty_y_min; //screen_y_map[dirty_y_min];         // and two lookups.
    e_dirty_y_max=dirty_y_max; //screen_y_map[dirty_y_max];

    if (updated)
         my_skinDC->Blit(screen_origin_x + e_dirty_x_min, screen_origin_y+e_dirty_y_min,     // target x,y
                    e_dirty_x_max-e_dirty_x_min+1,   e_dirty_y_max-e_dirty_y_min+1,     // size w,h
                    my_memDC, 0,0, wxCOPY, false);
	
  }
 else  // skins are off
  {
    //int depth = my_lisabitmap->GetDepth();
    //int width = my_lisabitmap->GetWidth();
    //int height= my_lisabitmap->GetHeight();

    if (!display_image)   display_image= new wxImage(my_lisabitmap->ConvertToImage());

	for ( int yo = 0 , yi=0; yi < effective_lisa_vid_size_y; yo++,yi++ )
	    {
		   // note neither xi, nor xo are incremented as part of the for-loop header, this is because
		   // the SETRGB16_AA macro expands to 16 iterations, and it increments xi on each iteration.
		   // however, it doesn't do anything with xo, so xo++ is passed as a parameter.
		   for ( int xo = 0, xi=0; xi < effective_lisa_vid_size_x;)
               { SETRGB16_RAW(xi,yi,{display_image->SetRGB(xo,yo,d,d,d+EXTRABLUE); xo++;},{xo++;});   }
	    }

	delete my_lisabitmap;
	my_lisabitmap=new wxBitmap(*display_image);
	my_memDC->SelectObjectAsSource(*my_lisabitmap);
  }
#endif
/////////////////////////////////////////////////////////////////////////

  e_dirty_x_min=dirty_x_min;                       // need to do these here so we can update just the rectangle we need.
  e_dirty_x_max=dirty_x_max;                       // it will be repeated again below, but so what, it's only 4 assignments
  e_dirty_y_min=dirty_y_min; //screen_y_map[dirty_y_min];         // and two lookups.
  e_dirty_y_max=dirty_y_max; //screen_y_map[dirty_y_max];


  if (updated)
  {
      memcpy(dirtyvidram,&lisaram[videolatchaddress],32768);
      repaintall |= REPAINT_INVALID_WINDOW | REPAINT_VIDEO_TO_SKIN;
	  updated=0;
  }


}

// this is essentially the same as SingleY, only for a differently sized display.
void LisaWin::RePaint_3A(void)
{
   // vars for SETRGB16_RAW
   int updated=0;	
   uint32 a3,xx;
   uint16 val;
   uint8  d;

   dirty_x_min=720; dirty_x_max=-1; dirty_y_min=384*3; dirty_y_max=-1;

#ifdef USE_RAW_BITMAP_ACCESS

 int depth;  
 int width;  
 int height; 
 int ox,oy; // private to this - not member vars

   
if (skins_on)
   {
    if (!my_skin) return;
    depth = my_skin->GetDepth();    width = my_skin->GetWidth();    height= my_skin->GetHeight();
    ox=screen_origin_x;    oy=screen_origin_y;
   }
else
   {
    if (!my_lisabitmap) return;
    depth = my_lisabitmap->GetDepth();    width = my_lisabitmap->GetWidth();    height= my_lisabitmap->GetHeight();
    ox=0; oy=0;
   }

   typedef wxPixelData<wxBitmap,wxNativePixelFormat> PixelData;
   PixelData data(skins_on ? *my_skin:*my_lisabitmap);

   PixelData::Iterator p(data);
   p.Reset(data);
   p.MoveTo(data,ox,oy);                             

   for ( int y = 0; y < effective_lisa_vid_size_y; ++y )   // effective_lisa_vid_size_y
   {
       PixelData::Iterator rowStart = p;                   // save the x,y coordinates at the start of the line

	   for ( int x = 0; x < effective_lisa_vid_size_x; )
	       { SETRGB16_RAW(x,y,  {p.Red()=d; p.Green()=d; p.Blue()=(d+EXTRABLUE); ++p;} , {++p;} );}  

       p = rowStart; p.OffsetY(data, 1);                   // restore the x,y coords from start of line, then increment y to do y++;
   }

#else

  // Since we're working the slower way - with images, we need to rebuild the bitmap from the image.
  // to do this, we discard the old bitmap and recreate it from the display_image.  This is of course
  // slower, which is why we recommend the use of USE_RAW_BITMAP_ACCESS, but USE_RAW_BITMAP_ACCESS
  // might not work everywhere, so we give the option.
  if (skins_on)
  {
    if (!my_skin || !my_lisabitmap) return; 

    //int depth = my_skin->GetDepth();
    //int width = my_skin->GetWidth();
    //int height= my_skin->GetHeight();

 	if (!display_image)   
         display_image= new wxImage(  my_lisabitmap->ConvertToImage());

	for ( int y=0; y < effective_lisa_vid_size_y; y++ )
	    {
		   // note neither xi, nor xo are incremented as part of the for-loop header, this is because
		   // the SETRGB16_AA macro expands to 16 iterations, and it increments xi on each iteration.
		   // however, it doesn't do anything with xo, so xo++ is passed as a parameter.
		    for ( int x=0; x < effective_lisa_vid_size_x;)
                { SETRGB16_RAW(x,y,{display_image->SetRGB(x,y,d,d,d+EXTRABLUE); },{;}); }
	    }

    delete my_lisabitmap;
	my_lisabitmap=new wxBitmap(*display_image);
	my_memDC->SelectObjectAsSource(*my_lisabitmap);
	
    e_dirty_x_min=dirty_x_min;                       // need to do these here so we can update just the rectangle we need.
    e_dirty_x_max=dirty_x_max;                       // it will be repeated again below, but so what, it's only 4 assignments
    e_dirty_y_min=dirty_y_min; //screen_y_map[dirty_y_min];         // and two lookups.
    e_dirty_y_max=dirty_y_max; //screen_y_map[dirty_y_max];

    if (updated)
        my_skinDC->Blit(screen_origin_x + e_dirty_x_min, screen_origin_y+e_dirty_y_min,     // target x,y
                    e_dirty_x_max-e_dirty_x_min+1,   e_dirty_y_max-e_dirty_y_min+1,     // size w,h
                   my_memDC, 0,0, wxCOPY, false);
  }
  else  // skins are off
  {
    //int depth = my_lisabitmap->GetDepth();
    //int width = my_lisabitmap->GetWidth();
    //int height= my_lisabitmap->GetHeight();

    if (!display_image)   display_image= new wxImage(my_lisabitmap->ConvertToImage());

	for ( int y=0; y < effective_lisa_vid_size_y; y++ )
	    {
		   // note neither xi, nor xo are incremented as part of the for-loop header, this is because
		   // the SETRGB16_AA macro expands to 16 iterations, and it increments xi on each iteration.
		   // however, it doesn't do anything with xo, so xo++ is passed as a parameter.
		   //for ( int xo = 0, xi=0; xi < effective_lisa_vid_size_x;)
               // { SETRGB16_RAW(xi,yi,{display_image->SetRGB(xo,yo,d,d,d+EXTRABLUE); xo++;},{xo++;});   }
		    for ( int x=0; x < effective_lisa_vid_size_x;)
                { SETRGB16_RAW(x,y,{display_image->SetRGB(x,y,d,d,d+EXTRABLUE); },{;}); }
	    }

	delete my_lisabitmap;
	my_lisabitmap=new wxBitmap(*display_image);
	my_memDC->SelectObjectAsSource(*my_lisabitmap);
  }
#endif
/////////////////////////////////////////////////////////////////////////

  e_dirty_x_min=dirty_x_min;                       // need to do these here so we can update just the rectangle we need.
  e_dirty_x_max=dirty_x_max;                       // it will be repeated again below, but so what, it's only 4 assignments
  e_dirty_y_min=dirty_y_min; //screen_y_map[dirty_y_min];         // and two lookups.
  e_dirty_y_max=dirty_y_max; //screen_y_map[dirty_y_max];

  if (updated)
  {
      memcpy(dirtyvidram,&lisaram[videolatchaddress],32768);
      repaintall |= REPAINT_INVALID_WINDOW | REPAINT_VIDEO_TO_SKIN;
	  updated=0;
  }


}


// no skins here.
void LisaWin::RePaint_DoubleY(void)
{
   // vars for SETRGB16_RAW
   int updated=0;	
   uint32 a3,xx;
   uint16 val;
   uint8  d;


   if (skins_on) {ALERT_LOG(0,"Skins should not be on!!!!"); turn_skins_off();}
   if (!my_lisabitmap) return;
   dirty_x_min=720; dirty_x_max=-1; dirty_y_min=384*3; dirty_y_max=-1;

#ifdef USE_RAW_BITMAP_ACCESS

   int depth;  
   int width;  
   int height; 
   int ox,oy; // private to this - not member vars

   if (!my_lisabitmap) return;
   depth = my_lisabitmap->GetDepth();    width = my_lisabitmap->GetWidth();    height= my_lisabitmap->GetHeight();
   ox=0; oy=0;
 
   typedef wxPixelData<wxBitmap,wxNativePixelFormat> PixelData;
   PixelData data(*my_lisabitmap);
   if (!data) return;

   PixelData::Iterator p(data);
   p.Reset(data);
   p.MoveTo(data,ox,oy);                             

   for ( int y = 0; y < effective_lisa_vid_size_y; ++y )   // effective_lisa_vid_size_y
   {
       PixelData::Iterator rowStart = p;                   // save the x,y coordinates at the start of the line

	   for ( int x = 0; x < effective_lisa_vid_size_x; )
	       { SETRGB16_RAW(x,y,  {p.Red()=d; p.Green()=d; p.Blue()=(d+EXTRABLUE); ++p;} , {++p;} );}  

       p = rowStart; p.OffsetY(data, 1);                   // restore the x,y coords from start of line, then increment y to do y++;
   }

#else

  // Since we're working the slower way - with images, we need to rebuild the bitmap from the image.
  // to do this, we discard the old bitmap and recreate it from the display_image.  This is of course
  // slower, which is why we recommend the use of USE_RAW_BITMAP_ACCESS, but USE_RAW_BITMAP_ACCESS
  // might not work everywhere, so we give the option.
//  int depth = my_lisabitmap->GetDepth();
//  int width = my_lisabitmap->GetWidth();
//  int height= my_lisabitmap->GetHeight();
  
  if (!display_image)   display_image= new wxImage(my_lisabitmap->ConvertToImage());
  
  for ( int y=0; y < effective_lisa_vid_size_y; y++)
      {
	    for ( int x=0; x < effective_lisa_vid_size_x;)
            { SETRGB16_RAW(x,y,{display_image->SetRGB(x,y,d,d,d+EXTRABLUE); },{;}); }
      }
  
  delete my_lisabitmap;
  my_lisabitmap=new wxBitmap(*display_image);
  my_memDC->SelectObjectAsSource(*my_lisabitmap);

#endif
/////////////////////////////////////////////////////////////////////////

  e_dirty_x_min=dirty_x_min;                       // need to do these here so we can update just the rectangle we need.
  e_dirty_x_max=dirty_x_max;                       // it will be repeated again below, but so what, it's only 4 assignments
  e_dirty_y_min=dirty_y_min; //screen_y_map[dirty_y_min];         // and two lookups.
  e_dirty_y_max=dirty_y_max; //screen_y_map[dirty_y_max];


  if (updated)
  {
      memcpy(dirtyvidram,&lisaram[videolatchaddress],32768);
      repaintall |= REPAINT_INVALID_WINDOW | REPAINT_VIDEO_TO_SKIN;
	  updated=0;
  }


}


void LisaWin::RePaint_2X3Y(void)
{
	   // vars for SETRGB16_RAW
	   int updated=0;	
	   uint32 a3,xx;
	   uint16 val;
	   uint8  d;


  	   if (!my_lisabitmap) return;
       dirty_x_min=720; dirty_x_max=-1; dirty_y_min=384*3; dirty_y_max=-1;
	
#ifdef USE_RAW_BITMAP_ACCESS

	   int depth;  
	   int width;  
	   int height; 
	   int ox,oy; // private to this - not member vars

	   if (!my_lisabitmap) return;
	   depth = my_lisabitmap->GetDepth();    width = my_lisabitmap->GetWidth();    height= my_lisabitmap->GetHeight();
	   ox=0; oy=0;
	 
	   typedef wxPixelData<wxBitmap,wxNativePixelFormat> PixelData;
	   PixelData data(*my_lisabitmap);
   	   if (!data) return;

	   PixelData::Iterator p(data);
	   p.Reset(data);
       p.MoveTo(data,ox,oy);

	   for ( int y = 0; y < effective_lisa_vid_size_y; ++y )   // effective_lisa_vid_size_y
	   {
	       PixelData::Iterator rowStart = p;              // save the x,y coordinates at the start of the line

		   for ( int x = 0; x < 720; )
		       { SETRGB16_RAW(x,y,  {p.Red()=d; p.Green()=d; p.Blue()=(d+EXTRABLUE); ++p; p.Red()=d; p.Green()=d; p.Blue()=(d+EXTRABLUE); ++p;} , 
		                            {++p;++p;} );}  

		   p = rowStart; p.OffsetY(data, 1);              // restore the x,y coords from start of line, then increment y to do y++;
	   }

	#else

	  // Since we're working the slower way - with images, we need to rebuild the bitmap from the image.
	  // to do this, we discard the old bitmap and recreate it from the display_image.  This is of course
	  // slower, which is why we recommend the use of USE_RAW_BITMAP_ACCESS, but USE_RAW_BITMAP_ACCESS
	  // might not work everywhere, so we give the option.
//	  int depth = my_lisabitmap->GetDepth();
//	  int width = my_lisabitmap->GetWidth();
//	  int height= my_lisabitmap->GetHeight();
      
	  if (!display_image)   display_image= new wxImage(my_lisabitmap->ConvertToImage());
      
	  for ( int yo = 0 , yi=0; yi < effective_lisa_vid_size_y; yo++,yi++ )
	      {
	  	   
	  	   for ( int xo = 0, xi=0; xi < 720;)
	             { SETRGB16_RAW(xi,yi,{display_image->SetRGB(xo,yo,d,d,d+EXTRABLUE); xo++; display_image->SetRGB(xo,yo,d,d,d+EXTRABLUE); xo++;},
	                                  {xo++;}  );   }
	      }
      
	  delete my_lisabitmap;
	  my_lisabitmap=new wxBitmap(*display_image);
	  my_memDC->SelectObjectAsSource(*my_lisabitmap);
    
	#endif
	/////////////////////////////////////////////////////////////////////////

    e_dirty_x_min=dirty_x_min;                       // need to do these here so we can update just the rectangle we need.
    e_dirty_x_max=dirty_x_max;                       // it will be repeated again below, but so what, it's only 4 assignments
    e_dirty_y_min=dirty_y_min; //screen_y_map[dirty_y_min];         // and two lookups.
    e_dirty_y_max=dirty_y_max; //screen_y_map[dirty_y_max];

	  if (updated)
	  {
	      memcpy(dirtyvidram,&lisaram[videolatchaddress],32768);
	      repaintall |= REPAINT_INVALID_WINDOW | REPAINT_VIDEO_TO_SKIN;
		  updated=0;
	  }

}



///////////////// SETRGB 16 bit AntiAliased MACRO //////////////////////////////////////////////////////   
//                                                                                                    //
//  macro to fill in r,g,b values, does 16 pixels at a time, must be a macro because of the Z param   //
//  and we want to get as much speed out of it as possible.                                           //
//                                                                                                    //
//  This would have been a 32 pixel macro if it were possible, however, since a single row is on the  //
//  Lisa's display is either 90 bytes or 76 bytes, we can only evenly divide by 2 bytes (16 bits.)    //
//                                                                                                    //
//                                                                                                    //
//  x,y are x,y coordinates on the bitmap.   They map to the Lisa's display, any other translation    //
//  must be handled in Z, or via other variables.   x gets incremented for each pixel handled.        //
//  x,y must be simple variables, and should not be expressions!                                      //
//                                                                                                    //
//  Z is a chunk of code to set r,g,b provided by the caller.  Z gets executed 16 times, once per     //
//  pixel.  This is needed because we might be using SetRGB on images or rawbitmap accesss, or some   //
//  other method in the future.   Z should be protected by enclosing it in curly braces when passing. //
//  Z may use the uint8 d to actually set the darkness level for a pixel.                             //
//                                                                                                    //
//  Y is identical to Z except that it's code to call when there's no update.  i.e. ++p               //
//                                                                                                    //
//                                                                                                    //
//  The following variables should be declared before calling this macro:                             //
//                                                                                                    //
//      int updated=0;	            // number of times we've made updates.                            //
//                                  // can be used as a threshhold to decide how to redraw the image. //
//      uint32 a1,a2,a3,a4,a5,xx;   // address above, below, for this value, horziontal byte offset   //
//      uint16 vup,vdn,val,vvu,vvd; // value above, below, this words read from addr @ a1,a2,a3       //
//      uint16 high,medium,low;     // used for antialiasing. how many neighboring bits               //
//      uint8 d;                    // darkness level to pass to Z                                    //
//                                                                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////

#define SETRGB16_AAG(x,y,Z,Y) {                                                                        \
	                                                                                                   \
	 xx=(x)>>3;                                                /*   turn x coord into byte offset */   \
	                                                                                                   \
	 a1=(yoffset[screen_to_mouse[ y>0?(y-1):0 ]]+xx) &32767;   /*   get the addresses for above   */   \
	 a2=(yoffset[screen_to_mouse[ y+1         ]]+xx) &32767;   /*   below and                     */   \
	 a3=(yoffset[screen_to_mouse[ y           ]]+xx) &32767;   /*   this value we're processing   */   \
	                                                                                                   \
	 vup=(lisaram[videolatchaddress+a1]<<8 )|lisaram[videolatchaddress+a1+1];  /*   word above    */   \
	 vdn=(lisaram[videolatchaddress+a2]<<8 )|lisaram[videolatchaddress+a2+1];  /*   word below    */   \
	 val=(lisaram[videolatchaddress+a3]<<8 )|lisaram[videolatchaddress+a3+1];  /*   this word     */   \
                                                                                                       \
                                                                                                       \
     if (videoramdirty>DIRECT_BLITS_THRESHHOLD ||        /*  If full update requested or          */   \
         (dirtyvidram[a1]<<8)|dirtyvidram[a1+1]!=vup ||  /*  if the video ram is dirty above or   */   \
         (dirtyvidram[a2]<<8)|dirtyvidram[a2+1]!=vdn ||  /*  below or on this value.              */   \
         (dirtyvidram[a3]<<8)|dirtyvidram[a3+1]!=val  )                                                \
      {                                   	                                                           \
       updated++;     	                                 /*  Keep track of update count           */   \
                                                                                                       \
	   dirty_x_min=MIN(x,dirty_x_min);       dirty_x_max=MAX(x+16,dirty_x_max);                        \
	   dirty_y_min=MIN(y,dirty_y_min);  	 dirty_y_max=MAX(y,dirty_y_max);                           \
                                                                                                       \
 	   a4=(a3-90)>0     ? (a3-90):a3;                                    /*   unscaled addr above */   \
	   a5=(a3+90)<32767 ? (a3+90):a3;                                    /*   unscaled addr below */   \
	   vvu=(lisaram[videolatchaddress+a4]<<8 )|lisaram[videolatchaddress+a4+1]; /* unscaled up    */   \
	   vvd=(lisaram[videolatchaddress+a5]<<8 )|lisaram[videolatchaddress+a5+1]; /* unscaled below */   \
       getgraymap(vvu,val,vvd,replacegray);              /*  Get the gray replacement map         */   \
	                                                                                                   \
	   high   = vup      & val     & vdn;                /*  above, middle, and below are black   */   \
	   medium = (vup&val)|(val&vdn)|(vdn&vup);           /*  two out of 3 are black               */   \
	   low    = vup      | val     | vdn;                /*  lowest - single pixel is black       */   \
	                                                                                                   \
	  d=bright[ ((BIT15 & val ) ? 7:0)  | replacegray[ 0]]; Z; x++;                                    \
	  d=bright[ ((BIT14 & val ) ? 7:0)  | replacegray[ 1]]; Z; x++;                                    \
	  d=bright[ ((BIT13 & val ) ? 7:0)  | replacegray[ 2]]; Z; x++;                                    \
	  d=bright[ ((BIT12 & val ) ? 7:0)  | replacegray[ 3]]; Z; x++;                                    \
	  d=bright[ ((BIT11 & val ) ? 7:0)  | replacegray[ 4]]; Z; x++;                                    \
	  d=bright[ ((BIT10 & val ) ? 7:0)  | replacegray[ 5]]; Z; x++;                                    \
	  d=bright[ ((BIT9  & val ) ? 7:0)  | replacegray[ 6]]; Z; x++;                                    \
	  d=bright[ ((BIT8  & val ) ? 7:0)  | replacegray[ 7]]; Z; x++;                                    \
	  d=bright[ ((BIT7  & val ) ? 7:0)  | replacegray[ 8]]; Z; x++;                                    \
	  d=bright[ ((BIT6  & val ) ? 7:0)  | replacegray[ 9]]; Z; x++;                                    \
	  d=bright[ ((BIT5  & val ) ? 7:0)  | replacegray[10]]; Z; x++;                                    \
	  d=bright[ ((BIT4  & val ) ? 7:0)  | replacegray[11]]; Z; x++;                                    \
	  d=bright[ ((BIT3  & val ) ? 7:0)  | replacegray[12]]; Z; x++;                                    \
	  d=bright[ ((BIT2  & val ) ? 7:0)  | replacegray[13]]; Z; x++;                                    \
	  d=bright[ ((BIT1  & val ) ? 7:0)  | replacegray[14]]; Z; x++;                                    \
	  d=bright[ ((BIT0  & val ) ? 7:0)  | replacegray[15]]; Z; x++;                                    \
	                                                                                                   \
	                                                                                                   \
      } 	                                                                                           \
     else                                                                                              \
      {                                                                      /* No updated needed */   \
	    Y; Y; Y; Y;                                                          /* so skip over 16X  */   \
	    Y; Y; Y; Y;                                                                                    \
	    Y; Y; Y; Y;                                                                                    \
	    Y; Y; Y; Y; x+=16;                                                                             \
	  }                                                                                                \
}                                                                                                     //
////////////////// GETRGB MACRO ENDS ///////////////////////////////////////////////////////////////////

/***************************************************************************************************\
   graymap[] are set to 8 which is the magical OR value into the color contrast table that forces
   gray replacement.  See ContrastChange() for details.  The graymap is stored as a 3 dimentional
   array, but we can make use of shifts and OR's to avoid multiplication that [][][] would be
   involved in dereferencing.  It's possible some compilers will optimize it properly, but why
   take a chance?  Access is equivalent to   color | graymap[(vup<<4) | (val<<2) | vdn];

   Since we work 2 bits at a time, we need to replace both bits with a gray together.  This is why 
   we have retval[1]=retval[0] and so on.   It would be possible to modify the code to do something
   like retval & 14 to avoid the copy, but it wouldn't be any faster and would make the code more
   complicated.
 
   This function is only called once per every 16 horizontal pixels processed for speed which is
   why it needs to store its results in an array.

 \***************************************************************************************************/

static inline void getgraymap(uint16 up, uint16 val, uint16 dn,  uint8 *retval)
{
	static uint8 graymap[4*4*4]={ 0,0,0,0,0,0,8,0,0,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,8,0,8,0,8,0,0,    
	                              0,0,0,0,8,0,8,8,0,0,0,0,0,0,8,0,0,0,0,0,0,0,8,8,0,8,0,8,0,0,0,0, }; 

// static int retval[16];
                                                                                                                                 
 retval[ 0]=retval[ 1]=graymap[(((BIT15+BIT14) & up)>>10 )|(((BIT15+BIT14) & val)>>12 ) | (((BIT15+BIT14) & dn)>>14 )]; 
 retval[ 2]=retval[ 3]=graymap[(((BIT13+BIT12) & up)>> 8 )|(((BIT13+BIT12) & val)>>10 ) | (((BIT13+BIT12) & dn)>>12 )]; 
 retval[ 4]=retval[ 5]=graymap[(((BIT11+BIT10) & up)>> 6 )|(((BIT11+BIT10) & val)>> 8 ) | (((BIT11+BIT10) & dn)>>10 )]; 
 retval[ 6]=retval[ 7]=graymap[(((BIT9 +BIT8 ) & up)>> 4 )|(((BIT9 +BIT8 ) & val)>> 6 ) | (((BIT9 +BIT8 ) & dn)>> 8 )]; 
 retval[ 8]=retval[ 9]=graymap[(((BIT7 +BIT6 ) & up)>> 2 )|(((BIT7 +BIT6 ) & val)>> 4 ) | (((BIT7 +BIT6 ) & dn)>> 6 )]; 
 retval[10]=retval[11]=graymap[(((BIT5 +BIT4 ) & up)     )|(((BIT5 +BIT4 ) & val)>> 2 ) | (((BIT5 +BIT4 ) & dn)>> 4 )]; 
 retval[12]=retval[13]=graymap[(((BIT3 +BIT2 ) & up)<< 2 )|(((BIT3 +BIT2 ) & val)     ) | (((BIT3 +BIT2 ) & dn)>> 2 )]; 
 retval[14]=retval[15]=graymap[(((BIT1 +BIT0 ) & up)<< 4 )|(((BIT1 +BIT0 ) & val)<< 2 ) | (((BIT1 +BIT0 ) & dn)     )]; 

}


void LisaWin::RePaint_AAGray(void)
{
   // vars for SETRGB16  
   int updated=0;	
   uint32 a1,a2,a3,a4,a5,xx;
   uint16 vup,vdn,val,vvu,vvd;
   uint16 high,medium,low;
   uint8  d;

   uint8 replacegray[16]; // ignore dumb compiler warning here!
   dirty_x_min=720; dirty_x_max=-1; dirty_y_min=384*3; dirty_y_max=-1;

#ifdef USE_RAW_BITMAP_ACCESS

 int depth;  
 int width;  
 int height; 
 int ox,oy;             // private to this - not member vars

   
if (skins_on)
   {
    if (!my_skin) { replacegray[0]=0;  // useless statement to suppress dumb "unused" compiler warning 
	                return;}

    depth = my_skin->GetDepth();    width = my_skin->GetWidth();    height= my_skin->GetHeight();
    ox=screen_origin_x;    oy=screen_origin_y;
   }
else
   {
    if (!my_lisabitmap) return;

    depth = my_lisabitmap->GetDepth();    width = my_lisabitmap->GetWidth();    height= my_lisabitmap->GetHeight();
    ox=0; oy=0;
   }

   typedef wxPixelData<wxBitmap,wxNativePixelFormat> PixelData;
   PixelData data(skins_on ? *my_skin:*my_lisabitmap);
   if (!data) return;
   PixelData::Iterator p(data);
   p.Reset(data);
   p.MoveTo(data,ox,oy); 
   for ( int y = 0; y < effective_lisa_vid_size_y; ++y )   // effective_lisa_vid_size_y
   {
       PixelData::Iterator rowStart = p;              // save the x,y coordinates at the start of the line

	   for ( int x = 0; x < effective_lisa_vid_size_x; )
	       { SETRGB16_AAG(x,y,  {p.Red()=d; p.Green()=d; p.Blue()=(d+EXTRABLUE); ++p;} , {++p;} );}  

	   p.Red()=0; p.Green()=0; p.Blue()=0;


       p = rowStart; p.OffsetY(data, 1);              // restore the x,y coords from start of line, then increment y to do y++;
   }

#else

  // Since we're working the slower way - with images, we need to rebuild the bitmap from the image.
  // to do this, we discard the old bitmap and recreate it from the display_image.  This is of course
  // slower, which is why we recommend the use of USE_RAW_BITMAP_ACCESS, but USE_RAW_BITMAP_ACCESS
  // might not work everywhere, so we give the option.
  if (skins_on)
  {
    if (!my_skin || !my_lisabitmap) return; 

  //  int depth = my_skin->GetDepth();
  //  int width = my_skin->GetWidth();
  //  int height= my_skin->GetHeight();

 	if (!display_image)   
         display_image= new wxImage(  my_lisabitmap->ConvertToImage());

	for ( int y=0; y < effective_lisa_vid_size_y; y++ )
	    {
		    for ( int x=0; x < effective_lisa_vid_size_x;)
                { SETRGB16_AAG(x,y,{display_image->SetRGB(x,y,d,d,d+EXTRABLUE); },{;}); }
	    }

    delete my_lisabitmap;
	my_lisabitmap=new wxBitmap(*display_image);
	my_memDC->SelectObjectAsSource(*my_lisabitmap);

    e_dirty_x_min=dirty_x_min;                       // need to do these here so we can update just the rectangle we need.
    e_dirty_x_max=dirty_x_max;                       // it will be repeated again below, but so what, it's only 4 assignments
    e_dirty_y_min=dirty_y_min; //screen_y_map[dirty_y_min];         // and two lookups.
    e_dirty_y_max=dirty_y_max; //screen_y_map[dirty_y_max];

    if (updated)
       my_skinDC->Blit(screen_origin_x + e_dirty_x_min, screen_origin_y+e_dirty_y_min,     // target x,y
                       e_dirty_x_max-e_dirty_x_min+1,   e_dirty_y_max-e_dirty_y_min+1,     // size w,h
                       my_memDC, 0,0, wxCOPY, false);

    // we don't delete display_image since we can reuse it the next time around and save time
  }
  else  // skins are off
  {
//    int depth = my_lisabitmap->GetDepth();
//    int width = my_lisabitmap->GetWidth();
//    int height= my_lisabitmap->GetHeight();

    if (!display_image)   display_image= new wxImage(my_lisabitmap->ConvertToImage());

	for ( int yo = 0 , yi=0; yi < effective_lisa_vid_size_y; yo++,yi++ )
	    {
		   // note neither xi, nor xo are incremented as part of the for-loop header, this is because
		   // the SETRGB16_AA macro expands to 16 iterations, and it increments xi on each iteration.
		   // however, it doesn't do anything with xo, so xo++ is passed as a parameter.
		   for ( int xo = 0, xi=0; xi < effective_lisa_vid_size_x;)
               { SETRGB16_AAG(xi,yi,{display_image->SetRGB(xo,yo,d,d,d+EXTRABLUE); xo++;},{xo++;});   }
	    }

	delete my_lisabitmap;
	my_lisabitmap=new wxBitmap(*display_image);
	my_memDC->SelectObjectAsSource(*my_lisabitmap);
  }
#endif
/////////////////////////////////////////////////////////////////////////

  e_dirty_x_min=dirty_x_min;                       // need to do these here so we can update just the rectangle we need.
  e_dirty_x_max=dirty_x_max;                       // it will be repeated again below, but so what, it's only 4 assignments
  e_dirty_y_min=dirty_y_min; //screen_y_map[dirty_y_min];         // and two lookups.
  e_dirty_y_max=dirty_y_max; //screen_y_map[dirty_y_max];

  if (updated)
  {
      memcpy(dirtyvidram,&lisaram[videolatchaddress],32768);
      repaintall |= REPAINT_INVALID_WINDOW | REPAINT_VIDEO_TO_SKIN;
	  updated=0;
  }


}


void LisaWin::OnPaint(wxPaintEvent &event )
{
  if (!my_lisaframe) return; // not built yet!
  if (!my_lisawin)   return;

  if (!my_lisaframe->running)   repaintall |= REPAINT_INVALID_WINDOW;
  if ( my_alias==NULL || refresh_bytemap) ContrastChange();

  if ( my_memDC==NULL)       my_memDC     =new class wxMemoryDC;
  if ( my_lisabitmap==NULL)
  {
       my_lisabitmap=new class wxBitmap(effective_lisa_vid_size_x,effective_lisa_vid_size_y, DEPTH); //,my_lisaframe->depth);
       my_memDC->SelectObjectAsSource(*my_lisabitmap);
	   my_memDC->SetBrush(FILLERBRUSH);      my_memDC->SetPen(FILLERPEN);
	   my_memDC->DrawRectangle(0 ,   0,   effective_lisa_vid_size_x, effective_lisa_vid_size_y);
  	   if (!my_memDC->IsOk()) ALERT_LOG(0,"my_memDC is not ok.");
  	   if (!my_lisabitmap->IsOk()) ALERT_LOG(0,"my_lisabitmap is not ok.");
  }

  if ((dirtyscreen || videoramdirty) && (powerstate & POWER_ON_MASK) == POWER_ON)
                                        (my_lisawin->*RePainter)();  // whoever came up with this C++ syntax instead of the old C one was on crack!

  // from wx2.8 help
  wxRegionIterator upd(GetUpdateRegion()); // get the update rect list

  while (upd)
  {

    wxRect rect(upd.GetRect());
    wxRect display(screen_origin_x,screen_origin_y, effective_lisa_vid_size_x,effective_lisa_vid_size_y);

    // if this was an actual request to update the screen, then flag it as such.
    if (rect.Intersects(display))         repaintall |= REPAINT_INVALID_WINDOW | REPAINT_VIDEO_TO_SKIN;

    if ( skins_on) OnPaint_skins(   rect);
    else           OnPaint_skinless(rect);

    upd++;
  }


  dirtyscreen=0;
  videoramdirty=0;
 #ifdef __WXOSX__
  event.Skip(false);
 #endif
}

void LisaWin::OnPaint_skinless(wxRect &rect)
{
   wxPaintDC dc(this);
   DoPrepareDC(dc);
#ifdef __WXMACOSX__
   dc.SetBackgroundMode(wxTRANSPARENT); // OS X has a nasty habbit of erasing the background even though we skip the event!
#endif

   int width, height, w_width, w_height;  
   width=rect.GetWidth();              // junk assignment to suppress " warning: unused parameter 'rect'"
   height=rect.GetHeight();

   GetClientSize(&w_width,&w_height);

   correct_my_lisabitmap();  // hack/fix for linux

   ox=(w_width  - effective_lisa_vid_size_x)/2;
   oy=(w_height - effective_lisa_vid_size_y)/2;
   screen_origin_x=ox;  screen_origin_y=oy;

   if (my_lisaframe->running)
   {
       ex=w_width-ox; ey=w_height-oy;
 
       // draw a black border around the lisa display
       dc.SetBrush(*wxBLACK_BRUSH);      dc.SetPen(*wxBLACK_PEN);
       if (oy>1)         dc.DrawRectangle(0 ,   0,   w_width,oy    );   // top
       if (ey<w_height)  dc.DrawRectangle(0 ,  ey-2, w_width,w_height); // bottom
       if (ey>1)         dc.DrawRectangle(0 ,  oy,   ox,     ey    );   // center-left
       if (ex-1<w_width) dc.DrawRectangle(ex-1,oy,   w_width,ey    );   // center-right

       if (e_dirty_x_min>e_dirty_x_max || e_dirty_y_min>e_dirty_y_max)
          {
           e_dirty_x_min=0; e_dirty_x_max=lisa_vid_size_x;
           e_dirty_y_min=0; e_dirty_y_max=effective_lisa_vid_size_y;
          }


 	   dc.Blit(ox+e_dirty_x_min,                 oy+e_dirty_y_min,                                 // target x,y on window
			      e_dirty_x_max-e_dirty_x_min,      e_dirty_y_max-e_dirty_y_min,     my_memDC,     // width, height, source DC
				  e_dirty_x_min,                    e_dirty_y_min);                                // source x,y
	
       e_dirty_x_min=720; e_dirty_x_max=0; e_dirty_y_min=364*3; e_dirty_y_max=-1;                  // reset coordinates for next pass
   }
   else    // not running
   {
       dc.SetBrush(*wxBLACK_BRUSH);  dc.SetPen(*wxBLACK_PEN);
       dc.DrawRectangle(0 ,0,   width,height);
   }
   
   repaintall=0;
}


void LisaWin::OnPaint_skins(wxRect &rect)
{

   wxPaintDC dc(this);
   DoPrepareDC(dc);
#ifdef __WXMACOSX__
   dc.SetBackgroundMode(wxTRANSPARENT); // OS X has a nasty habbit of erasing the background even though we skip the event!
#endif
   

   correct_my_skin();

   // The first time we're in here, draw the skins.  The skins are cut in quarters because
   // some versions of wxWidgets can't handle such a large wxBitmap. :-(  Once we build the skin
   // we discard the images and set them to null, so this only runs once.

   if ((powerstate & POWER_NEEDS_REDRAW)==POWER_NEEDS_REDRAW && my_skin0)
   {
    int y=0;

    my_skinDC->Blit(0,y, my_skin0->GetWidth(),my_skin0->GetHeight(),my_skin0DC, 0,0 ,wxCOPY, false);   y+=my_skin0->GetHeight();
    my_skinDC->Blit(0,y, my_skin1->GetWidth(),my_skin1->GetHeight(),my_skin1DC, 0,0 ,wxCOPY, false);   y+=my_skin1->GetHeight();
    my_skinDC->Blit(0,y, my_skin2->GetWidth(),my_skin2->GetHeight(),my_skin2DC, 0,0 ,wxCOPY, false);   y+=my_skin2->GetHeight();
    my_skinDC->Blit(0,y, my_skin3->GetWidth(),my_skin3->GetHeight(),my_skin3DC, 0,0 ,wxCOPY, false); //y+=my_skin3->GetHeight();


    delete my_skin0;           my_skin0=NULL;
    delete my_skin1;           my_skin1=NULL;
    delete my_skin2;           my_skin2=NULL;
    delete my_skin3;           my_skin3=NULL;

    delete my_skin0DC;         my_skin0DC=NULL;
    delete my_skin1DC;         my_skin1DC=NULL;
    delete my_skin2DC;         my_skin2DC=NULL;
    delete my_skin3DC;         my_skin3DC=NULL;
   }

   if ((floppystate & FLOPPY_NEEDS_REDRAW) )
   {
     // The floppy's going to be redrawn so we to blit the floppy to the
     // skin, and then the window will need to be updated from the skin.
     repaintall |= REPAINT_INVALID_WINDOW | REPAINT_FLOPPY_TO_SKIN;


     switch(floppystate & FLOPPY_ANIM_MASK)
     {

        case 0:  my_skinDC->Blit(FLOPPY_LEFT, FLOPPY_TOP, my_floppy0->GetWidth(),my_floppy0->GetHeight(),my_floppy0DC,    0,0 ,wxCOPY, false); break;
        case 1:  my_skinDC->Blit(FLOPPY_LEFT, FLOPPY_TOP, my_floppy1->GetWidth(),my_floppy1->GetHeight(),my_floppy1DC,    0,0 ,wxCOPY, false); break;
        case 2:  my_skinDC->Blit(FLOPPY_LEFT, FLOPPY_TOP, my_floppy2->GetWidth(),my_floppy2->GetHeight(),my_floppy2DC,    0,0 ,wxCOPY, false); break;
        case 3:  my_skinDC->Blit(FLOPPY_LEFT, FLOPPY_TOP, my_floppy3->GetWidth(),my_floppy3->GetHeight(),my_floppy3DC,    0,0 ,wxCOPY, false); break;
       default:  my_skinDC->Blit(FLOPPY_LEFT, FLOPPY_TOP, my_floppy4->GetWidth(),my_floppy4->GetHeight(),my_floppy4DC,    0,0 ,wxCOPY, false);

     }

     floppystate &= FLOPPY_ANIMATING | FLOPPY_ANIM_MASK; //XXX Is this right?
    }




    if ((powerstate & POWER_NEEDS_REDRAW) )
    {
     // The power's going to be redrawn so we to blit the power to the
     // skin, and then the window will need to be updated from the skin.
     repaintall |= REPAINT_INVALID_WINDOW | REPAINT_POWER_TO_SKIN;

     if ((powerstate & POWER_ON_MASK) == POWER_ON)
        {
          my_skinDC->Blit(POWER_LEFT, POWER_TOP,  my_poweron->GetWidth(), my_poweron->GetHeight(), my_poweronDC, 0,0, wxCOPY, false);

          // These coordinates are a small rectangle around the powerlight
          if (powerstate & POWER_PUSHED)
             my_skinDC->Blit(1143-2,836-2, (1185-1143)+2  , (876-836)+2, my_poweronDC,  1143,(836-738), wxCOPY, false);
        }
     else
        {
          my_skinDC->Blit(POWER_LEFT, POWER_TOP,  my_poweroff->GetWidth(), my_poweroff->GetHeight(), my_poweroffDC, 0,0, wxCOPY, false);

          if (powerstate & POWER_PUSHED)
              my_skinDC->Blit(1143-2,836-2, (1185-1143)+2  , (876-836)+2, my_poweroffDC,  1143,(836-738), wxCOPY, false);
        }

     powerstate &= ~POWER_PUSHED;
    }


     ///////////////////////////////////////////////////////////////////
     int vbX,vbY;
     int ivbX,ivbY;
     int width,height;

     GetViewStart(&vbX,&vbY);           // convert scrollbar position into skin relative pixels

     ivbX=vbX; ivbY=vbY;
     vbX=vbX * (my_skin->GetWidth()/100);
     vbY=vbY * (my_skin->GetHeight()/100);

	 vbX  +=rect.GetX()-1;
	 vbY  +=rect.GetY()-1;
	 height=rect.GetHeight()+1;
	 width =rect.GetWidth()+1;
	
     width= MIN(width,my_skin->GetWidth()  );
     height=MIN(height,my_skin->GetHeight());
	 vbX=MAX(vbX,0); vbY=MAX(vbY,0);


	 dc.Blit(vbX,vbY, width, height, my_skinDC, vbX,vbY, wxCOPY, false);

     repaintall=0;
}

void LisaWin::OnErase(wxEraseEvent &event)
{
#ifdef __WXOSX__
	event.GetDC()->SetBackground(*wxTRANSPARENT_BRUSH);
	event.GetDC()->SetBackgroundMode(wxTRANSPARENT);
	event.Skip();
	event.StopPropagation();
#endif
}


void black(void)
{
  if (skins_on && my_skin!=NULL)
     {
	   if (my_skinDC)
       {       
        my_skinDC->SetBrush(FILLERBRUSH);   // these must be white, else linux drawing breaks
        my_skinDC->SetPen(FILLERPEN);       // these must be white, else linux drawing breaks
        my_skinDC->DrawRectangle(screen_origin_x,screen_origin_y, effective_lisa_vid_size_x,effective_lisa_vid_size_y);
       }
      }
  else
     {
	   if (my_memDC)
	   {
         my_memDC->SetBrush(FILLERBRUSH);   // these must be white, else linux drawing breaks
         my_memDC->SetPen(FILLERPEN);       // these must be white, else linux drawing breaks
         my_memDC->DrawRectangle(0,0,effective_lisa_vid_size_x+1,effective_lisa_vid_size_y);
       }
     }

my_lisawin->Refresh();
my_lisawin->Update();
}


extern "C" void setstatusbar(char *text)
{
	wxString x=wxString(text, wxConvLocal, 2048); //wxSTRING_MAXLEN);
    my_lisaframe->SetStatusBarText(x);
}

extern "C" int islisarunning(void)
{return     my_lisaframe->running;}


void lightning(void)
{

 if (!skins_on)
    {
        contrast=0xf;
        //contrastchange();
        black();
        ALERT_LOG(0,"Lightning skipped - skins off.");
        return;
    }
 static XTIMER lastclk;

 if (lastclk==cpu68k_clocks) {
                              ALERT_LOG(0,"skipping duplicate");
                              return; // got a duplicate due to screen refresh
                             }
 lastclk=cpu68k_clocks;

 // CAN YOU FEEL IT BUILDING
 
 // CAN YOU FEEL IT BUILDING
 black();
 // DEVASTATION IS ON THE WAY
 
 wxMilliSleep(300);
 my_poweroffclk->Play(wxSOUND_ASYNC);

 /* Draw the central flash */
 my_skinDC->SetPen(FILLERPEN);
 my_skinDC->SetBrush(FILLERBRUSH);
 my_skinDC->DrawRectangle(screen_origin_x + effective_lisa_vid_size_x/2 -24, screen_origin_y,
                          48, effective_lisa_vid_size_y);

 videoramdirty=0; my_lisawin->RefreshRect(wxRect(screen_origin_x + effective_lisa_vid_size_x/2 -24, screen_origin_y,
                          48, effective_lisa_vid_size_y),false);

 my_lisawin->Refresh(); my_lisawin->Update();
 wxMilliSleep(100);

 /* Blacken the screen */
 my_skinDC->SetPen(*wxBLACK_PEN);
 my_skinDC->SetBrush(*wxBLACK_BRUSH);


 for (int i=12; i>-1; i-=4)
 {
   // left

   my_skinDC->SetPen(*wxBLACK_PEN);
   my_skinDC->SetBrush(*wxBLACK_BRUSH);

   my_skinDC->DrawRectangle(screen_origin_x, screen_origin_y,
                            effective_lisa_vid_size_x/2 -i, effective_lisa_vid_size_y);

   // right
   my_skinDC->DrawRectangle(screen_origin_x + effective_lisa_vid_size_x/2 + i, screen_origin_y,
                            effective_lisa_vid_size_x/2 -i+1,
                            effective_lisa_vid_size_y);

   videoramdirty=0;
   my_lisawin->RefreshRect(wxRect(screen_origin_x, screen_origin_y,
                                  effective_lisa_vid_size_x,effective_lisa_vid_size_y),
                                  false);
   my_lisawin->Refresh(); my_lisawin->Update();
   wxMilliSleep(75);
 }


     my_lisawin->powerstate = POWER_NEEDS_REDRAW;
     my_lisawin->powerstate = POWER_NEEDS_REDRAW;     my_lisawin->dirtyscreen=1;     my_lisawin->repaintall|=1;
     my_lisawin->powerstate = POWER_OFF;
    contrast=0xf;
    contrastchange();

    my_lisawin->Refresh(); my_lisawin->Update();
}

int initialize_all_subsystems(void);


void handle_powerbutton(void)
{

//   ALERT_LOG(0,"Power Button Pressed.");


              if ((my_lisawin->powerstate & POWER_ON_MASK) == POWER_ON) // power is already on, send a power-key event instead
              {
               int i;

               setstatusbar("Sending power key event");
               presspowerswitch();                      // send keyboard event.

               if (running_lisa_os==LISA_OFFICE_RUNNING)
                  {
                    refresh_rate_used=5*REFRESHRATE;       // speed up contrast ramp by lowering the refresh rate
                    my_lisaframe->clockfactor=0;           // speed up shutdown to compensane for printing slowdown
                  }
               my_lisaconfig->Save(pConfig, floppy_ram);

               setstatusbar("Flushing any queued print jobs.");
               for (i=0; i<9; i++) iw_formfeed(i);
               //setstatusbar("Shutting down printers");
               //iw_shutdown();                             // this is too slow, so skip it
               setstatusbar("Waiting for Lisa to shut down...");
              }
              else
              {
				  int ret;
				
				  // 20071204 hack to fix windows bugs with RAWBITMAP
				  #ifdef USE_RAW_BITMAP_ACCESS
        
				    #ifdef __WXMSW__
					if (skins_on) {turn_skins_off(); turn_skins_on();}
					black();
				    #endif  
        
				  #endif
	
                  ret=initialize_all_subsystems();
                  if (!ret)
                  {
                        my_lisawin->powerstate|= POWER_NEEDS_REDRAW | POWER_ON;
                        my_lisaframe->running=emulation_running; 
                        my_lisaframe->runtime.Start(0);
						my_lisaframe->lastcrtrefresh=0;
                  }
                  else 
                       if (!romless) wxMessageBox(_T("Power on failed."), _T("Poweron Failed"), wxICON_INFORMATION | wxOK);
                  if (ret>1) EXIT(999,0,"Out of memory or other fatal error.");  // out of memory or other fatal error!
                  contrast=0;

                  presspowerswitch();                      // send keyboard event.

                  my_lisawin->dirtyscreen=1;
              }

        my_lisawin->dirtyscreen=1;

        save_global_prefs();
        my_lisaframe->UpdateProfileMenu();
}


extern "C" void lisa_powered_off(void)
{
   my_lisaframe->running=emulation_off;                // no longer running
   if ((my_lisawin->floppystate & FLOPPY_ANIM_MASK)!=FLOPPY_EMPTY) 
   {
	 eject_floppy_animation();
     my_lisaframe->Refresh(false,NULL);                // clean up display
     my_lisaframe->Update();
     my_lisaframe->Refresh(false,NULL);                // clean up display
     my_lisaframe->Update();
     my_lisaframe->Refresh(false,NULL);                // clean up display
     my_lisaframe->Update();
   }
   
   if (skins_on) lightning();					     // CRT lightning
   else black();

   unvars();					                     // reset global vars
   my_lisaconfig->Save(pConfig, floppy_ram);         // save PRAM, configuration
   fileMenu->Check(ID_RUN,false);		             // emulator no longer running
   my_lisaframe->runtime.Pause();                    // pause the stopwatch

   my_lisawin->powerstate= POWER_NEEDS_REDRAW | POWER_OFF;
   my_lisawin->Update();

   setstatusbar("The Lisa has powered off");         // status
   my_lisaframe->Refresh(false,NULL);                // clean up display
   my_lisaframe->Update();

   iw_enddocuments();
}

extern "C" void lisa_rebooted(void)
{
   setstatusbar("The Lisa is rebooting.");
   my_lisaconfig->Save(pConfig, floppy_ram);         // save PRAM, configuration
   unvars();                                         // reset global vars and all
   my_lisaframe->running=emulation_off;              // prevent init_all_subs from barking
   my_lisawin->powerstate = POWER_OFF;               // will be turned back on immediately
   handle_powerbutton();                             // back on we go.
}



void LisaWin::OnMouseMove(wxMouseEvent &event)
{

    //PrepareDC(dc);
    wxString str;
    int vbX,vbY;                     // scrollbar location

	mousemoved++;

     // windows bug - crashes because it gets a mouse event before the window is ready.

     wxPoint pos = event.GetPosition();

     if (skins_on && my_skin==NULL) return;
     if (my_lisabitmap==NULL) return;

     GetViewStart(&vbX,&vbY);
    // convert scrollbar position into skin relative pixels
     if (skins_on && my_skin!=NULL)
        {
         vbX=vbX * (my_skin->GetWidth()/100);
         vbY=vbY * (my_skin->GetHeight()/100);
        }
     else
        {
         vbX=vbX * (my_lisabitmap->GetWidth()/100);
         vbY=vbY * (my_lisabitmap->GetHeight()/100);
        }

     pos.x+=vbX;
     pos.y+=vbY;

    long x= pos.x;
    long yy= pos.y;

    long y = doubley ? (yy>>1) : yy;
    long tempy;

    tempy=pos.y-screen_origin_y;
    if (tempy<0) tempy=0;
    if (tempy>500) tempy=499;


    y=screen_to_mouse[tempy];

    x=x-screen_origin_x;


    // power animation + button

	if (skins_on)
	{

      if (pos.x>1143 && pos.x<1185 && pos.y>836 && pos.y<876)
      {
       my_lisawin->powerstate &= POWER_ON;

        // unlike normal UI buttons, where the action happens when you let go of the mouse button,
        // this is as it should be, as the Lisa turns on when the power button is pushed in.
        if (event.LeftDown())    {  if (sound_effects_on) my_lisa_power_switch01->Play(wxSOUND_ASYNC);
                                    my_lisawin->powerstate |= POWER_NEEDS_REDRAW | POWER_PUSHED;
                                    handle_powerbutton();
								    my_lisaframe->Refresh(false,NULL);                // clean up display
								    my_lisaframe->Update();

                                 }

        if (event.LeftUp())      {
	                                 if (sound_effects_on) my_lisa_power_switch02->Play(wxSOUND_ASYNC);
	                                 my_lisawin->powerstate |= POWER_NEEDS_REDRAW;
								     my_lisaframe->Refresh(false,NULL);                // clean up display
								     my_lisaframe->Update();
	
                                 }


      }

      if    (my_floppy0!=NULL)
	        {
	         if	(pos.x>FLOPPY_LEFT && pos.x<FLOPPY_LEFT+my_floppy0->GetWidth() &&
    		     pos.y>FLOPPY_TOP  && pos.y<FLOPPY_TOP +my_floppy0->GetHeight()    )
	             {
	   	           if (event.LeftDown())   my_lisaframe->OnxFLOPPY();
  				   if (event.RightDown())  my_lisaframe->OnxNewFLOPPY();
	             }
			}
	} // end if skins on

    if ( (y>0 && y<lisa_vid_size_y) && (x>=0 && x<lisa_vid_size_x) && my_lisaframe->running)
            {
                static long lastup;
                long now;                              // tee hee, the long now.
                int lu=event.LeftUp();
                now=my_lisaframe->runtime.Time();
#if defined(__WXX11__) || defined(__WXGTK__) || defined(__WXOSX__)
                if (hide_host_mouse) SetCursor(wxCURSOR_BLANK);
                else                 SetCursor(*m_dot_cursor);
#else
                if (hide_host_mouse) SetCursor(wxCURSOR_BLANK);
                else                 SetCursor(wxCURSOR_CROSS);
#endif
                add_mouse_event(x,y, lu ? -1:  (event.LeftDown() ? 1:0) );
				seek_mouse_event();

                // double click hack  - fixme BUG BUG BUG - fixme - well timing bug, will not be fixed if 32Mhz is allowed
                if (lu)
                   {
                          if (now-lastup<1500)
                          {
                           add_mouse_event(x,y,  1);
                           add_mouse_event(x,y, -1);
                           add_mouse_event(x,y,  1);
                           add_mouse_event(x,y, -1);
                          }

                          lastup=now;
                   }


            }
    else
            my_lisawin->SetCursor(wxNullCursor);

    if (event.RightDown())   {videoramdirty=32768; my_lisaframe->Refresh(false,NULL);} // force screen refresh
}


void LisaEmFrame::OnRun(wxCommandEvent& WXUNUSED(event))
{
    if (my_lisaframe->running==emulation_off)    {handle_powerbutton(); return;                 }
    if (my_lisaframe->running==emulation_paused) {my_lisaframe->running=emulation_running;      }

    fileMenu->Check(ID_RUN,  true);
}


extern "C" void pause_run(void)
{
   if (my_lisaframe->running==emulation_off) return;

   if (my_lisaframe->running==emulation_running)
   {
     my_lisaframe->runtime.Pause();
     my_lisaframe->running=emulation_paused;
     return;
   }
}

extern "C" void resume_run(void)
{
   if (my_lisaframe->running==emulation_off) return;

   if (my_lisaframe->running==emulation_paused)
   {
     my_lisaframe->runtime.Resume();
     my_lisaframe->running=emulation_running;
     return;
   }
}


void LisaEmFrame::OnPause(wxCommandEvent& WXUNUSED(event))
{
    if (my_lisaframe->running==emulation_off)      {                                                return;}
    if (my_lisaframe->running==emulation_running)  {pause_run();   fileMenu->Check(ID_PAUSE,true);  return;}
    if (my_lisaframe->running==emulation_paused)   {resume_run();  fileMenu->Check(ID_PAUSE,false); return;}
}


void LisaEmFrame::OnAbout(wxCommandEvent& WXUNUSED(event))
{
    wxAboutDialogInfo info;
    info.SetName(_T("LisaEm"));

#ifdef VERSION
    info.SetVersion(_T(VERSION));
#else
    info.SetVersion(_T("1.x.x Unknown"));
#endif

	info.SetDescription(_T("The first fully functional Apple Lisa emulator."));
    info.SetCopyright(_T("\xa9 2007 Ray Arachelian"));
	info.SetWebSite(_T("http://lisaem.sunder.net"));
    info.AddDeveloper(_T("Ray Arachelian - Emulator"));
	info.AddDeveloper(_T("Brian Foley - OS X UI"));
	info.AddDeveloper(_T("James Ponder - 68K core.\n"));
#ifdef LICENSE
	info.SetLicense(_T(LICENSE));
#endif
	
#ifdef BUILTBY
	info.AddDeveloper(_T("\n\n" BUILTBY));
#endif

	wxAboutBox(info);
}
#ifdef __WXOSX__
void LisaEmFrame::OnQuit(wxCommandEvent& WXUNUSED(event))
#else
void LisaEmApp::OnQuit(wxCommandEvent& WXUNUSED(event))
#endif
{
    if (NULL!=my_lisabitmap          )             {delete my_lisabitmap          ;   my_lisabitmap=NULL;           }
    if (NULL!=my_memDC               )             {delete my_memDC               ;   my_memDC=NULL;                }

    if (NULL!=my_lisa_sound          )             {delete my_lisa_sound          ;   my_lisa_sound=NULL;           }

    if (NULL!=my_floppy_eject        )             {delete my_floppy_eject        ;   my_floppy_eject=NULL;         }
    if (NULL!=my_floppy_insert       )             {delete my_floppy_insert       ;   my_floppy_insert=NULL;        }
    if (NULL!=my_floppy_motor1       )             {delete my_floppy_motor1       ;   my_floppy_motor1=NULL;        }
    if (NULL!=my_floppy_motor2       )             {delete my_floppy_motor2       ;   my_floppy_motor2=NULL;        }
    if (NULL!=my_lisa_power_switch01 )             {delete my_lisa_power_switch01 ;   my_lisa_power_switch01=NULL;  }
    if (NULL!=my_lisa_power_switch02 )             {delete my_lisa_power_switch02 ;   my_lisa_power_switch02=NULL;  }
    if (NULL!=my_poweroffclk         )             {delete my_poweroffclk         ;   my_poweroffclk=NULL;          }
    if (NULL!=my_bytemap)                          {delete my_bytemap;     my_bytemap=NULL; }
    if (NULL!=my_alias  )                          {delete my_alias  ;     my_alias  =NULL; }

    if (NULL!=my_skin   )                          {delete my_skin   ;     my_skin   =NULL; }
    if (NULL!=my_skin0  )                          {delete my_skin0  ;     my_skin   =NULL; }
    if (NULL!=my_skin1  )                          {delete my_skin1  ;     my_skin   =NULL; }
    if (NULL!=my_skin2  )                          {delete my_skin2  ;     my_skin   =NULL; }
    if (NULL!=my_skin3  )                          {delete my_skin3  ;     my_skin   =NULL; }



    if (NULL!=my_floppy0)                          {delete my_floppy0;     my_floppy0=NULL; }
    if (NULL!=my_floppy1)                          {delete my_floppy1;     my_floppy1=NULL; }
    if (NULL!=my_floppy2)                          {delete my_floppy2;     my_floppy2=NULL; }
    if (NULL!=my_floppy3)                          {delete my_floppy3;     my_floppy3=NULL; }
    if (NULL!=my_floppy4)                          {delete my_floppy4;     my_floppy4=NULL; }

    if (NULL!=my_poweron   )                       {delete my_poweron   ;  my_poweron   =NULL; }
    if (NULL!=my_poweroff  )                       {delete my_poweroff  ;  my_poweroff  =NULL; }

    if (NULL!=my_bytemapDC )                       {delete my_bytemapDC ;  my_bytemapDC =NULL; }
    if (NULL!=my_aliasDC   )                       {delete my_aliasDC   ;  my_aliasDC   =NULL; }

    if (NULL!=my_skinDC    )                       {delete my_skinDC    ;  my_skinDC    =NULL; }
    if (NULL!=my_floppy0DC )                       {delete my_floppy0DC ;  my_floppy0DC =NULL; }
    if (NULL!=my_floppy1DC )                       {delete my_floppy1DC ;  my_floppy1DC =NULL; }
    if (NULL!=my_floppy2DC )                       {delete my_floppy2DC ;  my_floppy2DC =NULL; }
    if (NULL!=my_floppy3DC )                       {delete my_floppy3DC ;  my_floppy3DC =NULL; }
    if (NULL!=my_floppy4DC )                       {delete my_floppy4DC ;  my_floppy4DC =NULL; }

    if (NULL!=my_poweronDC )                       {delete my_poweronDC ;  my_poweronDC =NULL; }
    if (NULL!=my_poweroffDC)                       {delete my_poweroffDC;  my_poweroffDC=NULL; }

#ifdef __WXOSX__
     my_lisaframe->m_emulation_timer->Stop();  // stop the timer
  	 wxMilliSleep(emulation_time*2);	       // ensure that any pending timer events are allowed to finish
	 delete my_lisaframe->m_emulation_timer;   // delete the timer
     if (my_LisaConfigFrame) {delete my_LisaConfigFrame; my_LisaConfigFrame=NULL;}  // close any ConfigFrame
	 Close();                                  // and bye bye we go.
#else
     my_lisaframe->m_emulation_timer->Stop();  // stop the timer
     wxMilliSleep(750);
     delete my_lisaframe->m_emulation_timer;
 	 wxMilliSleep(250);
     delete my_lisaframe;
     wxExit();
#endif
}

void LisaEmFrame::OnLisaWeb(wxCommandEvent& WXUNUSED(event)) {::wxLaunchDefaultBrowser(_T("http://lisaem.sunder.net"));}
void LisaEmFrame::OnLisaFaq(wxCommandEvent& WXUNUSED(event)) {::wxLaunchDefaultBrowser(_T("http://lisafaq.sunder.net"));}




void LisaEmFrame::OnConfig(wxCommandEvent& WXUNUSED(event))
{
          if (my_LisaConfigFrame) {delete my_LisaConfigFrame; my_LisaConfigFrame=NULL;}

          if (!my_LisaConfigFrame)    my_LisaConfigFrame=new LisaConfigFrame( wxT("Preferences"), my_lisaconfig);
          #if defined(__WXMOTIF__)
          int x, y;

          GetSize(&x, &y);
          x+=WINXDIFF; y+=WINYDIFF;
          SetSize(wxDefaultCoord, wxDefaultCoord,x,y);
          #endif

          my_LisaConfigFrame->Show();

}

void LisaEmFrame::OnOpen(wxCommandEvent& WXUNUSED(event))
{
       wxString openfile;
       wxFileDialog open(this,                  wxT("Open LisaEm Preferences:"),
                                                wxEmptyString,
                                                wxEmptyString,
                                                wxT("LisaEm Preferences (*.lisaem)|*.lisaem|All (*.*)|*.*"),
                                                (long int)wxFD_OPEN,wxDefaultPosition);

       if (open.ShowModal()==wxID_OK)           openfile=open.GetPath();
       else return;


       if (pConfig) {pConfig->Flush();  delete pConfig; pConfig=NULL;}
       if (my_LisaConfigFrame) {delete my_LisaConfigFrame; my_LisaConfigFrame=NULL;}

       pConfig=new wxFileConfig(_T("LisaEm"),
                              _T("sunder.NET"),
                                (openfile),     //local
                                (openfile),     //global
                              wxCONFIG_USE_LOCAL_FILE,
                              wxConvAuto() );   // or wxConvUTF8

      pConfig->Get(true);
      if (my_lisaconfig) delete my_lisaconfig;
      my_lisaconfig = new LisaConfig();
      my_lisaconfig->Load(pConfig, floppy_ram);// load it in





      myconfigfile=openfile;  // update global file to point to new config
      save_global_prefs();    // and save them.
}

void LisaEmFrame::OnSaveAs(wxCommandEvent& WXUNUSED(event))
{
       wxString savefile;

	   wxFileName prefs=wxFileName(myconfigfile);
	   wxString justTheFilename=prefs.GetFullName();
 	   wxString justTheDir=prefs.GetPath(wxPATH_GET_VOLUME|wxPATH_GET_SEPARATOR,wxPATH_NATIVE);


       wxFileDialog open(this,                  wxT("Save LisaEm Preferences As:"),
                                                justTheDir,  // path
                                                justTheFilename,  // prefs file name
                                                wxT("LisaEm Preferences (*.lisaem)|*.lisaem|All (*.*)|*.*"),
                                                (long int)wxFD_SAVE,wxDefaultPosition
                        );

       if (open.ShowModal()==wxID_OK)           savefile=open.GetPath();
       else return;

       wxFileOutputStream out(savefile);

       if (pConfig->Save(out,wxConvUTF8) )
          {
            myconfigfile=savefile;
            save_global_prefs();
          }
}


void LisaEmFrame::OnDebugger(wxCommandEvent& WXUNUSED(event))            {}



void LisaEmFrame::OnPOWERKEY(wxCommandEvent& WXUNUSED(event))            {fileMenu->Check(ID_RUN,true); handle_powerbutton();}


void LisaEmFrame::OnAPPLEPOWERKEY(wxCommandEvent& WXUNUSED(event))
        {
            send_cops_keycode(KEYCODE_COMMAND|KEY_DOWN);
            handle_powerbutton();
            send_cops_keycode(KEYCODE_COMMAND|KEY_UP);
        }

void LisaEmFrame::OnTraceLog(wxCommandEvent& WXUNUSED(event))
     {
      debug_log_enabled=!debug_log_enabled;
      if (debug_log_enabled) debug_on("user enabled");
      else                   debug_off();

      ALERT_LOG(0,"Debug has been turned %s",debug_log_enabled?"on":"off");
     }

void LisaEmFrame::OnKEY_APL_DOT(wxCommandEvent& WXUNUSED(event))         {apple_dot();}
void LisaEmFrame::OnKEY_APL_S(wxCommandEvent& WXUNUSED(event))           {apple_S();}
void LisaEmFrame::OnKEY_APL_ENTER(wxCommandEvent& WXUNUSED(event))       {apple_enter();}
void LisaEmFrame::OnKEY_APL_RENTER(wxCommandEvent& WXUNUSED(event))      {apple_renter();}
void LisaEmFrame::OnKEY_APL_1(wxCommandEvent& WXUNUSED(event))           {apple_1();}
void LisaEmFrame::OnKEY_APL_2(wxCommandEvent& WXUNUSED(event))           {apple_2();}
void LisaEmFrame::OnKEY_APL_3(wxCommandEvent& WXUNUSED(event))           {apple_3();}

void LisaEmFrame::OnKEY_OPT_0(wxCommandEvent& WXUNUSED(event))           {shift_option_0();}
void LisaEmFrame::OnKEY_OPT_4(wxCommandEvent& WXUNUSED(event))           {shift_option_4();}
void LisaEmFrame::OnKEY_OPT_7(wxCommandEvent& WXUNUSED(event))           {shift_option_7();}
void LisaEmFrame::OnKEY_NMI(wxCommandEvent& WXUNUSED(event))             {send_nmi_key();}
void LisaEmFrame::OnKEY_RESET(wxCommandEvent& WXUNUSED(event))
{
	if (!running)
	{
		wxMessageBox(wxT("The Lisa isn't powered.  Pressing RESET now won't do anything interesting."),
					 wxT("Lisa isn't powered on"), wxICON_INFORMATION | wxOK);
		return;
	}

   if (yesnomessagebox("This will reboot the Lisa, but may cause file system corruption",
					   "Really RESET the Lisa?")==0) return;

   lisa_rebooted();
}

// display a floating keyboard and allow edits.
void LisaEmFrame::OnKEYBOARD(wxCommandEvent& WXUNUSED(event))  {}




void LisaEmFrame::OnASCIIKB(wxCommandEvent& WXUNUSED(event))   {asciikeyboard= 1; save_global_prefs();}
void LisaEmFrame::OnRAWKB(wxCommandEvent& WXUNUSED(event))     {asciikeyboard= 0; save_global_prefs();}
void LisaEmFrame::OnRAWKBBUF(wxCommandEvent& WXUNUSED(event))  {asciikeyboard=-1; save_global_prefs();}

/* Make sure only one of the Throttle menu items is checked at any time. */
void updateThrottleMenus(float throttle)
{
    if (!my_lisaframe) return;
    if (!throttleMenu) return;

    my_lisaframe->reset_throttle_clock();

    throttleMenu->Check(ID_THROTTLE5,   throttle ==  5.0);
    throttleMenu->Check(ID_THROTTLE8,   throttle ==  8.0);
    throttleMenu->Check(ID_THROTTLE10,  throttle == 10.0);
    throttleMenu->Check(ID_THROTTLE12,  throttle == 12.0);
    throttleMenu->Check(ID_THROTTLE16,  throttle == 16.0);
    throttleMenu->Check(ID_THROTTLE32,  throttle == 32.0);
//    throttleMenu->Check(ID_THROTTLEX,   throttle == 10000000);

	throttleMenu->Check(ID_ET100_75,    emulation_time==100 && emulation_tick==75);
	throttleMenu->Check(ID_ET50_30,     emulation_time== 50 && emulation_tick==30);
	throttleMenu->Check(ID_ET40_25,     emulation_time== 40 && emulation_tick==25);
	throttleMenu->Check(ID_ET30_20,     emulation_time== 30 && emulation_tick==20);


    #ifdef TIE_VIA_TIMER_TO_HOST
    via_throttle_factor=throttle/5.0;
    #endif
}



// This is necessary since if we change the throttle, the OnIdle
// execution loop needs a new reference point, else it will try
// to adjust the runtime from power on which will not work.
void LisaEmFrame::reset_throttle_clock(void)
{
   cpu68k_reference=cpu68k_clocks;
   last_runtime_sample=0;
   lastcrtrefresh=0;
   runtime.Start(0);
   if    (throttle  ==10000000)
          clockfactor=0;
   else   clockfactor=1.0/(throttle*1000.0);
   cycles_wanted=(XTIMER)(float)(emulation_time/clockfactor);
}


void LisaEmFrame::OnET100_75(wxCommandEvent& WXUNUSED(event))  {emulation_time=100; emulation_tick=75; reset_throttle_clock(); save_global_prefs();}
void LisaEmFrame::OnET50_30(wxCommandEvent& WXUNUSED(event))   {emulation_time= 50; emulation_tick=30; reset_throttle_clock(); save_global_prefs();}
void LisaEmFrame::OnET40_25(wxCommandEvent& WXUNUSED(event))   {emulation_time= 40; emulation_tick=25; reset_throttle_clock(); save_global_prefs();}
void LisaEmFrame::OnET30_20(wxCommandEvent& WXUNUSED(event))   {emulation_time= 30; emulation_tick=20; reset_throttle_clock(); save_global_prefs();}


void LisaEmFrame::OnThrottle5(wxCommandEvent& WXUNUSED(event))
{
    throttle = 5;
    reset_throttle_clock();
    save_global_prefs();
}

void LisaEmFrame::OnThrottle8(wxCommandEvent& WXUNUSED(event))
{
    throttle = 8;
    reset_throttle_clock();
    save_global_prefs();
}

void LisaEmFrame::OnThrottle10(wxCommandEvent& WXUNUSED(event))
{
    throttle = 10;
    reset_throttle_clock();
    save_global_prefs();
}

void LisaEmFrame::OnThrottle12(wxCommandEvent& WXUNUSED(event))
{
    throttle = 12;
    reset_throttle_clock();
    save_global_prefs();
}
void LisaEmFrame::OnThrottle16(wxCommandEvent& WXUNUSED(event))
{
    throttle = 16;
    reset_throttle_clock();
    save_global_prefs();
}

void LisaEmFrame::OnThrottle32(wxCommandEvent& WXUNUSED(event))
{
    throttle=32;
    reset_throttle_clock();
    save_global_prefs();
}

void LisaEmFrame::OnThrottleX(wxCommandEvent& WXUNUSED(event))
{
    throttle=10000000;
    reset_throttle_clock();
    save_global_prefs();
}




extern "C" void messagebox(char *s, char *t)  // messagebox string of text, title
           {
           	 wxString text =wxString(s, wxConvLocal, 2048); //wxSTRING_MAXLEN);
 			 wxString title=wxString(t, wxConvLocal, 2048); //wxSTRING_MAXLEN);
             wxMessageBox(text,title, wxICON_INFORMATION | wxOK);
           }

extern "C" int yesnomessagebox(char *s, char *t)  // messagebox string of text, title
           {
           	 wxString text =wxString(s, wxConvLocal, 2048); //wxSTRING_MAXLEN);
 			 wxString title=wxString(t, wxConvLocal, 2048); //wxSTRING_MAXLEN);
             wxMessageDialog w(my_lisawin,text,title, wxICON_QUESTION  | wxYES_NO |wxNO_DEFAULT,wxDefaultPosition );
             if (w.ShowModal()==wxID_YES) return 1;
             return 0;
           }





void LisaEmFrame::UpdateProfileMenu(void)
{
  if (!profileMenu)   return;
  if (!my_lisaconfig) return;
  // s=paralell port, 3=s1h, 4=s1l, 5=s2h,6=s2l,7=s3h,8=s3l

  profileMenu->Check(ID_PROFILEPWR,     (IS_PARALLEL_PORT_ENABLED(2) && !my_lisaconfig->parallel.IsSameAs(_T("NOTHING"), false)  ));
                                           
  profileMenu->Check(ID_PROFILE_S1U,    (IS_PARALLEL_PORT_ENABLED(3) && (my_lisaconfig->slot1.IsSameAs(_T("dualparallel"),false)) && !my_lisaconfig->s1h.IsSameAs(_T("NOTHING"), false)  ));
  profileMenu->Check(ID_PROFILE_S1L,    (IS_PARALLEL_PORT_ENABLED(4) && (my_lisaconfig->slot1.IsSameAs(_T("dualparallel"),false)) && !my_lisaconfig->s1l.IsSameAs(_T("NOTHING"), false)  ));
  profileMenu->Check(ID_PROFILE_S2U,    (IS_PARALLEL_PORT_ENABLED(5) && (my_lisaconfig->slot2.IsSameAs(_T("dualparallel"),false)) && !my_lisaconfig->s2h.IsSameAs(_T("NOTHING"), false)  ));
  profileMenu->Check(ID_PROFILE_S2L,    (IS_PARALLEL_PORT_ENABLED(6) && (my_lisaconfig->slot2.IsSameAs(_T("dualparallel"),false)) && !my_lisaconfig->s2l.IsSameAs(_T("NOTHING"), false)  ));
  profileMenu->Check(ID_PROFILE_S3U,    (IS_PARALLEL_PORT_ENABLED(7) && (my_lisaconfig->slot3.IsSameAs(_T("dualparallel"),false)) && !my_lisaconfig->s3h.IsSameAs(_T("NOTHING"), false)  ));
  profileMenu->Check(ID_PROFILE_S3L,    (IS_PARALLEL_PORT_ENABLED(8) && (my_lisaconfig->slot3.IsSameAs(_T("dualparallel"),false)) && !my_lisaconfig->s3l.IsSameAs(_T("NOTHING"), false)  ));

  profileMenu->Enable(ID_PROFILEPWR,( !my_lisaconfig->parallel.IsSameAs(_T("NOTHING"), false) ));
  if ( my_lisaconfig->parallel.IsSameAs(_T("PROFILE"), false) )  
     profileMenu->SetLabel(ID_PROFILEPWR,wxT("Power ProFile Parallel Port"));
  else
     profileMenu->SetLabel(ID_PROFILEPWR,wxT("Power ADMP on Parallel Port"));

// GLOBAL(int,via_port_idx_bits[],{0, 2,1, 4,3, 6,5});
                              //2, 4,3, 6,5, 8,7

  if (my_lisaconfig->slot1.IsSameAs(_T("dualparallel"),false))
  {
    if (my_lisaconfig->s1h.IsSameAs(_T("PROFILE"), false)) profileMenu->SetLabel(ID_PROFILE_S1U,wxT("Power ProFile on Slot 1 Upper Port"));
    if (my_lisaconfig->s1l.IsSameAs(_T("PROFILE"), false)) profileMenu->SetLabel(ID_PROFILE_S1L,wxT("Power ProFile on Slot 1 Lower Port"));

    if (my_lisaconfig->s1h.IsSameAs(_T("ADMP"), false)) profileMenu->SetLabel(ID_PROFILE_S1U,wxT("Power ADMP on Slot 1 Upper Port"));
    if (my_lisaconfig->s1l.IsSameAs(_T("ADMP"), false)) profileMenu->SetLabel(ID_PROFILE_S1L,wxT("Power ADMP on Slot 1 Lower Port"));

    if (my_lisaconfig->s1h.IsSameAs(_T("NOTHING"), false)) profileMenu->SetLabel(ID_PROFILE_S1U,wxT("Nothing on Slot 1 Upper Port"));
    if (my_lisaconfig->s1l.IsSameAs(_T("NOTHING"), false)) profileMenu->SetLabel(ID_PROFILE_S1L,wxT("Nothing on Slot 1 Lower Port"));


	profileMenu->Enable(ID_PROFILE_S1U,( !my_lisaconfig->s1h.IsSameAs(_T("NOTHING"), false)) );      
	profileMenu->Enable(ID_PROFILE_S1L,( !my_lisaconfig->s1l.IsSameAs(_T("NOTHING"), false)) );
  }
  else 
  {
	profileMenu->Enable(ID_PROFILE_S1U,false );      
	profileMenu->Enable(ID_PROFILE_S1L,false );
	profileMenu->SetLabel(ID_PROFILE_S1U,_T("No Card"));
	profileMenu->SetLabel(ID_PROFILE_S1L,_T("No Card"));
  }


  if (my_lisaconfig->slot2.IsSameAs(_T("dualparallel"),false))
  {
    if (my_lisaconfig->s2h.IsSameAs(_T("PROFILE"), false)) profileMenu->SetLabel(ID_PROFILE_S2U,wxT("Power ProFile on Slot 2 Upper Port"));
    if (my_lisaconfig->s2l.IsSameAs(_T("PROFILE"), false)) profileMenu->SetLabel(ID_PROFILE_S2L,wxT("Power ProFile on Slot 2 Lower Port"));

    if (my_lisaconfig->s2h.IsSameAs(_T("ADMP"), false)) profileMenu->SetLabel(ID_PROFILE_S2U,wxT("Power ADMP on Slot 2 Upper Port"));
    if (my_lisaconfig->s2l.IsSameAs(_T("ADMP"), false)) profileMenu->SetLabel(ID_PROFILE_S2L,wxT("Power ADMP on Slot 2 Lower Port"));

    if (my_lisaconfig->s2h.IsSameAs(_T("NOTHING"), false)) profileMenu->SetLabel(ID_PROFILE_S2U,wxT("Nothing on Slot 2 Upper Port"));
    if (my_lisaconfig->s2l.IsSameAs(_T("NOTHING"), false)) profileMenu->SetLabel(ID_PROFILE_S2L,wxT("Nothing on Slot 2 Lower Port"));


	profileMenu->Enable(ID_PROFILE_S2U,( !my_lisaconfig->s2h.IsSameAs(_T("NOTHING"), false)) );      
	profileMenu->Enable(ID_PROFILE_S2L,( !my_lisaconfig->s2l.IsSameAs(_T("NOTHING"), false)) );
  }
  else 
  {
	profileMenu->Enable(ID_PROFILE_S2U,false );      
	profileMenu->Enable(ID_PROFILE_S2L,false );
	profileMenu->SetLabel(ID_PROFILE_S2U,_T("No Card"));
	profileMenu->SetLabel(ID_PROFILE_S2L,_T("No Card"));
  }


  if (my_lisaconfig->slot3.IsSameAs(_T("dualparallel"),false))
  {
    if (my_lisaconfig->s3h.IsSameAs(_T("PROFILE"), false)) profileMenu->SetLabel(ID_PROFILE_S3U,wxT("Power ProFile on Slot 3 Upper Port"));
    if (my_lisaconfig->s3l.IsSameAs(_T("PROFILE"), false)) profileMenu->SetLabel(ID_PROFILE_S3L,wxT("Power ProFile on Slot 3 Lower Port"));

    if (my_lisaconfig->s3h.IsSameAs(_T("ADMP"), false)) profileMenu->SetLabel(ID_PROFILE_S3U,wxT("Power ADMP on Slot 3 Upper Port"));
    if (my_lisaconfig->s3l.IsSameAs(_T("ADMP"), false)) profileMenu->SetLabel(ID_PROFILE_S3L,wxT("Power ADMP on Slot 3 Lower Port"));

    if (my_lisaconfig->s3h.IsSameAs(_T("NOTHING"), false)) profileMenu->SetLabel(ID_PROFILE_S3U,wxT("Nothing on Slot 3 Upper Port"));
    if (my_lisaconfig->s3l.IsSameAs(_T("NOTHING"), false)) profileMenu->SetLabel(ID_PROFILE_S3L,wxT("Nothing on Slot 3 Lower Port"));

	profileMenu->Enable(ID_PROFILE_S3U,( !my_lisaconfig->s3h.IsSameAs(_T("NOTHING"), false)) );      
	profileMenu->Enable(ID_PROFILE_S3L,( !my_lisaconfig->s3l.IsSameAs(_T("NOTHING"), false)) );
  }
  else 
  {
	profileMenu->Enable(ID_PROFILE_S3U,false );      
	profileMenu->Enable(ID_PROFILE_S3L,false );
	profileMenu->SetLabel(ID_PROFILE_S3U,_T("No Card"));
	profileMenu->SetLabel(ID_PROFILE_S3L,_T("No Card"));
  }

}


void LisaEmFrame::OnProFilePowerX(int bit)
{

  int devtype=0;

  if (running)
   if (check_running_lisa_os()!=LISA_ROM_RUNNING)
   {
   switch(bit)
   { 
 	case 2: if (my_lisaconfig->parallel.IsSameAs(_T("PROFILE"), false)) devtype=1; break;

    case 3: if (my_lisaconfig->s1h.IsSameAs(_T("PROFILE"), false)) devtype=1; break;
    case 4: if (my_lisaconfig->s1l.IsSameAs(_T("PROFILE"), false)) devtype=1; break;

    case 5: if (my_lisaconfig->s2h.IsSameAs(_T("PROFILE"), false)) devtype=1; break;
    case 6: if (my_lisaconfig->s2l.IsSameAs(_T("PROFILE"), false)) devtype=1; break;

    case 7: if (my_lisaconfig->s3h.IsSameAs(_T("PROFILE"), false)) devtype=1; break;
    case 8: if (my_lisaconfig->s3l.IsSameAs(_T("PROFILE"), false)) devtype=1; break;
    }
   }

  if (running && (IS_PARALLEL_PORT_ENABLED(bit)) && devtype)
  {
    if (yesnomessagebox("The Lisa may be using this Profile hard drive.  Powering it off may cause file system damage.  Are you sure  you wish to power it off?",
                        "DANGER!")==0) return;
  }

  profile_power^=(1<<(bit-2));

  UpdateProfileMenu();
}


void LisaEmFrame::OnProFilePower( wxCommandEvent& WXUNUSED(event))  {OnProFilePowerX(2);}
void LisaEmFrame::OnProFileS1UPwr(wxCommandEvent& WXUNUSED(event))  {OnProFilePowerX(3);}
void LisaEmFrame::OnProFileS1LPwr(wxCommandEvent& WXUNUSED(event))  {OnProFilePowerX(4);}
void LisaEmFrame::OnProFileS2UPwr(wxCommandEvent& WXUNUSED(event))  {OnProFilePowerX(5);}
void LisaEmFrame::OnProFileS2LPwr(wxCommandEvent& WXUNUSED(event))  {OnProFilePowerX(6);}
void LisaEmFrame::OnProFileS3UPwr(wxCommandEvent& WXUNUSED(event))  {OnProFilePowerX(7);}
void LisaEmFrame::OnProFileS3LPwr(wxCommandEvent& WXUNUSED(event))  {OnProFilePowerX(8);}

void LisaEmFrame::OnProFilePwrOnAll(wxCommandEvent& WXUNUSED(event))
{  profile_power=127; UpdateProfileMenu();}

void LisaEmFrame::OnProFilePwrOffAll(wxCommandEvent& WXUNUSED(event))
{
  if (running && profile_power)
  {
    if (yesnomessagebox("Lisa is using the profile drives. Are you sure you wish to remove power to all Profile drives?",
                        "DANGER!")==0) return;
  }
  profile_power=0; UpdateProfileMenu();
}


void LisaEmFrame::OnNewProFile(wxCommandEvent& WXUNUSED(event))     
{      int sz;
       int blocks[]={9728,19456,32768,40960,65536,81920,131072};
       //               0     1     2     3     4     5     6
       //              5M   10M   16M   20M   32M   40M    64M

	 char cfilename[MAXPATHLEN];
	 wxFileDialog open(this,                        wxT("Create blank ProFile drive as:"),
	                                                wxEmptyString,
	                                                wxT("lisaem-profile.dc42"),
	                                                wxT("Disk Copy (*.dc42)|*.dc42|All (*.*)|*.*"),
	                                                (long int)wxFD_SAVE,wxDefaultPosition);

	 if (open.ShowModal()==wxID_OK)                 
	 {
	 wxString filename=open.GetPath();
	 strncpy(cfilename,filename.fn_str(),MAXPATHLEN);
     sz=pickprofilesize(cfilename);  if (sz<0 || sz>6) return;
     int i=dc42_create(cfilename,"-lisaem.sunder.net hd-",blocks[sz]*512,blocks[sz]*20);
     if (i) 
        wxMessageBox(wxT("Could not create the file to store the Profile.\n\nDo you have permission to write to this folder?\nIs there enough free disk space?"),
        wxT("Failed to create drive!"));
     }
}







extern "C" void eject_floppy_animation(void)
{
 if ((my_lisawin->floppystate & FLOPPY_ANIM_MASK)==FLOPPY_PRESENT)          // initiate eject animation sequence
     {my_lisawin->floppystate=FLOPPY_NEEDS_REDRAW|FLOPPY_INSERT_2; return;}

   return;
}


extern "C" void floppy_motor_sounds(int track)
{
// there really are 3-4 speeds, however, my admittedly Bolt Thrower damaged ears can only distinguish two  :)
// close enough for government work, I guess.

#ifndef __WXMSW__
 if (wxSound::IsPlaying()) return;       // doesn't work on windows
#else
 if (my_lisaframe->soundplaying)
    if (my_lisaframe->soundsw.Time()<200) return;
my_lisaframe->soundplaying=1;
my_lisaframe->soundsw.Start(0);
#endif
 wxSound::Stop();

 if (sound_effects_on)
 {
   if (track>35)  {if (my_floppy_motor1->IsOk()) my_floppy_motor1->Play(wxSOUND_ASYNC);}
   else           {if (my_floppy_motor2->IsOk()) my_floppy_motor2->Play(wxSOUND_ASYNC);}
 }

}




void LisaEmFrame::OnFLOPPY(wxCommandEvent& WXUNUSED(event)) {OnxFLOPPY();}

void LisaEmFrame::OnxFLOPPY(void)
{
    if ((my_lisawin->floppystate & FLOPPY_ANIM_MASK)!=FLOPPY_EMPTY) {
        wxMessageBox(_T("A previously inserted diskette is still in the drive. "
            "Please eject the diskette before inserting another."),
            _T("Diskette is already inserted!"), wxICON_INFORMATION | wxOK);
        return;
    }

    pause_run();

    wxString openfile;
    wxFileDialog open(this,                     wxT("Insert a Lisa diskette"),
                                                wxEmptyString,
                                                wxEmptyString,
                                                wxT("Disk Copy (*.dc42)|*.dc42|DART (*.dart)|*.dart|All (*.*)|*.*"),
                                                (long int)wxFD_OPEN,wxDefaultPosition);
    if (open.ShowModal()==wxID_OK)              openfile=open.GetPath();

    resume_run();


    if (!openfile.Len()) return;
    const wxCharBuffer s = openfile.fn_str();

    if (my_lisaframe->running) {
        if (floppy_insert((char *)(const char *)s)) return;
    } else {
        floppy_to_insert = openfile;
    }

    if ((my_lisawin->floppystate & FLOPPY_ANIM_MASK)==FLOPPY_EMPTY) {
        // initiate insert animation sequence
        my_lisawin->floppystate=FLOPPY_NEEDS_REDRAW|FLOPPY_ANIMATING|FLOPPY_INSERT_0;
     }
}


void LisaEmFrame::OnNewFLOPPY(wxCommandEvent& WXUNUSED(event)) {OnxNewFLOPPY();}

void LisaEmFrame::OnxNewFLOPPY(void)
{
    if (!my_lisaframe->running) {
        wxMessageBox(_T("Please turn the Lisa on before inserting a diskette."),
            _T("The Lisa is Off"), wxICON_INFORMATION | wxOK);
        return;
    }

    if ((my_lisawin->floppystate & FLOPPY_ANIM_MASK)!=FLOPPY_EMPTY) {
        wxMessageBox(_T("A previously inserted diskette is still in the drive. "
            "Please eject the diskette before inserting another."),
            _T("Diskette is already inserted!"), wxICON_INFORMATION | wxOK);
        return;
    }

    pause_run();

    wxString openfile;
    wxFileDialog open(this,                     wxT("Create and insert a blank microdiskette image"),
                                                wxEmptyString,
                                                wxT("blank.dc42"),
                                                wxT("Disk Copy (*.dc42)|*.dc42|All (*.*)|*.*"),
                                                (long int)wxFD_SAVE,wxDefaultPosition);
    if (open.ShowModal()==wxID_OK)              openfile=open.GetPath();

    resume_run();
    if (!openfile.Len()) return;



    const wxCharBuffer s = openfile.fn_str();
    int i = dc42_create((char *)(const char *)s,"-not a Macintosh disk-", 400*512*2,400*2*12);
    if (i)  {
        wxMessageBox(_T("Could not create the diskette"),
            _T("Sorry"), wxICON_INFORMATION | wxOK);
        return;
    } else {
        floppy_insert((char *)(const char *)s);
    }


 if ((my_lisawin->floppystate & FLOPPY_ANIM_MASK)==FLOPPY_EMPTY) {
     // initiate insert animation sequence
     my_lisawin->floppystate=FLOPPY_NEEDS_REDRAW|FLOPPY_ANIMATING|FLOPPY_INSERT_0;
 }
}

//  for future use - for when we figure out the right way to turn of skins on the fly
//  - the real issue is the window size mechanism doesn't work properly.  won't prevent the user
//  from stretching the window, or making it too small.  also in win32/linux with skins off
//  the window is too small - shows scrollbars, but it allows stretching past the lisa screen bitmap
//  which causes garbage in the non-refreshed space.
void LisaEmFrame::UnloadImages(void)
{
   if (display_image)   {   delete display_image; display_image  = NULL;}
	 
   if ( my_skinDC     )  {  delete my_skinDC;     my_skinDC      = NULL;}
   if ( my_skin0DC    )  {  delete my_skin0DC;    my_skin0DC     = NULL;}
   if ( my_skin1DC    )  {  delete my_skin1DC;    my_skin1DC     = NULL;}
   if ( my_skin2DC    )  {  delete my_skin2DC;    my_skin2DC     = NULL;}
   if ( my_skin3DC    )  {  delete my_skin3DC;    my_skin3DC     = NULL;}
   if ( my_floppy0DC  )  {  delete my_floppy0DC;  my_floppy0DC   = NULL;}
   if ( my_floppy1DC  )  {  delete my_floppy1DC;  my_floppy1DC   = NULL;}
   if ( my_floppy2DC  )  {  delete my_floppy2DC;  my_floppy2DC   = NULL;}
   if ( my_floppy3DC  )  {  delete my_floppy3DC;  my_floppy3DC   = NULL;}
   if ( my_floppy4DC  )  {  delete my_floppy4DC;  my_floppy4DC   = NULL;}
   if ( my_poweronDC  )  {  delete my_poweronDC;  my_poweronDC   = NULL;}
   if ( my_poweroffDC )  {  delete my_poweroffDC; my_poweroffDC  = NULL;}

   if ( my_skin       )  {  delete my_skin;       my_skin        = NULL;}
   if ( my_skin0      )  {  delete my_skin0;      my_skin0       = NULL;}
   if ( my_skin1      )  {  delete my_skin1;      my_skin1       = NULL;}
   if ( my_skin2      )  {  delete my_skin2;      my_skin2       = NULL;}
   if ( my_skin3      )  {  delete my_skin3;      my_skin3       = NULL;}
   if ( my_floppy0    )  {  delete my_floppy0;    my_floppy0     = NULL;}
   if ( my_floppy1    )  {  delete my_floppy1;    my_floppy1     = NULL;}
   if ( my_floppy2    )  {  delete my_floppy2;    my_floppy2     = NULL;}
   if ( my_floppy3    )  {  delete my_floppy3;    my_floppy3     = NULL;}
   if ( my_floppy4    )  {  delete my_floppy4;    my_floppy4     = NULL;}
   if ( my_poweron    )  {  delete my_poweron;    my_poweron     = NULL;}
   if ( my_poweroff   )  {  delete my_poweroff;   my_poweroff    = NULL;}

   skins_on=0;

   int x,y;
   my_lisawin->SetClientSize(wxSize(effective_lisa_vid_size_x,effective_lisa_vid_size_y));     // LisaWin //
   my_lisawin->GetClientSize(&x,&y);                                                           // LisaWin //
   my_lisawin->GetSize(&x,&y);                                                                 // LisaWin //

   screen_origin_x=(x  - effective_lisa_vid_size_x)>>1;                                        // center display
   screen_origin_y=(y  - effective_lisa_vid_size_y)>>1;                                        // on skinless
   screen_origin_x= (screen_origin_x<0 ? 0:screen_origin_x);
   screen_origin_y= (screen_origin_y<0 ? 0:screen_origin_y);


// X11, Windows return too small a value for getsize
   x+=WINXDIFF; y+=WINYDIFF;
   SetMaxSize(wxSize(ISKINSIZE));                                                              // Frame   //
   SetMinSize(wxSize(720,384));                                                                // Frame   //

   SendSizeEvent();
}

void LisaEmFrame::LoadImages(void)
{

/*
 * On MacOS X, load the images from the Resources/ dir inside the app bundle.
 * Much faster than WX's breathtakingly slow parsing of XPMs, and much
 * smaller too since we can use PNGs.
 * On Linux/Win32 use embedded XPM strings/BMP resources.
 */

if (skins_on)
{
   if (!my_skin) my_skin     = new wxBitmap(ISKINSIZEX,ISKINSIZEY,DEPTH);//, -1);  //20061228//
   if (display_image)   {   delete display_image; display_image  = NULL;}

   wxString pngfile;
   wxStandardPathsBase& stdp = wxStandardPaths::Get();
   wxString rscDir = stdp.GetResourcesDir() + wxFileName::GetPathSeparator(wxPATH_NATIVE);
	
    // stare not upon the insanity that predated this code, for it was written by drunkards! (*Burp*)

// OS X, and Unixen will use resource dirs to load png files
#ifndef __WXMSW__

   pngfile=rscDir + _T("lisaface0.png");  if (!my_skin0)    my_skin0    = new wxBitmap(pngfile, wxBITMAP_TYPE_PNG);
   pngfile=rscDir + _T("lisaface1.png");  if (!my_skin1)    my_skin1    = new wxBitmap(pngfile, wxBITMAP_TYPE_PNG);
   pngfile=rscDir + _T("lisaface2.png");  if (!my_skin2)    my_skin2    = new wxBitmap(pngfile, wxBITMAP_TYPE_PNG);
   pngfile=rscDir + _T("lisaface3.png");  if (!my_skin3)    my_skin3    = new wxBitmap(pngfile, wxBITMAP_TYPE_PNG);
                  
   pngfile=rscDir + _T("floppy0.png");    if (!my_floppy0)  my_floppy0  = new wxBitmap(pngfile, wxBITMAP_TYPE_PNG);
   pngfile=rscDir + _T("floppy1.png");    if (!my_floppy1)  my_floppy1  = new wxBitmap(pngfile, wxBITMAP_TYPE_PNG);
   pngfile=rscDir + _T("floppy2.png");    if (!my_floppy2)  my_floppy2  = new wxBitmap(pngfile, wxBITMAP_TYPE_PNG);
   pngfile=rscDir + _T("floppy3.png");    if (!my_floppy3)  my_floppy3  = new wxBitmap(pngfile, wxBITMAP_TYPE_PNG);
   pngfile=rscDir + _T("floppyN.png");    if (!my_floppy4)  my_floppy4  = new wxBitmap(pngfile, wxBITMAP_TYPE_PNG);
                  
   pngfile=rscDir + _T("power_on.png");   if (!my_poweron ) my_poweron  = new wxBitmap(pngfile, wxBITMAP_TYPE_PNG);
   pngfile=rscDir + _T("power_off.png");  if (!my_poweroff) my_poweroff = new wxBitmap(pngfile, wxBITMAP_TYPE_PNG);

// windoze will load BMP resources from the .EXE file
#else
   if (!my_skin0)     my_skin0     = new wxBITMAP( lisaface0 );
   if (!my_skin1)     my_skin1     = new wxBITMAP( lisaface1 );
   if (!my_skin2)     my_skin2     = new wxBITMAP( lisaface2 );
   if (!my_skin3)     my_skin3     = new wxBITMAP( lisaface3 );
                                                               
   if (!my_floppy0)   my_floppy0   = new wxBITMAP( floppy0   );
   if (!my_floppy1)   my_floppy1   = new wxBITMAP( floppy1   );
   if (!my_floppy2)   my_floppy2   = new wxBITMAP( floppy2   );
   if (!my_floppy3)   my_floppy3   = new wxBITMAP( floppy3   );
   if (!my_floppy4)   my_floppy4   = new wxBITMAP( floppyN   );

   if (!my_poweron )  my_poweron   = new wxBITMAP( power_on  );
   if (!my_poweroff)  my_poweroff  = new wxBITMAP( power_off );

#endif

   if (!my_skinDC)     my_skinDC    = new wxMemoryDC;
   if (!my_skin0DC)    my_skin0DC   = new wxMemoryDC;
   if (!my_skin1DC)    my_skin1DC   = new wxMemoryDC;
   if (!my_skin2DC)    my_skin2DC   = new wxMemoryDC;
   if (!my_skin3DC)    my_skin3DC   = new wxMemoryDC;

   if (!my_floppy0DC)  my_floppy0DC = new wxMemoryDC;
   if (!my_floppy1DC)  my_floppy1DC = new wxMemoryDC;
   if (!my_floppy2DC)  my_floppy2DC = new wxMemoryDC;
   if (!my_floppy3DC)  my_floppy3DC = new wxMemoryDC;
   if (!my_floppy4DC)  my_floppy4DC = new wxMemoryDC;

   if (!my_poweronDC)  my_poweronDC = new wxMemoryDC;
   if (!my_poweroffDC) my_poweroffDC= new wxMemoryDC;

   my_skinDC->SelectObject(*my_skin);

   my_skin0DC->SelectObject(*my_skin0);
   my_skin1DC->SelectObject(*my_skin1);
   my_skin2DC->SelectObject(*my_skin2);
   my_skin3DC->SelectObject(*my_skin3);

   int y=0;
   my_skinDC->Blit(0,y, my_skin0->GetWidth(),my_skin0->GetHeight(),my_skin0DC, 0,0 ,wxCOPY, false);   y+=my_skin0->GetHeight();
   my_skinDC->Blit(0,y, my_skin1->GetWidth(),my_skin1->GetHeight(),my_skin1DC, 0,0 ,wxCOPY, false);   y+=my_skin1->GetHeight();
   my_skinDC->Blit(0,y, my_skin2->GetWidth(),my_skin2->GetHeight(),my_skin2DC, 0,0 ,wxCOPY, false);   y+=my_skin2->GetHeight();
   my_skinDC->Blit(0,y, my_skin3->GetWidth(),my_skin3->GetHeight(),my_skin3DC, 0,0 ,wxCOPY, false); //y+=my_skin3->GetHeight();

   my_floppy0DC->SelectObject(*my_floppy0);
   my_floppy1DC->SelectObject(*my_floppy1);
   my_floppy2DC->SelectObject(*my_floppy2);
   my_floppy3DC->SelectObject(*my_floppy3);
   my_floppy4DC->SelectObject(*my_floppy4);
   my_poweronDC->SelectObject(*my_poweron);
   my_poweroffDC->SelectObject(*my_poweroff);

   my_lisawin->SetMinSize(wxSize(IWINSIZE));
   my_lisawin->SetClientSize(wxSize(IWINSIZE));                                                     // Lisawin   //
   my_lisawin->SetMaxSize(wxSize(ISKINSIZE));                                                       // Lisawin   //
   my_lisawin->SetScrollbars(ISKINSIZEX/100, ISKINSIZEY/100,  100,100,  0,0,  true);                // Lisawin   //
   my_lisawin->EnableScrolling(true,true);                                                          // Lisawin   //

   SendSizeEvent();

   screen_origin_x=140;
   screen_origin_y=130;
  }

}



LisaEmFrame::LisaEmFrame(const wxString& title)
       : wxFrame(NULL, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE)
{
   effective_lisa_vid_size_y=500;
   wxInitAllImageHandlers();

   barrier=0;
   clx=0;
   lastt2 = 0;
   lastclk = 0;
   lastcrtrefresh=0;
   hostrefresh=1000/ 8;

   int x,y;
   y=myConfig->Read(_T("/lisaframe/sizey"),(long)0);
   x=myConfig->Read(_T("/lisaframe/sizex"),(long)0);

   if (x<=0 || x>4096 || y<=0 || y>2048) {x=IWINSIZEX;y=IWINSIZEY;}
   //wxScreenDC theScreen;
   //theScreen.GetSize(&screensizex,&screensizey);

   wxDisplaySize(&screensizex,&screensizey);
   // Grrr! OS X sometimes returns garbage here
   if (screensizex< 0 || screensizex>8192 || screensizey<0 || screensizey>8192)
      {ALERT_LOG(0,"Got Garbage screen size: %d,%d",screensizex,screensizey);
	   screensizex=1024; screensizey=768;}

   if (x>screensizex || y>screensizey)  // make sure we don't get too big
      {
        x=MIN(screensizex-100,IWINSIZEX);
        y=MIN(screensizey-150,IWINSIZEY);
      }

#ifndef __WXOSX__
    SetIcon( wxICON( lisa2icon ));
#endif

    if (skins_on)
    {
       SetMinSize(wxSize(IWINSIZE));                                                                // Frame    //
       if (x<IWINSIZEX || y<IWINSIZEY) {x=IWINSIZEX; y=IWINSIZEY;}
       SetMinSize(wxSize(720,384));
       SetClientSize(wxSize(x,y));                                                                  // Frame    //
       GetClientSize(&dwx,&dwy);
       dwx-=x; dwy-=y;
       SetClientSize(wxSize(x-dwx,y-dwy));
       SetMaxSize(wxSize(ISKINSIZE));                                                               // Frame    //
    }                                                                                               // Frame    //
    else
    {
       // try to fit the Lisa's display on the screen at least.
       if (x<effective_lisa_vid_size_x || y<effective_lisa_vid_size_y)
	      {x=effective_lisa_vid_size_x;   y=effective_lisa_vid_size_y;}

       // but if it's too big, ensure we don't go over the display'size.
       x=MIN(screensizex-100,x); y=MIN(screensizey-150,x);

       SetMinSize(wxSize(720,384));
	   SetClientSize(wxSize(x,y));                                                                  // Frame    //
       GetClientSize(&dwx,&dwy);
       dwx-=x; dwy-=y;

       SetMaxSize(wxSize(ISKINSIZE));                                                               // Frame    //
 	}


    buildscreenymap();

    // Create a menu bar
    fileMenu     = new wxMenu;
    editMenu     = new wxMenu;
    keyMenu      = new wxMenu;
    DisplayMenu  = new wxMenu;
    throttleMenu = new wxMenu;
	profileMenu  = new wxMenu;
    helpMenu     = new wxMenu;




	profileMenu->Append(ID_PROFILE_ALL_ON,   wxT("Power On all Parallel Devices"), wxT("Powers on all parallel port attached devices.")  );
    profileMenu->Append(ID_PROFILE_ALL_OFF,  wxT("Power off all Parallel Devices"), wxT("Shuts off all parallel port attached devices.")  );

    profileMenu->AppendSeparator();

	profileMenu->AppendCheckItem(ID_PROFILEPWR,       wxT("Power ProFile on Parallel Port"), wxT("Toggles power to the Profile drive on the parallel port") );

	profileMenu->AppendSeparator();

    profileMenu->AppendCheckItem(ID_PROFILE_S1U,      wxT("Power ProFile on Slot 1 Upper Port"),wxT("Toggle power to the device") );
	profileMenu->AppendCheckItem(ID_PROFILE_S1L,      wxT("Power ProFile on Slot 1 Lower Port"),wxT("Toggle power to the device") );
	profileMenu->AppendSeparator();                                                                                      
                                                                                                                         
    profileMenu->AppendCheckItem(ID_PROFILE_S2U,      wxT("Power ProFile on Slot 2 Upper Port"),wxT("Toggle power to the device") );
    profileMenu->AppendCheckItem(ID_PROFILE_S2L,      wxT("Power ProFile on Slot 2 Lower Port"),wxT("Toggle power to the device") );
	profileMenu->AppendSeparator();                                                                                      
                                                                                                                         
    profileMenu->AppendCheckItem(ID_PROFILE_S3U,      wxT("Power ProFile on Slot 3 Upper Port"),wxT("Toggle power to the device") );
    profileMenu->AppendCheckItem(ID_PROFILE_S3L,      wxT("Power ProFile on Slot 3 Lower Port"),wxT("Toggle power to the device") );
                                                                                                                        
    editMenu->Append(wxID_PASTE, wxT("Paste") ,       wxT("Paste clipboard text to keyboard.") );

    DisplayMenu->AppendCheckItem(ID_VID_AA  ,         wxT("AntiAliased")           ,  wxT("Aspect Corrected with Anti Aliasing") );
    DisplayMenu->AppendCheckItem(ID_VID_AAG ,         wxT("AntiAliased with Gray Replacement"),  wxT("Aspect Corrected with Anti Aliasing and Gray Replacing") );
    //DisplayMenu->AppendCheckItem(ID_VID_SCALED,  wxT("Scaled")                ,  wxT("Scaled by wxWidgets") );

    DisplayMenu->AppendSeparator();
    DisplayMenu->AppendCheckItem(ID_VID_SY  ,         wxT("Raw")                   ,  wxT("Uncorrected Aspect Ratio") );
    DisplayMenu->AppendSeparator();

    DisplayMenu->AppendCheckItem(ID_VID_DY  ,         wxT("Double Y")              ,  wxT("Double Vertical Size") );
    DisplayMenu->AppendCheckItem(ID_VID_2X3Y,         wxT("Double X, Triple Y")    ,  wxT("Corrected Aspect Ratio, Large Display") );

    DisplayMenu->AppendSeparator();

    DisplayMenu->AppendCheckItem(ID_VID_SKINS_ON,     wxT("Turn Skins On"),         wxT("Turn on skins") );
    DisplayMenu->AppendCheckItem(ID_VID_SKINS_OFF,    wxT("Turn Skins Off"),        wxT("Turn off skins") );

    DisplayMenu->AppendSeparator();

  // reinstated as per request by Kallikak
    DisplayMenu->AppendCheckItem(ID_REFRESH_60Hz,wxT("60Hz Refresh"),wxT("60Hz Display Refresh - skip no frames - for fast machines"));
    DisplayMenu->AppendCheckItem(ID_REFRESH_20Hz,wxT("20Hz Refresh"),wxT("20Hz Display Refresh - display every 2nd frame"));
    DisplayMenu->AppendCheckItem(ID_REFRESH_12Hz,wxT("12Hz Refresh"),wxT("12Hz Display Refresh - display every 5th frame - for slow machines"));
    DisplayMenu->AppendCheckItem(ID_REFRESH_8Hz,wxT( " 8Hz Refresh"),wxT("8Hz Display Refresh - display every 7th frame - for slow machines"));
    DisplayMenu->AppendCheckItem(ID_REFRESH_4Hz,wxT( " 4Hz Refresh"),wxT("4Hz Display Refresh - display every 9th frame - for super slow machines"));

    DisplayMenu->AppendSeparator();

    DisplayMenu->AppendCheckItem(ID_HIDE_HOST_MOUSE,wxT("Hide Host Mouse Pointer"),wxT("Hides the host mouse pointer - may cause lag"));


    throttleMenu->AppendCheckItem(ID_THROTTLE5,       wxT("5 MHz")                  , wxT("5 MHz - Stock Lisa Speed, recommended.") );
    throttleMenu->AppendCheckItem(ID_THROTTLE8,       wxT("8 MHz")                  , wxT("8 MHz - Original Macintosh 128 Speed") );
    throttleMenu->AppendCheckItem(ID_THROTTLE10,      wxT("10 MHz")                 , wxT("10Mhz"));
    throttleMenu->AppendCheckItem(ID_THROTTLE12,      wxT("12 MHz")                 , wxT("12Mhz"));
    throttleMenu->AppendCheckItem(ID_THROTTLE16,      wxT("16 MHz")                 , wxT("16Mhz"));
    throttleMenu->AppendCheckItem(ID_THROTTLE32,      wxT("32 MHz")                 , wxT("32Mhz - For modern machines") );

    throttleMenu->AppendSeparator();

	throttleMenu->AppendCheckItem(ID_ET100_75,       wxT("Higher 68000 Performance"),     wxT("Normal 100/75ms duty timer - faster emulated CPU, less smooth animations"));
    throttleMenu->AppendCheckItem(ID_ET50_30,        wxT("Balanced 68000/Graphics"),      wxT("Medium  50/30ms duty timer - smoother animation, slower CPU"));
    throttleMenu->AppendCheckItem(ID_ET40_25,        wxT("Smoother Graphics"),            wxT("Small   40/25ms duty timer - even smoother animation, slower CPU"));
    throttleMenu->AppendCheckItem(ID_ET30_20,        wxT("Smoothest display, lowest 68000 Performance"),     wxT("Tiny    30/20ms duty timer - smoothest animation, slowest CPU"));


//    throttleMenu->AppendCheckItem(ID_THROTTLEX,  wxT("Unthrottled")            , wxT("Dangerous to your machine!") );


    // The "About" item should be in the help menu

    //helpMenu->Append(wxID_ABOUT, wxT("&License"),                 wxT("License Information"));
    //helpMenu->AppendSeparator();
    //helpMenu->Append(wxID_ABOUT, wxT("Help \tF1"),                wxT("Emulator Manual"));
    //helpMenu->Append(wxID_ABOUT, wxT("Lisa Help"),                wxT("How to use Lisa software"));
    //helpMenu->AppendSeparator();

	helpMenu->Append(wxID_ABOUT, wxT("&About LisaEm"),                   wxT("About the Lisa Emulator"));
	#ifndef __WXOSX__
	    helpMenu->AppendSeparator();
	#endif

    helpMenu->Append(ID_LISAWEB, wxT("Lisa Emulator Webpage"),    wxT("http://lisaem.sunder.net"));
    helpMenu->Append(ID_LISAFAQ, wxT("Lisa FAQ webpage"),         wxT("http://lisafaq.sunder.net"));


    keyMenu->Append(ID_POWERKEY,         wxT("Power Button"),     wxT("Push the Power Button"));
    keyMenu->Append(ID_APPLEPOWERKEY,    wxT("Apple+Power Button"),wxT("Push Apple + the Power Button"));
    keyMenu->AppendSeparator();

    keyMenu->Append(ID_KEY_APL_DOT,      wxT("Apple ."),          wxT("Apple + ."));

    keyMenu->Append(ID_KEY_APL_S,        wxT("Apple S"),          wxT("Apple + S"));
    keyMenu->Append(ID_KEY_APL_ENTER,    wxT("Apple Enter"),      wxT("Apple + Enter"));
    keyMenu->Append(ID_KEY_APL_RENTER,   wxT("Apple Right Enter"),wxT("Apple + Numpad Enter"));

    keyMenu->Append(ID_KEY_APL_1,        wxT("Apple 1"),          wxT("Apple + 1"));
    keyMenu->Append(ID_KEY_APL_2,        wxT("Apple 2"),          wxT("Apple + 2"));
    keyMenu->Append(ID_KEY_APL_3,        wxT("Apple 3"),          wxT("Apple + 3"));


    keyMenu->Append(ID_KEY_OPT_0,        wxT("Option 0"),         wxT("Option 0"));
    keyMenu->Append(ID_KEY_OPT_4,        wxT("Option 4"),         wxT("Option 4"));
    keyMenu->Append(ID_KEY_OPT_7,        wxT("Option 7"),         wxT("Option 7"));

    keyMenu->AppendSeparator();
    keyMenu->Append(ID_KEY_NMI,          wxT("NMI Key"),          wxT("Send Non-Maskable Interrupt key - for LisaBug"));
	keyMenu->Append(ID_KEY_RESET,        wxT("Reset Button"),     wxT("Reset the Lisa - use only if the running OS has crashed!"));

    keyMenu->AppendSeparator();
    keyMenu->AppendCheckItem(ID_ASCIIKB, wxT("ASCII Keyboard"),    wxT("Translate host keys into ASCII, then to Lisa keys (preferred)"));
    keyMenu->AppendCheckItem(ID_RAWKB,   wxT("Raw Keyboard"),      wxT("Map host keys to Lisa keys directly"));
    keyMenu->AppendCheckItem(ID_RAWKBBUF,wxT("Raw Buffered Keyboard"),wxT("Map host keys to Lisa keys directly, buffer to prevent repeats"));
    //not-yet-used//keyMenu->AppendSeparator();
    //not-yet-used//keyMenu->Append(ID_KEYBOARD,         wxT("Keyboard"),         wxT("Lisa Keyboard"));

    fileMenu->Append(wxID_OPEN,          wxT("Open Preferences"), wxT("Open a LisaEm Preferences file"));
    fileMenu->Append(wxID_SAVEAS,        wxT("Save Preferences As"), wxT("Save current LisaEm Preferences to a new file"));
    fileMenu->Append(wxID_PREFERENCES,   wxT("Preferences"),      wxT("Configure this Lisa"));

    fileMenu->AppendSeparator();

    fileMenu->AppendCheckItem(ID_RUN,    wxT("Run"), wxT("Run Emulation") );
    fileMenu->AppendCheckItem(ID_PAUSE,  wxT("Pause"), wxT("Pause Emulation") );

    fileMenu->AppendSeparator();

    fileMenu->Append(  ID_FLOPPY,        wxT("Insert diskette"),    wxT("Insert a disk image"));
    fileMenu->Append(ID_NewFLOPPY,       wxT("Insert blank diskette"),wxT("Create, and insert, a blank disk image"));
    fileMenu->AppendSeparator();
	fileMenu->Append(ID_PROFILE_NEW,     wxT("Create new Profile image"), wxT("Creates a blank ProFile storage file")  );
    fileMenu->AppendSeparator();

    fileMenu->Append(ID_SCREENSHOT,      wxT("Screenshot"),       wxT("Save the current screen as an image"));
    fileMenu->Append(ID_SCREENSHOT2,     wxT("Full Screenshot"),  wxT("Save a screenshot along with the skin"));
    fileMenu->Append(ID_SCREENSHOT3,     wxT("Raw Screenshot"),   wxT("Save a raw screenshot"));

    fileMenu->AppendSeparator();

    fileMenu->Append(ID_FUSH_PRNT,       wxT("Flush Print Jobs"),   wxT("Force pending print jobs to print"));

    fileMenu->AppendSeparator();

#ifndef __WXMSW__
#ifdef TRACE
    fileMenu->AppendCheckItem(ID_DEBUG,  wxT("Trace Log"),     wxT("Trace Log On/Off"));
    fileMenu->AppendSeparator();
#endif
#endif


    fileMenu->Append(wxID_EXIT,          wxT("Exit"),             wxT("Quit the Emulator"));

    // Now append the freshly created menu to the menu bar...

    menuBar = new wxMenuBar();

    menuBar->Append(fileMenu, wxT("File"));
    menuBar->Append(editMenu, wxT("Edit"));
    menuBar->Append(keyMenu,  wxT("Key"));
    menuBar->Append(DisplayMenu, wxT("Display"));
    menuBar->Append(throttleMenu, wxT("Throttle"));
    menuBar->Append(profileMenu, wxT("Parallel Port"));

    menuBar->Append(helpMenu, wxT("&Help"));


    SetMenuBar(menuBar);
	UpdateProfileMenu();

    CreateStatusBar(1);

    SetStatusText(wxT("Welcome to the Lisa Emulator Project!"));
    soundplaying=0;

    if (!my_lisawin) my_lisawin = new LisaWin(this);

    my_lisawin->doubley = 0;
    my_lisawin->dirtyscreen = 1;
    my_lisawin->brightness = 0;
    my_lisawin->refresh_bytemap = 1;

   if (skins_on)
      {
           int x,y;
           LoadImages();

           x=my_skin->GetWidth();
           y=my_skin->GetHeight();

           my_lisawin->SetMaxSize(wxSize(x,y));
           my_skinDC->SelectObject(*my_skin);

           my_lisawin->SetVirtualSize(x,y);
           my_lisawin->SetScrollbars(x/100, y/100,  100,100,  0,0,  true);
           my_lisawin->EnableScrolling(true,true);
      }
   else
      {
           int x,y;
           y=myConfig->Read(_T("/lisaframe/sizey"),(long)0);
           x=myConfig->Read(_T("/lisaframe/sizex"),(long)0);
           if (x<effective_lisa_vid_size_x || y<effective_lisa_vid_size_y )
              {x=effective_lisa_vid_size_x;   y=effective_lisa_vid_size_y;}
//           ALERT_LOG(0,"Setting frame size to:%d,%d",x,y);

	       SetClientSize(wxSize(x,y));															 // Frame    //
           GetClientSize(&x,&y);                                                                 // Frame    //
		   SetMinSize(wxSize(720,384));
           x+=WINXDIFF; y+=WINYDIFF;                                                             // LisaWin  //
           my_lisawin->SetClientSize(wxSize(x,y));
           my_lisawin->GetSize(&x,&y);                                                           // LisaWin  //
           // X11, Windows return too small a value for getsize
           x+=WINXDIFF; y+=WINYDIFF;                                                             // LisaWin  //

           my_lisawin->SetMaxSize(wxSize(ISKINSIZE));                                            // LisaWin  //
           my_lisawin->SetMinSize(wxSize(720,384));                                              // LisaWin  //
           my_lisawin->EnableScrolling(false,false);                                             // LisaWin  //
      }

   SendSizeEvent();
   my_lisawin->Show(true);
   fileMenu->Check(ID_PAUSE,false);
   fileMenu->Check(ID_RUN,  false);


   m_emulation_timer = new wxTimer(this, ID_EMULATION_TIMER);
   m_emulation_timer->Start(emulation_tick, wxTIMER_CONTINUOUS);
}


void LisaEmFrame::OnFlushPrint(wxCommandEvent& WXUNUSED(event))  {iw_enddocuments();iw_enddocuments();}


// copied/based on wxWidgets samples image.cpp (c) 1998-2005 Robert Roebling
void LisaEmFrame::OnScreenshot(wxCommandEvent& event)
{
 wxImage* image=NULL;

    wxString description;

    pause_run();

    if ( event.GetId()==ID_SCREENSHOT3)
         {
		   int updated=0;	
		   uint32 a3,xx;
		   uint16 val;
		   uint8  d;

		   uint8 bright[8];

  		   bright[0]=240;                                          
  		   bright[1]=240;                        
  		   bright[2]=240;               
           bright[4]=bright[5]=bright[6]=bright[7]=0;

           description = _T("Save RAW Screenshot");
           image = new class wxImage(lisa_vid_size_x, lisa_vid_size_y, true);

		   for ( int yi=0; yi < lisa_vid_size_y; yi++)
			    {
				   for ( int xi=0; xi < lisa_vid_size_x;)
					{ SETRGB16_RAW_X(xi,yi,{image->SetRGB(xi,yi,d,d,d+EXTRABLUE);});   }
			    }
         }
    else  if ( event.GetId()==ID_SCREENSHOT2 && skins_on)
         {
          description = _T("Save Screenshot with Skin");
          image = new class wxImage(my_skin->ConvertToImage());
         }
    else if ( event.GetId()==ID_SCREENSHOT ||( event.GetId()==ID_SCREENSHOT2 && !skins_on) )
         {
         
          if (!skins_on) image = new class wxImage(my_lisabitmap->ConvertToImage());
          else {
  			    wxBitmap   *bitmap=NULL;   
                wxMemoryDC *dc=NULL;
				dc=new class wxMemoryDC;
			    
			    bitmap=new class wxBitmap(effective_lisa_vid_size_x, effective_lisa_vid_size_y,DEPTH);
		        dc->SelectObject(*bitmap);

 		        dc->Blit(0,0, effective_lisa_vid_size_x, effective_lisa_vid_size_y,
	                      (skins_on ? my_skinDC:my_memDC), 
	                      screen_origin_x,screen_origin_y, wxCOPY, false);
				DEBUG_LOG(0,"converting to image");

                image = new class wxImage(bitmap->ConvertToImage());
				delete bitmap;
				delete dc;
               }
          description = _T("Save Screenshot");

         }
    else
         {
          resume_run();
		  return;
		 }


wxString filter;
#ifdef wxUSE_LIBPNG
                                        filter+=_T("PNG files (*.png)|");
                                        //filter+=_T("PNG files (*.png)|*.png|");
#endif
#ifdef wxUSE_LIBJPEG
                                        filter+=_T("JPEG files (*.jpg)|");
#endif
#ifdef wxUSE_LIBTIFF
                                        filter+=_T("TIFF files (*.tif)|");
#endif

// is this enabled, now that Unisys had the evil patent expire?
#ifdef wxUSE_LIBGIF
                                        filter+=_T("GIF files (*.gif)|");
#endif

#ifdef wxUSE_PCX
                                        filter+=_T("PCX files (*.pcx)|");
#endif
                                        filter+=_T("BMP files (*.bmp)|");

    wxString savefilename = wxFileSelector(   description,
                                            wxEmptyString,
                                            wxEmptyString,
                                            wxEmptyString,
                                                filter,
//#ifdef wxFD_SAVE
                                            wxFD_SAVE);
//#else
//                                            wxSAVE);
//#endif


    if ( savefilename.empty() )  {resume_run();  return; }

    wxString extension;
    wxFileName::SplitPath(savefilename, NULL, NULL, &extension);

    if ( extension == _T("bpp") )
    {
            image->SetOption(wxIMAGE_OPTION_BMP_FORMAT, wxBMP_1BPP_BW);
    }
    else if ( extension == _T("png") )
    {

       if ( event.GetId()==ID_SCREENSHOT2 && skins_on)
          {
            image->SetOption(wxIMAGE_OPTION_PNG_FORMAT, wxPNG_TYPE_COLOUR );
          }
       else
          {
            image->SetOption(wxIMAGE_OPTION_PNG_FORMAT, wxPNG_TYPE_GREY);
            image->SetOption(wxIMAGE_OPTION_PNG_BITDEPTH, 8);
          }
    }

   image->SaveFile(savefilename);
   resume_run();
}

/* Connects a Dual Parallel port card to the specified slot (numbered 0-2). */
void connect_2x_parallel_to_slot(int slot)
{
	
//	ALERT_LOG(0,"Connecting Dual Parallel card to slot %d",slot+1);
	
    int low, high, v;
    switch (slot)
    {
        case 0:
            get_exs0_pending_irq = get_exs0_pending_irq_2xpar;
            low = Ox0000_slot1; high = Ox2000_slot1; v = 3;       // vias 3,4
            break;
        case 1:
            get_exs1_pending_irq = get_exs1_pending_irq_2xpar;
            low = Ox4000_slot2; high = Ox6000_slot2; v = 5;       // vias 5,6
            break;
        case 2:
            get_exs2_pending_irq = get_exs2_pending_irq_2xpar;
            low = Ox8000_slot3; high = Oxa000_slot3; v = 7;       // vias 7,8
            break;
        default:
           //ALERT_LOG(0, "Unknown slot number %d, should be 0-2!", slot);
           return;
    }

    mem68k_memptr    [low] = lisa_mptr_2x_parallel_l;
    mem68k_fetch_byte[low] = lisa_rb_2x_parallel_l;
    mem68k_fetch_word[low] = lisa_rw_2x_parallel_l;
    mem68k_fetch_long[low] = lisa_rl_2x_parallel_l;
    mem68k_store_byte[low] = lisa_wb_2x_parallel_l;
    mem68k_store_word[low] = lisa_ww_2x_parallel_l;
    mem68k_store_long[low] = lisa_wl_2x_parallel_l;

    mem68k_memptr    [high] = lisa_mptr_2x_parallel_h;
    mem68k_fetch_byte[high] = lisa_rb_2x_parallel_h;
    mem68k_fetch_word[high] = lisa_rw_2x_parallel_h;
    mem68k_fetch_long[high] = lisa_rl_2x_parallel_h;
    mem68k_store_byte[high] = lisa_wb_2x_parallel_h;
    mem68k_store_word[high] = lisa_ww_2x_parallel_h;
    mem68k_store_long[high] = lisa_wl_2x_parallel_h;

    reset_via(v);
    reset_via(v+1);
    via[v].active = 1;
    via[v+1].active = 1;
}

/* Connects a printer/profile to the specified VIA */
void connect_device_to_via(int v, wxString device, wxString *file)
{
	char tmp[MAXPATHLEN];

//    if (v==2) {ALERT_LOG(0,"Connecting %s filename:%s to VIA #%d (motherboard parallel port)",device.c_str(),file->c_str(), v );}
// 	  else      {ALERT_LOG(0,"Connecting %s filename:%s to VIA #%d (slot #%d %s)",device.c_str(),file->c_str(), v,1+((v-2)/2), ((v&1) ? "upper":"lower") );}

    if (device.IsSameAs(_T("ADMP"), false))
    {
        via[v].ADMP = v;
        ImageWriter_LisaEm_Init(v);                 // &ADMP,pcl,ps,xlib
        DEBUG_LOG(0, "Attached ADMP to VIA#%d", v);
        return;
	} else via[v].ADMP=0;


    if (device.IsSameAs(_T("PROFILE"), false))
    {
        if (file->Len()==0)
        {
         static const wxString def[]={  _T("null"),_T("COPS"),
                            _T("lisaem-profile.dc42"),
                            _T("lisaem-s1h-profile.dc42"),
                            _T("lisaem-s1l-profile.dc42"),
                            _T("lisaem-s2h-profile.dc42"),
                            _T("lisaem-s2l-profile.dc42"),
                            _T("lisaem-s3h-profile.dc42"),
                            _T("lisaem-s3l-profile.dc42") };

         if (v>1 && v<9) *file =def[v];
         else return;
        }

		strncpy(tmp,(file->fn_str()),MAXPATHLEN);
	    ALERT_LOG(0, "Attempting to attach VIA#%d to profile %s", v, tmp);
        via[v].ProFile = (ProFileType *)malloc(sizeof(ProFileType));
        int i = profile_mount(tmp, via[v].ProFile);
        if (i) {
            free(via[v].ProFile);
            via[v].ProFile = NULL;
            ALERT_LOG(0, "Couldn't get profile because: %d",i);
        } else {
			via[v].ProFile->vianum=v;
            ProfileReset(via[v].ProFile);
        }
    }
}

/*
 * Connect some (virtual) device to one of the serial ports. 0 is Serial A,
 * 1 is Serial B. A number of the parameters passed in may be modified by
 * this code.
 */
void connect_device_to_serial(int port, FILE **scc_port_F, uint8 *serial,
    wxString *setting, wxString *param, int *scc_telnet_port)
{
	char cstr_param[MAXPATHLEN];
	strncpy(cstr_param,param->fn_str(),MAXPATHLEN);
	
    if (port !=0 && port != 1) {
        DEBUG_LOG(0, "Warning Serial port is not A/B: %d", port);
        return;
    }

    if (setting->IsSameAs(_T("LOOPBACK"),false)) {
        *scc_port_F = NULL;
        *serial = SCC_LOOPBACKPLUG;
        return;
    }

    if (setting->IsSameAs(_T("NOTHING"), false) || setting->IsSameAs(_T("NULL"), false) || setting->Len()==0) {
        *scc_port_F = NULL;
        *serial = SCC_NOTHING;
        *setting = _T("NOTHING");  // fill in for writing into ini file
        return;
    }

    if (setting->IsSameAs(_T("FILE"), false))
    {
        *scc_port_F = fopen(cstr_param, "r+b");
        if (!*scc_port_F) {
            wxString err = wxString();
            err.Printf(_T("Could not map Serial port %c to file %s"), (port==0?'A':'B'), cstr_param);
            wxMessageBox(err, _T("Serial port configuration"),  wxICON_INFORMATION | wxOK);
            *scc_port_F = NULL;
            *serial = SCC_NOTHING;
        } else {
            *serial = SCC_FILE;
        }
        return;
    }

    if (setting->IsSameAs(_T("PIPE"), false)) {
        *scc_port_F = popen(cstr_param, "r+b");
        if (!*scc_port_F) {
            wxString err = wxString();
            err.Printf(_T("Could not map Serial port %c to pipe %s"), (port==0?'A':'B'), cstr_param);
            wxMessageBox(err, _T("Serial port configuration"),  wxICON_INFORMATION | wxOK);
            *scc_port_F = NULL;
            *serial = SCC_NOTHING;
        } else {
            *serial = SCC_PIPE;
        }
        return;
    }

#ifndef __MSVCRT__
    if (setting->IsSameAs(_T("TELNETD"), false)) {
        unsigned long x;
        *scc_port_F = NULL;
        *serial = SCC_TELNETD;

        *scc_telnet_port = (param->ToULong(&x, 10)==false) ? 9300+port : x;
        ALERT_LOG(0,"Connecting TELNETD 127.0.0.1:%d on serial port %d",*scc_telnet_port,port);

        init_telnet_serial_port(!port);     // serial a=port 1, serial b=port 0 (reversed from config!)
        ALERT_LOG(0,"return from init_telnet_serial_port %d",port);

        return;
    }
#endif

    if (setting->IsSameAs(_T("IMAGEWRITER"), false)) {
        *scc_port_F = NULL;
        *serial = SCC_IMAGEWRITER_PS;
        if (port) scc_b_IW=1; else scc_a_IW=0;
        ImageWriter_LisaEm_Init(port);
        return;
    }

	strncpy(cstr_param,setting->fn_str(),MAXPATHLEN);
    DEBUG_LOG(0, "Warning: unrecognised Serial %c setting '%s'", port==0?'A':'B', cstr_param);
}

int romlessboot_pick(void)
{
 int r;
 wxString choices[]={
	                     wxT( "ProFile Hard Drive on Parallel Port"),
                         wxT( "Floppy Diskette")
                    };

 wxString txt;
 wxSingleChoiceDialog *d=NULL;
 txt.Printf(_T("Which device should the virtual Lisa boot from?"));

 // if the parallel port device is a profile, offer a choice between the floppy and the profile
 if ( my_lisaconfig->parallel.IsSameAs(_T("PROFILE"), false) )  
 {

 d=new wxSingleChoiceDialog(my_lisaframe,
                        txt,
                        wxT("ROMless Boot"),
                        2,             choices);

 if (d->ShowModal()==wxID_CANCEL) {delete d; return -1;}
 r=d->GetSelection();
 }
 else r=1;  // if not a profile, just boot from the floppy drive.

 if (r==1)   // if there's no floppy inserted, ask for one.
 {
      if (!my_lisaframe->floppy_to_insert.Len()) my_lisaframe->OnxFLOPPY();
      if (!my_lisaframe->floppy_to_insert.Len()) return -1;  // if the user hasn't picked a floppy, abort.
      const wxCharBuffer s = my_lisaframe->floppy_to_insert.fn_str();
      int i=floppy_insert((char *)(const char *)s);
      wxMilliSleep(100);  // wait for insert sound to complete to prevent crash.
      if (i) { eject_floppy_animation(); return -1;}
      my_lisaframe->floppy_to_insert=_T("");
 }

 delete d;
 return r;
}


 int initialize_all_subsystems(void)
 {
   /*
   In the beginning there was data.  The data was without form and null,
   and darkness was upon the face of the console; and the Spirit of IBM
   was moving over the face of the market.  And DEC said, "Let there be
   registers"; and there were registers.  And DEC saw that they carried;
   and DEC separated the data from the instructions.  DEC called the data
   Stack, and the instructions they called Code.  And there was evening
   and there was morning, one interrupt.         -- Rico Tudor            */

   int pickprofile=-1;
   char tmp[MAXPATHLEN];

       if (my_lisaframe->running) {ALERT_LOG(0,"Already running!"); 
                                   return 0;}
       buglog=stderr;

       setstatusbar("initializing all subsystems...");

       // initialize parity array (it's 2x faster to use an array than to call the fn)
       setstatusbar("Initializing Parity calculation cache array...");
       {uint16 i;
       for (i=0; i<256; i++) eparity[i]=evenparity((uint8) i);
       }

       segment1=0; segment2=0; start=1;

       // needs to be above read_config_files
       scc_a_port=NULL;
       scc_b_port=NULL;
       scc_a_IW=-1;
       scc_b_IW=-1;
       serial_a=0;
       serial_b=0;

       init_floppy(my_lisaconfig->iorom);

       bitdepth=8;                             // have to get this from the X Server side...


       DEBUG_LOG(0,"serial number");
       serialnum[0]=0x00;                      // "real" ones will be loaded from the settings file
       serialnum[1]=0x00;
       serialnum[2]=0x00;
       serialnum[3]=0x00;
       serialnum[4]=0x00;
       serialnum[5]=0x00;
       serialnum[6]=0x00;
       serialnum[7]=0x00;
       serialnumshiftcount=0; serialnumshift=0;


       // Is there no explicit way to init the COPS?
       DEBUG_LOG(0,"copsqueue");
       init_cops();


       setstatusbar("Initializing irq handlers");
       init_IRQ();


       setstatusbar("Initializing vias");
       init_vias();


       setstatusbar("Initializing profile hd's");
       init_Profiles();


       setstatusbar("Initializing Lisa RAM");
       TWOMEGMLIM=0x001fffff;

      // Simulate physical Lisa memory boards
      switch(my_lisaconfig->mymaxlisaram)
      {
       case 512  : maxlisaram=0x100000;  minlisaram=0x080000;  break;     // single 512KB board in slot 1
       case 1024 : maxlisaram=0x180000;  minlisaram=0x080000;  break;     // two 512KB boards in slot1, slot 2
       case 1536 : maxlisaram=0x200000;  minlisaram=0x080000;  break;     // 512KB board in slot1, 1024KB board in slot 2
       default:    maxlisaram=0x200000;  minlisaram=0x000000;  break;     // two 1024KB boards in slot 1,2
      }

       // maxlisaram! New!  And! Improved! Now available in bytes at a register Store Near You!  OpCodes are Standing by!
       // Act now!  don't delay!  Limited Time Offer!  JSR Now!  Restrictions Apply!  Before engaging in any strenous programming
       // activity, you should always consult your MMU today!  Not 0xFD1C insured!


      TWOMEGMLIM=maxlisaram-1;
   videolatchaddress=maxlisaram-0x10000;

   videolatch=(maxlisaram>>15)-1; // last page of ram is by default what the video latch gets set to.
   lastvideolatch=videolatch;  lastvideolatchaddress=videolatchaddress;

   if (lisaram) free(lisaram);          // remove old junk if it exists

   lisaram=(uint8 *)malloc(maxlisaram+512);     // added 512 bytes at the end to avoid overflows.
   if (!lisaram) {
       wxMessageBox(_T("Could not allocate memory for the Lisa to use."),
                    _T("Out of Memory!"), wxICON_INFORMATION | wxOK);
       return 23;
   }

   memset(lisaram,0xff,maxlisaram+512);

   if (my_lisaconfig->kbid) set_keyboard_id(my_lisaconfig->kbid);  else set_keyboard_id(-1);

    setstatusbar("Initializing Lisa Serial Number");
    // Parse sn and correct if needed  ////////////////////////////////////////////////////////////////////////////////////////
	{

		char cstr[32];
        //char mybuffer[5];
        int i,j; char c;
		strncpy(cstr,my_lisaconfig->myserial.fn_str(),32);
		char *s=cstr;
        for (i=0; (c=s[i]); i++)  {if ( !ishex(c) ) my_lisaconfig->myserial.Printf(_T("%s"),LISA_CONFIG_DEFAULTSERIAL); break;}

        if (i<31) DEBUG_LOG(1,"Warning: serial # less than 32 bytes (%d)!\n",i);

        for (i=0,j=0; i<32; i+=2,j++)
                serialnum240[j]=(gethex(s[i])<<4)|gethex(s[i|1]);

        vidfixromchk(serialnum240);

        my_lisaconfig->myserial.Printf(_T("%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"),
                        serialnum240[0],                 serialnum240[1],
                        serialnum240[2],                 serialnum240[3],
                        serialnum240[4],                 serialnum240[5],
                        serialnum240[6],                 serialnum240[7],
                        serialnum240[8],                 serialnum240[9],
                        serialnum240[10],                serialnum240[11],
                        serialnum240[12],                serialnum240[13],
                        serialnum240[14],                serialnum240[15]);

//        ALERT_LOG(0,"Serial # used: %s", my_lisaconfig->myserial.c_str());
                                  //ff028308104050ff 0010163504700000

    }//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    setstatusbar("Initializing Serial Ports");

    // If either serial port is set as loopback, both must be.
    if (my_lisaconfig->serial2_setting.IsSameAs(_T("LOOPBACK"), false) ||
        my_lisaconfig->serial1_setting.IsSameAs(_T("LOOPBACK"), false)) 
        {
          my_lisaconfig->serial1_setting = _T("LOOPBACK");
          my_lisaconfig->serial2_setting = _T("LOOPBACK");
        }


    connect_device_to_serial(0, &scc_a_port_F, &serial_a,
        &my_lisaconfig->serial1_setting, &my_lisaconfig->serial1_param,
        &scc_a_telnet_port);


    connect_device_to_serial(1, &scc_b_port_F, &serial_b,
        &my_lisaconfig->serial2_setting, &my_lisaconfig->serial2_param,
        &scc_b_telnet_port);

    DEBUG_LOG(0,"Initializing SCC Z8530");
    initialize_scc();

  DEBUG_LOG(0,"setting motherboard latches");
  softmemerror=0; harderror=0; videoirq=0; bustimeout=0; videobit=0;

  DEBUG_LOG(0,"Checking host CPU bit order sanity.");
  reg68k_sanity_check_bitorder();

  if (sizeof(XTIMER)<8) { wxMessageBox(_T("XTIMER isn't int64!."),
									   _T("Danger!"), wxICON_INFORMATION | wxOK);  return 24; }



  setstatusbar("Initializing Generator CPU functions.");
  cpu68k_init();

  setstatusbar("Initializing MMU");
  init_lisa_mmu();

  setstatusbar("Initializing Generator CPU IPC Allocator");
  init_ipct_allocator();

  setstatusbar("Initializing Lisa Boot ROM");
  
  DEBUG_LOG(0,"Loading Lisa ROM");
  strncpy(tmp,my_lisaconfig->rompath.fn_str(),MAXPATHLEN);
  if      (read_dtc_rom(tmp,   lisarom)==0)
           {
            if (checkromchksum())
               {
                if (!yesnomessagebox("BOOT ROM checksum doesn't match, this may crash the emulator.  Continue anyway?",
                                     "ROM Checksum mismatch"))   return -2;
               }
             DEBUG_LOG(0,"Load of DTC assembled ROM successful");
             fixromchk();
           }
  else if (read_split_rom(tmp, lisarom)==0)
           {
            if (checkromchksum())
               {
                if (!yesnomessagebox("BOOT ROM checksum doesn't match, this may crash the emulator.  Continue anyway?",
                                     "ROM Checksum mismatch")  ) return -2;
               }
  
              DEBUG_LOG(0,"Load of split ROM");
              fixromchk();
           }
  else if (read_rom(tmp,       lisarom)==0)
           {
            if (checkromchksum())
               {
                if (!yesnomessagebox("BOOT ROM checksum doesn't match, this may crash the emulator.  Continue anyway?",
                                     "ROM Checksum mismatch")  ) return -2;
               }
  
              DEBUG_LOG(0,"Load of normal ROM successful");
              fixromchk();
           }
  else  
       {
         romless=1;
         pickprofile=romlessboot_pick(); if (pickprofile<0) return -2;
		 ALERT_LOG(0,"picked %d",pickprofile);
       }
  
  setstatusbar("Initializing Lisa Display");
  if (has_xl_screenmod())
  {

   // This warning flag isn't really a configuration setting as such, but leave it here for the moment
   // the purpose of the flag is to prevent nagging the user.  So we only show the 3A warning the first time
   if (!my_lisaconfig->saw_3a_warning)
   wxMessageBox(_T("This is the XL Screen Modification ROM! You will only be able to run MacWorks."),
                    _T("3A ROM!"),   wxICON_INFORMATION | wxOK);


   my_lisaconfig->saw_3a_warning=1;
   lisa_vid_size_x=608;
   lisa_vid_size_y=431;
   effective_lisa_vid_size_x=608;
   effective_lisa_vid_size_y=431;

   screen_origin_x=140+56;
   screen_origin_y=130+34;

   lisa_vid_size_x=608;
   lisa_vid_size_y=431;

   lisa_vid_size_xbytes=76;
   has_lisa_xl_screenmod=1;

   setvideomode(0x3a);
  }
  else
  {
   lisa_vid_size_x=720;
   lisa_vid_size_y=364;
   effective_lisa_vid_size_x=720;
   effective_lisa_vid_size_y=500;

   screen_origin_x=140;
   screen_origin_y=130;

   lisa_vid_size_x=720;
   lisa_vid_size_y=364;


   lisa_vid_size_xbytes=90;
   has_lisa_xl_screenmod=0;

   setvideomode(lisa_ui_video_mode);


  }

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  setstatusbar("Initializing Parallel Port");
  // Handle Parallel Ports

  // Connect profile/printer to builtin parallel port
  connect_device_to_via(2, my_lisaconfig->parallel, &my_lisaconfig->parallelp);

  // ----------------------------------------------------------------------------------

  // Dual Parallel Port Expansion Cards

  setstatusbar("Initializing Expansion Card Slots");
  memset(dualparallelrom,0xff,2048);

  strncpy(tmp,my_lisaconfig->dualrom.fn_str(),MAXPATHLEN);
  if (read_parallel_card_rom(tmp)==0)
     {
  	    ALERT_LOG(0,"Connecting Dual Parallel Port Cards.");
  
        if (my_lisaconfig->slot1.IsSameAs(_T("dualparallel"),false) )
          {
  			    ALERT_LOG(0,"Connecting slot 1");
                  connect_2x_parallel_to_slot(0);
                  connect_device_to_via(3, my_lisaconfig->s1h, &my_lisaconfig->s1hp);
                  connect_device_to_via(4, my_lisaconfig->s1l, &my_lisaconfig->s1lp);
          }
  
        if (my_lisaconfig->slot2.IsSameAs(_T("dualparallel"),false) )
          {
  		        ALERT_LOG(0,"Connecting slot 2");
                  connect_2x_parallel_to_slot(1);
                  connect_device_to_via(5, my_lisaconfig->s2h, &my_lisaconfig->s2hp);
                  connect_device_to_via(6, my_lisaconfig->s2l, &my_lisaconfig->s2lp);
          }
  
  
        if (my_lisaconfig->slot3.IsSameAs(_T("dualparallel"),false))
          {
  		        ALERT_LOG(0,"Connecting slot 3");
                  connect_2x_parallel_to_slot(2);
                  connect_device_to_via(7, my_lisaconfig->s3h, &my_lisaconfig->s3hp);
                  connect_device_to_via(8, my_lisaconfig->s3l, &my_lisaconfig->s3lp);
          }
  
  
     }


    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    ALERT_LOG(0,"initializing video");
    disable_vidram(); videoramdirty=32768;

    ALERT_LOG(0,"Calling Redraw Pixels");
    LisaRedrawPixels();

    setstatusbar("Initializing CPU Registers");
    cpu68k_reset();

    ALERT_LOG(0,"done initializing...");

//    debug_off();

    setstatusbar("Executing Lisa Boot ROM");
    refresh_rate_used=refresh_rate;       // powering (back) on, so use the user selected refresh rate.
    my_lisaframe->reset_throttle_clock();
    my_lisawin->Refresh(false,NULL); //
    // needs to be at the end since romless_boot sets up registers which get whacked by the cpu initialization

    if (romless)  
       {
           if (pickprofile) wxMilliSleep(1000); 
		   if (romless_boot(pickprofile) ) return 1; // failed

//         // has to be here since romless_boot overwrites low memory
  		   storeword(0x298, my_lisaconfig->slot1.IsSameAs(_T("dualparallel"),false) ? (dualparallelrom[0]<<8)|dualparallelrom[1]  : 0);
 		   storeword(0x29a, my_lisaconfig->slot2.IsSameAs(_T("dualparallel"),false) ? (dualparallelrom[0]<<8)|dualparallelrom[1]  : 0);
 		   storeword(0x29c, my_lisaconfig->slot3.IsSameAs(_T("dualparallel"),false) ? (dualparallelrom[0]<<8)|dualparallelrom[1]  : 0);
      }

    return 0;
} ///////////////////// end of load.///////////////////



 // just to be a bastard!  why? because split roms are a lame result of dumping ROMs from a ROM reader.
 // otherwise we have to merge it every time we boot up. :-)
 extern "C" void rename_rompath(char *rompath)
 {
   if (!my_lisaconfig)  return;

   my_lisaconfig->rompath=wxString(rompath, wxConvLocal, 2048); //wxSTRING_MAXLEN);
   my_lisaconfig->Save(pConfig, floppy_ram);

   if (my_LisaConfigFrame) my_LisaConfigFrame->m_rompath->SetValue(my_lisaconfig->rompath);
   DEBUG_LOG(0,"saved %s as default\n",rompath);
 }

extern "C" void force_refresh(void)                    //20060202.;
{
            my_lisaframe->Refresh(false,NULL);
           // my_lisaframe->Update();
}

extern "C" void save_pram(void)
{
 my_lisaconfig->Save(pConfig, floppy_ram);// save it so defaults are created
}

void invalidate_configframe(void) {my_LisaConfigFrame=NULL;}


extern "C" void save_configs(void)
{
  my_lisaconfig->Save(pConfig, floppy_ram);// save it so defaults are created
  save_global_prefs();
}


            //               0     1     2     3     4     5     6
            //              5M   10M   16M   20M   32M   40M    64M
extern "C" int pickprofilesize(char *filename)
{
 wxString choices[]={    wxT( "5M - any OS"),
                         wxT("10M - any OS"),
                         wxT("16M - MacWorks only"),
                         wxT("20M - MacWorks only"),
                         wxT("32M - MacWorks only"),
                         wxT("40M - MacWorks only"),
                         wxT("64M - MacWorks only") };

 wxSingleChoiceDialog *d;
 
 wxString txt=wxString(filename, wxConvLocal, 2048); //wxSTRING_MAXLEN);

 txt+=_T(" does not yet exist.  What size drive should I create?");

 d=new wxSingleChoiceDialog(my_lisaframe,
                           txt,
                           _T("Hard Drive Size?"),
                           7,             choices);

 if (d->ShowModal()==wxID_CANCEL) {delete d; return -1;}
 
 int r=d->GetSelection();
 delete d;
 return r;
}




// bridge to LisaConfigFrame to let it know what config file is opened.
wxString get_config_filename(void)  { return myconfigfile;}


void turn_skins_on(void)   {
                             skins_on_next_run=1;
                             skins_on=1;
                             save_global_prefs();
                             my_lisaframe->LoadImages();
                             setvideomode(lisa_ui_video_mode);
                             black();
                             screen_origin_x=140;
                             screen_origin_y=130;
                           }

void turn_skins_off(void)  {
                             skins_on_next_run=0;
                             skins_on=0;
                             save_global_prefs();
                             my_lisaframe->UnloadImages();
                             setvideomode(lisa_ui_video_mode);
                             black();
							}



extern "C" void contrastchange(void) {my_lisawin->ContrastChange();}

void setvideomode(int mode) {my_lisawin->SetVideoMode(mode);}



//----- ImageWriter interfaces to old C code. -----//



#include "imagewriter-wx.h"

ImageWriter *imagewriter[10];           // two serial ports, one parallel, 6 max dual parallel port cards, 9 possible


extern "C" int ImageWriter_LisaEm_Init(int iwnum)
{
 if (iwnum>10) return -1;
 if (imagewriter[iwnum]!=NULL) return 0; // already built, reuse it;
 imagewriter[iwnum]=new ImageWriter(my_lisaframe,
                                    my_lisaconfig->iw_png_on,
                                    my_lisaconfig->iw_png_path,
                                    my_lisaconfig->iw_dipsw_1);

 imagewriter[iwnum]->iwid=iwnum;         // set printer ID

// imagewriter[iwnum]->test(); // spit out a set of test pages.
 
 return 0;
}

extern "C" void iw_shutdown(void)
{
  int i;
  for (i=0; i<10; i++) if (imagewriter[i]!=NULL) {delete imagewriter[i]; imagewriter[i]=NULL;}
}

extern "C" void iw_formfeed(int iw)              {if (iw<10 && iw>-1 && imagewriter[iw]) imagewriter[iw]->iw_formfeed();     }
extern "C" void ImageWriterLoop(int iw,uint8 c)  {if (iw<10 && iw>-1 && imagewriter[iw]) imagewriter[iw]->ImageWriterLoop(c);}
extern "C" void iw_enddocument(int i)
{
  if (i<0 || i>10) return;
  if (imagewriter[i]!=NULL) {imagewriter[i]->EndDocument();}
}

extern "C" void iw_enddocuments(void)
{
  int i;
  for (i=2; i<10; i++) 
      if (imagewriter[i]!=NULL) 
	       imagewriter[i]->EndDocument(); 

  for (i=2; i<10; i++) via[i].last_pa_write=-1;

 }

// if any printer job has lingered for more than X seconds, flush that printer.
void iw_check_finish_job(void)
{
  int i;
  for (i=2; i<10; i++) 
	  if (imagewriter[i]!=NULL) 
       {
	    if ((cpu68k_clocks - via[i].last_pa_write > FIFTEEN_SECONDS) || via[i].last_pa_write==-1)
	       {
			 #ifdef DEBUG
			 ALERT_LOG(0,"No activity on printer #%d - flushing page",i);
			 #endif
	         via[i].last_pa_write=-1;
	         imagewriter[i]->EndDocument();  // ensure no stray data left queued up
	       }
       }
}



void uninit_gui(void);

// from generator
unsigned int gen_quit = 0;
unsigned int gen_debugmode = 1;






#ifdef DEBUG
void    dumpallmmu(void);
#endif
void    dumpvia(void);

/////------------------------------------------------------------------------------------------------------------------------


void banner(void)
{

 //         .........1.........2.........3.........4.........5.........6.........7
 //         123456789012345678901234567890123456789012345678901234567890123456789012345678
    printf("\n\n");
    printf("-----------------------------------------------------------------------\n");
#ifdef VERSION
    printf("  The Lisa II Emulator   %-17s   http://lisaem.sunder.net  \n",my_version);
#else
    printf("  The Lisa II Emulator   %-17s   http://lisaem.sunder.net  \n","UNKNOWN");
#endif
    printf("  -------------------------------------------------------------------  \n");
    printf("  Copyright  (C)   MMVII  by Ray A. Arachelian,   All Rights Reserved  \n");
    printf("  Released  under  the terms of  the  GNU Public License  Version 2.0  \n");
    printf("  -------------------------------------------------------------------  \n");
    printf("  For historical/archival/educational use only - no warranty provied.  \n");
    printf("  -------------------------------------------------------------------  \n");
    printf("  Portions of this software contain bits of code from the following:   \n");
    printf("  generator - www.squish.net/generator  wxWidgets.org  - www.ijg.org   \n");
    printf("   Many thanks to David T. Craig for the tons of Lisa documentation    \n");
    printf("     Many thanks to James McPhail for Lisa and 68000 hardware help     \n");
	printf("      Many thanks to Brain Folley for the OS X UI help                 \n");
    printf("-----------------------------------------------------------------------\n\n");

#ifdef DEBUG
    printf("\nDEBUG is compiled in.\n");
    fprintf(buglog,"\nDEBUG is compiled in.\n");
#endif

#ifdef DEBUGMEMCALLS
    printf("\nDEBUGMEMCALLS is enabled.\n");
    fprintf(buglog,"\nDEBUGMEMCALLS is enabled.\n");
    ALERT_LOG(0,"DEBUGMEMCALLS is on.");
#endif
}



//uint8 evenparity(uint8 data)          // naive way
//{ return  !( (   (data & 0x01) ? 1:0) ^
//             (   (data & 0x02) ? 1:0) ^
//             (   (data & 0x04) ? 1:0) ^
//             (   (data & 0x08) ? 1:0) ^
//             (   (data & 0x10) ? 1:0) ^
//             (   (data & 0x20) ? 1:0) ^
//             (   (data & 0x40) ? 1:0) ^
//             (   (data & 0x80) ? 1:0)   );}
//

uint8 evenparity(uint8 data)            // from http://graphics.stanford.edu/~seander/bithacks.html#ParityParallel
{
 uint32 v=data;
//v ^= v >> 16;       // not needed since we're working on a byte - only here for completeness, but commented out
//v ^= v >> 8;        // ditto
  v ^= v >> 4;
  v &= 0xf;
  return !(  (0x6996 >> v) & 1  );
}

uint8 oddparity(uint8 data)            // from BitTwiddling hacks.
{
 uint32 v=data;

 v ^= v >> 4;
 v &= 0xf;

 return (  (0x6996 >> v) & 1  );
}




// This redraws the lisa display ram into the Ximage
// If you are porting to another OS, this is one of
// the functions to replace.  This is slow, so it should be called as rarely as possible.
// i.e. only when the Lisa switches video screens by changing videolatch

extern "C" void XLLisaRedrawPixels(void);          // proto to supress warning below
extern "C" void LisaRedrawPixels(void);


extern "C" void LisaRedrawPixels(void)
{
 my_lisawin->repaintall |= REPAINT_VIDEO_TO_SKIN;
 my_lisawin->Refresh(false,NULL);
 //my_lisawin->Update();
}

#ifdef __cplusplus
extern "C"
#endif
void LisaScreenRefresh(void)
{
 LisaRedrawPixels();
}



static float normalthrottle=0;

extern "C" void sound_off(void)
{
	// restore throttle
	if (normalthrottle!=0)
	{my_lisaframe->throttle=normalthrottle; updateThrottleMenus(my_lisaframe->throttle);}

    if (cpu68k_clocks-my_lisaframe->lastclk<50000) return;  // prevent sound from shutting down immediately
    wxSound::Stop();
}



extern "C" void sound_play(uint16 t2)
{

    // temporarily slow down CPU durring beeps so that they're fully played.
	normalthrottle=my_lisaframe->throttle; updateThrottleMenus(5.0);


    int samples=22050*2;                // a second


    int data_size=0;
    int cycles, halfcycles;
    unsigned char vhigh, vlow; //, l;


    if (t2>0xaf) return;                                  // out of range
    if (my_lisaframe->lastt2==t2 && (cpu68k_clocks-my_lisaframe->lastclk < 50000)) return;  // duplicate call if timing <1/100th of a second

    my_lisaframe->lastt2=t2;
    my_lisaframe->lastclk=cpu68k_clocks;


    static const unsigned char header[45]=
    {
        // 0     1    2    3    4    5    6    7    8    9    a    b    c    d    e    f
/*00*/  0x52,0x49,0x46,0x46,   0,   0,0x00,0x00,0x57,0x41,0x56,0x45,0x66,0x6D,0x74,0x20,  // RIFFF-..WAVEfmt
/*10*/  0x10,0x00,0x00,0x00,   1,   0,0x01,0x00,0x22,0x56,0x00,0x00,0x22,0x56,0x00,0x00,  //  ........"V.."V..
/*20*/  0x01,0x00,0x08,0x00,0x64,0x61,0x74,0x61,0x22,0x2D,0x00,0x00//0x80,0x80,0x80,0x80  //  ....data"-......
    };           //        d    a    t    a [LSB____________MSB]

    static unsigned char data[22050*5+45+1024];
    unsigned char *dataptr;
    //data=(unsigned char *)malloc(45+samples+1024);  // allocate a buffer, make it 22Kbps
    memcpy(data,header,45);        // copy the header over it.


    data[0x16]=1;      // 1=1 channel = mono, 2= 2 channels = stereo
    data[0x17]=0;


    data[0x18]=0x22;   // bit rate LSB.   0x5622=22050Hz   22Khz
    data[0x19]=0x56;   //                 0x2D22=11554Hz   11Khz
    data[0x1a]=0x00;   //                 0xAC44=44100Hz   44Khz
    data[0x1b]=0x00;   // bit rate MSB


    data[0x1c]=0x22;   // bytes per second
    data[0x1d]=0x56;   // should match bit rate for 8 bit samples
    data[0x1e]=0x00;
    data[0x1f]=0x00;

    data[0x20]=1;      // Bytes Per Sample:
    data[0x21]=0;      // 1=8 bit Mono, 2=8 bit Stereo or 16 bit Mono, 4=16 bit Stereo

    data[0x22]=8;      // 8, or 16  bits/sample
    data[0x23]=0;

    dataptr=&data[0x2c]; data_size=0;

    volume=(volume & 0x0f);

    vlow =128-(volume<<3);
    vhigh=128+(volume<<3);

    cycles=t2;
    if (!(my_lisaconfig->iorom & 0x20)) cycles=cycles*8/10;       // cycles *=0.8
    cycles=(cycles>>1)+(cycles>>2);                // cycles *=0.75; (1/2 + 1/4th)
    halfcycles=cycles>>1;                          // cycles /=2;

    int i=0;
    if (t2>0x90) samples=cycles;                   // prevent click from turning into beep
    while (samples>=cycles)
    {
        i++;
     memset(dataptr,vhigh,halfcycles); dataptr+=halfcycles;
     memset(dataptr,vlow, halfcycles); dataptr+=halfcycles;
     samples-=cycles; data_size+=cycles;
    }

    data[0x28]=((data_size)    ) & 0xff;
    data[0x29]=((data_size)>>8 ) & 0xff;
    data[0x2a]=((data_size)>>16) & 0xff;
    data[0x2b]=((data_size)>>24) & 0xff;

    data[0x04]=((data_size+36)    ) & 0xff;
    data[0x05]=((data_size+36)>>8 ) & 0xff;
    data[0x06]=((data_size+36)>>16) & 0xff;
    data[0x07]=((data_size+36)>>24) & 0xff;


    dataptr=&data[0];

    if (my_lisa_sound!=NULL) delete my_lisa_sound;
    my_lisa_sound=new wxSound;  //my_lisa_sound->Create(dataptr,data_size);

    // this should work, but it doesn't on all platforms
    //my_lisa_sound->Create(data_size,dataptr);
    //my_lisa_sound->Play(wxSOUND_ASYNC);

    // so instead, use the super lame ass way to play a sound
    FILE *f;
    f=fopen("tmpsnd.wav","wb");
    if (f) {
             fwrite(dataptr,data_size+45,1,f); fclose(f);
             my_lisa_sound->Create(_T("tmpsnd.wav"),false);
             // we do not check for sound_effects_on here as this is the Lisa beeping.
             my_lisa_sound->Play(wxSOUND_ASYNC);
             unlink("tmpsnd.wav");
           }
    errno=0;  // avoid interfearance with other stuff like libdc42.
    return;
}



