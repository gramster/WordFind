/*
   Personal Trainer Plus
*/

#include <Pilot.h>
#include <string.h>
#include "ptrsc.h"
#ifdef __GNUC__
#include "Callbacks.h"
#endif

// for definition of IsDigit

//#include <CharAttr.h>
#include <SysEvtMgr.h>

//-----------------------------------------------------------
// Callbacks

static void ClientsDrawFunc(UInt itemnum, RectanglePtr bounds, CharPtr *data)
{
    FormPtr frm = FrmGetActiveForm();
    ListPtr list = FrmGetObjectPtr(frm, 
    				FrmGetObjectIndex(frm, ClientsClientList));
    char *client = "Invalid";
    switch (itemnum)
    {
    case 0:
    	client = "Mike";
	break;
    case 1:
    	client = "Melinda";
	break;
    }
    WinDrawChars(client, strlen(client), 
    			bounds->topLeft.x, bounds->topLeft.y);
}    

//-----------------------------------------------------------
// Form Event Handlers

static Boolean ClientsFormHandleEvent(EventPtr event)
{
    Boolean		handled;
    FormPtr		frm;
    ListPtr		list;
#ifdef __GNUC__
    CALLBACK_PROLOGUE
#endif
    frm = FrmGetActiveForm();
    list = FrmGetObjectPtr(frm, FrmGetObjectIndex(frm, ClientsClientList));

    handled = true;

    switch (event->eType)
    {
    case ctlSelectEvent:  // A control button was pressed and released.
    	switch (event->data.ctlSelect.controlID)
	{
	case ClientsNewButton:
	    break;
	case ClientsOKButton:
	    //SwitchForm(ClientForm);
	    break;
	}
	break;

    case frmOpenEvent:
	LstSetDrawFunction(list, ClientsDrawFunc);        
	FrmDrawForm(FrmGetActiveForm());
	break;

    case menuEvent:
	//	  if (event->data.menu.itemID == FirstBeep)
	//  		SndPlaySystemSound(sndInfo);
	//	  else
	//  		SndPlaySystemSound(sndStartUp);
        handled = false;
	break;
    default:
        handled = false;
        break;
    }
#ifdef __GNUC__
    CALLBACK_EPILOGUE
#endif
    return handled;
}

// Install form event handlers

static Boolean SetFormEventHandler(FormPtr frm, int formID)
{
    switch (formID)
    {
    case ClientsForm:
        FrmSetEventHandler(frm, ClientsFormHandleEvent);
        return true;
    default:
        return false;
    }
}

//------------------------------------------------------------
// Beow here the code is pretty generic

static Err StartApplication(void)
{
    FrmGotoForm(ClientsForm);
    return 0;
}

static void StopApplication(void)
{
}

static Boolean ApplicationHandleEvent(EventPtr event)
{
    FormPtr	frm;
    Int		formID;
    Boolean	handled = false;

    if (event->eType == frmLoadEvent)
    {
        // Load the form resource specified in the event then activate the form.
	int formID = event->data.frmLoad.formID;
	FormPtr frm = FrmInitForm(formID);
	if (formID == ClientsForm)
	{
	    //PopupPtr popup = FrmGetObjectPtr(frm, 
    	    //			FrmGetObjectIndex(frm, ClientsClientPopup));
	    // popup->SetSelection();
	}
	FrmSetActiveForm(frm);

        // Set the event handler for the form.  The handler of the currently
	// active form is called by FrmDispatchEvent each time it receives an event.

        SetFormEventHandler(frm, formID);
        return true;
    }
    return false;
}

static void EventLoop(void)
{
    EventType	event;
    Word	error;

    do {
	EvtGetEvent(&event, evtWaitForever);
	if (! SysHandleEvent(&event))
	    if (! MenuHandleEvent(0, &event, &error))
		if (! ApplicationHandleEvent(&event))
                    FrmDispatchEvent(&event);
    } while (event.eType != appStopEvent);
}


static DWord NormalLaunch()
{
    Err err = 0;
    if ((err = StartApplication()) == 0)
    {
        EventLoop();
	StopApplication();
    }
    return err;
}

DWord PilotMain(Word cmd, Ptr cmdPBP, Word launchFlags)
{
    Err err = 0;
    switch(cmd)
    {
    case sysAppLaunchCmdNormalLaunch:
        NormalLaunch();
        break;
    }
    return err;
}
