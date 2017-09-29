/*
	Hello.c 
	From Palm Programming: the Developer's Guide
	by Neil Rhodes and Julie McKeehan
*/

#include <Pilot.h>				
#include "ConsultRsc.h"
#ifdef __GNUC__
#include "Callbacks.h"
#endif		

// for definition of IsDigit

#include <CharAttr.h>
#include <SysEvtMgr.h>

static Boolean MyFormHandleEvent(EventPtr event)
{
    Boolean		handled;
#ifdef __GNUC__
    CALLBACK_PROLOGUE
#endif
    handled = false;

    switch (event->eType)
    {
    case ctlSelectEvent:  // A control button was pressed and released.
   	FrmAlert(GoodnightMoonAlert);
	handled = true;
	break;
  	
    case frmOpenEvent:	
	FrmDrawForm(FrmGetActiveForm());
	handled = true;
	break;
			
    case menuEvent:		
	if (event->data.menu.itemID == FirstBeep)
	     SndPlaySystemSound(sndInfo);
	else
	     SndPlaySystemSound(sndStartUp);
	handled = true;
	break;
    }
#ifdef __GNUC__
    CALLBACK_EPILOGUE
#endif
    return(handled);
}

static Boolean ApplicationHandleEvent(EventPtr event)
{
    FormPtr	frm;
    Int		formId;
    Boolean	handled = false;

    if (event->eType == frmLoadEvent)
    {
	// Load the form resource specified in the event then activate the form.
	formId = event->data.frmLoad.formID;
	frm = FrmInitForm(formId);
	FrmSetActiveForm(frm);

	// Set the event handler for the form.  The handler of the currently 
	// active form is called by FrmDispatchEvent each time it receives an event.
	switch (formId)
	{
	case ConsultForm:
		FrmSetEventHandler(frm, MyFormHandleEvent);
		break;
	}
	handled = true;
    }
    return handled;
}

Err Application::StartApplication(void)
{
    FrmGotoForm(ConsultForm);
    return 0;
}

void Application::EventLoop(void)
{
    EventType	event;
	
    do {
	Word	error;
	EvtGetEvent(&event, evtWaitForever);
	if (! SysHandleEvent(&event))
	    if (! MenuHandleEvent(0, &event, &error))
		if (! ApplicationHandleEvent(&event))
		    FrmDispatchEvent(&event);
    } while (event.eType != appStopEvent);
}

void Application::StopApplication(void)
{
}

DWord Application::DoNormalLaunch(Word cmd, Ptr cmdPBP)
{
    Err err = StartApplication();
    if (err == 0)
    {
        EventLoop();
        StopApplication();
    }
    return err;
}

DWord Application::Main(Word cmd, Ptr cmdPBP, Word launchFlags)
{
    Err err = 0;		
    if (cmd == ::sysAppLaunchCmdNormalLaunch)
        err = DoNormalLaunch();
    return err;
}

DWord PilotMain(Word cmd, Ptr cmdPBP, Word launchFlags)
{
    Application &app = GetApplication();
    return app.Main(cmd, cmdPBP, launchFlags);
}

