#include <Pilot.h>
#ifdef __GNUC__
#include "Callbacks.h"
#endif
#include "ptrsc.h"
#include "fw.h"

//-------------------------------------------------------------------
//& 
//& #define TopForm                            1000	//(Left Origin = 0, Top Origin = 0, Width = 160, Height = 160, Usable = 1, Modal = 0, Save Behind = 0, Help ID = 0, Menu Bar ID = 1000, Default Button ID = 0)
//& #define HelloWorldButtonButton                    1003	//(Left Origin = 59, Top Origin = 91, Width = 36, Height = 12, Usable = 1, Anchor Left = 1, Frame = 1, Non-bold Frame = 1, Font = Standard)
//& #define GoodnightMoonAlert                        1101
//& #define GoodnightMoonOK                           0
//&
//& #define HelloWorldMenuBar                         1000
//& # define FirstMenu                                 1010
//& #  define FirstBeep                                 1010
//& # define SecondMenu                                1000
//& #  define SecondBeepmore                            1000
//& 
//-------------------------------------------------------------------
//! 
//! #include "ptrsc.h"
//! 
//! MENU ID HelloWorldMenuBar
//! BEGIN
//!     PULLDOWN "First"
//!     BEGIN
//!         MENUITEM "Beep" ID FirstBeep
//!     END
//! 
//!     PULLDOWN "Second"
//!     BEGIN
//!         MENUITEM "Beep more" ID SecondBeepmore
//!     END
//! END
//! 
//! 
//! FORM ID TopForm AT (0 0 160 160)
//! MENUID 1000
//! BEGIN
//!     TITLE "Hello World"
//!     BUTTON "Button" ID HelloWorldButtonButton AT (59 91 36 12) LEFTANCHOR FRAME FONT 0
//! END
//! 
//! ALERT ID GoodnightMoonAlert
//! INFORMATION
//! BEGIN
//!     TITLE "An alert"
//!     MESSAGE "Goodnight moon!"
//!     BUTTONS "OK"
//! END
//! 
//! VERSION ID 1000 "1.0"
//! 
//-------------------------------------------------------------------

class HelloApplication : public Application
{
  protected:
    virtual int TopFormID() const;
    virtual void SetEventHandler(FormPtr frm, Int formID);
  public:
    static Boolean TopFormEventHandler(EventPtr event);
};

int HelloApplication::TopFormID() const
{
    return TopForm;
}

Boolean HelloApplication::TopFormEventHandler(EventPtr event)
{
    Boolean handled = true;
#ifdef __GNUC__
    CALLBACK_PROLOGUE
#endif
    switch (event->eType)
    {
    case ctlSelectEvent:
       ::FrmAlert(GoodnightMoonAlert);
       break;
    case frmOpenEvent:
        ::FrmDrawForm(::FrmGetActiveForm());
	break;
    case menuEvent:
        if (event->data.menu.itemID == FirstBeep)
	    ::SndPlaySystemSound(sndInfo);
	else
	    ::SndPlaySystemSound(sndStartUp);
	break;
    default:
        handled = 0; //false;
    }
#ifdef __GNUC__
    CALLBACK_EPILOGUE
#endif
    return handled;
}

void HelloApplication::SetEventHandler(FormPtr frm, Int formID)
{
    switch (formID)
    {
    case TopForm:
        FrmSetEventHandler(frm, TopFormEventHandler);
    }
}

DWord RunApplication(Word cmd, Ptr cmdPBP, Word launchflags)
{
    HelloApplication app;
    return app.Main(cmd, cmdPBP, launchflags);
}

