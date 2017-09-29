#include "mypilot.h"

#include "fw.h"
#ifdef __GNUC__
#include "Callbacks.h"
#endif

void strncpy(char *dst, const char *src, unsigned n)
{
    while (n-- > 0 && (*dst++ = *src++) != 0)
        (void)0;
}

void memset(char *dst, const char v, unsigned n)
{
    while (n-- > 0)
        *dst++ = v;
}

void memcpy(char *dst, const char *src, unsigned n)
{
    while (n-- > 0)
        *dst++ = *src++;
}

int strcmp(char *s1, char *s2)
{
    while (*s1 && *s1 == *s2)
    {
        ++s1;
	++s2;
    }
    return (*s1-*s2);
}

int strncmp(const char *s1, const char *s2, unsigned n)
{
    while (n > 0 && *s1 && *s1 == *s2)
    {
        ++s1;
	++s2;
	--n;
    }
    return (n==0) ? 0 : (*s1-*s2);
}

const char *ltoa(unsigned long l)
{
    if (l <= 0)
    	return "0";
    else
    {
        static char x[10];
    	int p = 10;
    	while (l)
    	{
        	x[--p] = (char)((l%10)+'0');
        	l = l / 10;
    	}
    	return x+p;
    }
}

void HMemCopy(char *buf, VoidHand h, UInt maxlen)
{
    buf[0] = 0;
    if (h)
    {
	char *s = (char *)MemHandleLock(h);
	if (s)
	{
	    strncpy(buf, s, maxlen-1);
	    buf[maxlen-1] = 0;
	}
	MemHandleUnlock(h);
    }
}

//Application *Application::instance = 0; // assignment will crash
Application *Application::instance;

#ifdef __GNUC__
extern "C" void _exit(int) {} // get around linker error
#endif

Boolean Database::Open(ULong type, UInt mode, ULong creator)
{
    if (!Application::HideSecretRecords())
        mode |= dmModeShowSecret;
    db = ::DmOpenDatabaseByTypeCreator(type, creator, mode);
    if (db)
    {
        numrecords = (int)::DmNumRecords(db);
        return True;
    }
    else
    {
        numrecords = -1;
        return False;
    }
}

    
Boolean Database::Create(CharPtr name, ULong type, 
					UInt mode, ULong creator)
{
    if (!::DmCreateDatabase(0, name, creator, type, false))
        db = 0;
    else
    	(void)Open(type, mode, creator);
    return (db ? True : False);
}


Boolean Database::OpenOrCreate(CharPtr name, ULong type, 
						UInt mode, ULong creator)
{
    Open(type, mode, creator);
    if (!db) Create(name, type, mode, creator);
    return (db ? True : False);
}

DWord Database::Close()
{
    if (db)
    {
        DWord error = ::DmCloseDatabase(db);
	if (error==0) db = 0;
	return error;
    }
    numrecords = -1;
    return 0;
}

Database::~Database()
{
    if (db) Close();
}

//-----------------------------------------------------------------

List::List(Form *owner_in, UInt id_in)
   : id(id_in), lst(0), owner(owner_in)
{ 
}

/* static */
void List::ListDrawEventHandler(UInt idx, RectanglePtr bounds, 
				CharPtr *data)
{
    (void)data;
    Form *form = Application::Instance()->GetForm();
    List *list = form ? form->GetList() : 0;
    char *item = list ? list->GetItem(idx) : 0;
    if (item) 
    	WinDrawChars(item, StrLen(item), bounds->topLeft.x, bounds->topLeft.y);
}

void List::Init()
{
}

Boolean List::Activate() // owner must be active
{
    lst = (ListPtr)owner->GetObject(id);
    if (lst)
    {
	LstSetListChoices(lst, (char**)0, (UInt)NumItems());
    	LstSetDrawFunction(lst, ListDrawEventHandler);
	return True;
    }
    return False;
}

void List::Erase()
{
    if (lst) LstEraseList(lst);
}

char *List::GetItem(UInt idx)
{
    (void)idx;
    return 0;
}

int List::NumItems()
{
    return 0;
}

List::~List()
{
}

//-----------------------------------------------------------------

Form::Form(UInt id_in)
    : id(id_in), frm(0)
{ 
}

void Form::Init()
{
    frm = ::FrmInitForm(id);
}

void Form::ShowProgress(int p)
{
    (void)p;
}

Boolean Form::IsActive() const
{
    return Application::IsActive(this);
}

VoidPtr Form::GetObject(UInt resid)
{
    if (frm && IsActive())
    {
        UInt oi = FrmGetObjectIndex(frm, resid);
        return FrmGetObjectPtr(frm, oi);
    }
    else return 0;
}

void Form::DrawControl(UInt resid)
{
    ControlPtr b = (ControlPtr)GetObject(resid);
    if (b)
    {
        CtlSetUsable(b, 1);
        CtlDrawControl(b);
    }
}

void Form::EnableControl(UInt resid)
{
    ControlPtr b = (ControlPtr)GetObject(resid);
    if (b) CtlSetEnabled(b, 1);
}

void Form::DisableControl(UInt resid)
{
    ControlPtr b = (ControlPtr)GetObject(resid);
    if (b) CtlSetEnabled(b, 0);
}

void Form::SetControlLabel(UInt resid, const char *lbl)
{
    ControlPtr sel = (ControlPtr)GetObject(resid);
    if (sel) CtlSetLabel(sel, (char*)lbl);
}

void Form::ClearField(UInt resid)
{
    FieldPtr fld = (FieldPtr)GetObject(resid);
    if (fld) FldDelete(fld, 0, FldGetTextLength(fld));
}

CharPtr Form::ReadField(UInt resid)
{
    FieldPtr fld = (FieldPtr)GetObject(resid);
    return fld ? FldGetTextPtr(fld) : 0;
}

void Form::InsertInField(UInt resid, char c)
{
    FieldPtr fld = (FieldPtr)GetObject(resid);
    if (fld) FldInsert(fld, &c, 1);
}

void Form::SetField(UInt resid, const char *txt)
{
    FieldPtr fld = (FieldPtr)GetObject(resid);
    if (fld)
    {
        FldDelete(fld, 0, FldGetTextLength(fld));
        FldInsert(fld, (char*)txt, strlen(txt));
    }
}

void Form::SetCheckBox(UInt resid, UInt val)
{
    ControlPtr cm = (ControlPtr)GetObject(resid);
    if (cm) CtlSetValue(cm, (Short)val);
}

UInt Form::IsCheckBoxSet(UInt resid)
{
    ControlPtr cm = (ControlPtr)GetObject(resid);
    return (UInt)((cm && CtlGetValue(cm)) ? 1 : 0);
}

void Form::DrawRectangle(int left, int top, int width, int height)
{
    WinHandle h = WinGetDrawWindow();
    WinSetDrawWindow(WinGetDisplayWindow());
    RectangleType r;
    r.topLeft.x = left;
    r.topLeft.y = top;
    r.extent.x = width;
    r.extent.y = height;
    WinDrawRectangle(&r, 0);
    WinSetDrawWindow(h);
}

void Form::ClearRectangle(int left, int top, int width, int height)
{
    WinHandle h = WinGetDrawWindow();
    WinSetDrawWindow(WinGetDisplayWindow());
    RectangleType r;
    r.topLeft.x = left;
    r.topLeft.y = top;
    r.extent.x = width;
    r.extent.y = height;
    WinEraseRectangle(&r, 0);
    WinSetDrawWindow(h);
}

Boolean Form::Activate()
{
    if (frm)
    {
	Application::Instance()->SetActiveForm(this);
        ::FrmSetActiveForm(frm);
        ::FrmSetEventHandler(frm, Form::EventHandler);
        return True;
    }
    return False;
}

void Form::Switch(UInt resid) // activate a different form
{
    Form *f = Application::Instance()->GetForm(resid);
    if (f) f->PostLoadEvent();
}

Boolean Form::EventHandler(EventPtr event) // static
{
#ifdef __GNUC__
    CALLBACK_PROLOGUE
#endif
    Boolean handled = False;
    Form *form = Application::Instance()->GetForm();
    if (form) switch (event->eType)
    {
    case ctlSelectEvent:
	handled = form->HandleSelect(event->data.ctlSelect.controlID);
	break;
    case sclRepeatEvent:
	handled = form->HandleScroll(event->data.sclRepeat.scrollBarID,
					event->data.sclRepeat.value,
					event->data.sclRepeat.newValue);
	break;
    case frmOpenEvent:
	handled = form->Open();
	break;
    case frmUpdateEvent:
	handled = form->Update();
	break;
    case menuEvent:
	handled = form->HandleMenu(event->data.menu.itemID);
	break;
    case popSelectEvent:
	handled = form->HandlePopupListSelect(event->data.popSelect.controlID,
						event->data.popSelect.listID,
						event->data.popSelect.selection);
	break;
    default:
        handled = False;
    }
#ifdef __GNUC__
    CALLBACK_EPILOGUE
#endif
    return handled;
}

Boolean Form::HandleSelect(UInt objID)
{
    (void)objID;
    return False;
}

Boolean Form::HandlePopupListSelect(UInt triggerID,
					UInt listID,
					UInt selection)
{
    (void)triggerID;
    (void)listID;
    (void)selection;
    return False;
}

Boolean Form::HandleScroll(UInt objID, Short oldv, Short newv)
{
    (void)objID;
    (void)oldv;
    (void)newv;
    return False;
}

Boolean Form::HandleMenu(UInt menuID)
{
    (void)menuID;
    return False;
}

Boolean Form::Open() // NB Dialogs mustn't use this (see PP pg 103)
{
    Draw();
    return True;
}

Boolean Form::Update()
{
    return False;
}

List *Form::GetList()
{
    return 0;
}

Form::~Form()
{
}

//------------------------------------------------------------

Boolean Dialog::Activate()
{
    return Form::Activate();
}

void Dialog::Init()
{
    Form::Init();
//    Application::Instance()->SetActiveForm(this);
//    ::FrmSetEventHandler(frm, Form::EventHandler);
}

Boolean Dialog::HandleSelect(UInt objID)
{
    return Form::HandleSelect(objID);
}

Boolean Dialog::HandleScroll(UInt objID, Short oldv, Short newv)
{
    return Form::HandleScroll(objID, oldv, newv);
}

UInt Dialog::Run()
{
    Init();
    if (Activate())
	return FrmDoDialog(frm);
    return 0;
}

Dialog::~Dialog()
{
    if (parent) parent->Activate();
    if (frm) FrmDeleteForm(frm);
}

//------------------------------------------------------------

void RadioGroupDialog::Init()
{
    Dialog::Init();
}

Boolean RadioGroupDialog::Activate()
{
    if (Dialog::Activate())
    {
        if (title) FrmSetTitle(frm, title);
        for (UInt i = 0; i < numcbs; i++)
        {
            ControlPtr s = (ControlPtr)GetObject(firstcb+i);
            if (s) CtlSetValue(s, (Short)((i==val)?1:0));
        }
	return True;
    }
    return False;
}

UInt RadioGroupDialog::Run()
{
    (void)Dialog::Run();
    return GetValue();
}

UInt RadioGroupDialog::GetValue()
{
    for (UInt i = 0; i < numcbs; i++)
    {
        ControlPtr s = (ControlPtr)GetObject(firstcb+i);
        if (s && CtlGetValue(s))
            return i;
    }
    return 0;
}


RadioGroupDialog::~RadioGroupDialog()
{
}

//------------------------------------------------------------

DWord Application::OpenDatabases()
{
    return 0;
}

DWord Application::CloseDatabases()
{
    return 0;
}

Form *Application::GetForm(UInt formID)
{
    (void)formID;
    return 0;
}

Form *Application::TopForm()
{
    return 0;
}

Application *Application::Instance()
{
    return instance;
}

void Application::SetActiveForm(Form *f)
{
    if (instance) instance->activeform = f;
}


Boolean Application::HideSecretRecords()
{
    if (instance && instance->sysPrefs.hideSecretRecords)
        return true;
    else
        return false;
}

Err Application::RomVersionCompatible(DWord requiredVersion, Word launchFlags)
{
    DWord romVersion;
	
    // See if we're on in minimum required version of the ROM or later.
    // The system records the version number in a feature.  A feature is a
    // piece of information which can be looked up by a creator and feature
    // number.

    FtrGet(sysFtrCreator, sysFtrNumROMVersion, &romVersion);

    if (romVersion < requiredVersion)
    {
	// If the user launched the app from the launcher, explain
	// why the app shouldn't run.  If the app was contacted for something
	// else, like it was asked to find a string by the system find, then
	// don't bother the user with a warning dialog.  These flags tell how
	// the app was launched to decided if a warning should be displayed.

	if ((launchFlags & (sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp)) ==
	    	(sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp))
	{
	  //  FrmAlert (RomIncompatibleAlert);
	    // Pilot 1.0 will continuously relaunch this app unless we switch to 
	    // another safe one.  The sysFileCDefaultApp is considered "safe".
	    if (romVersion < 0x02000000)
		AppLaunchWithCommand(sysFileCDefaultApp, sysAppLaunchCmdNormalLaunch, NULL);
	}
	return (sysErrRomIncompatible);
    }
    return 0;
}

int Application::EventWaitTime()
{
    return evtWaitForever;
}

Boolean Application::HandleLoadEvent(UInt formID)
{
    Form *form = GetForm(formID);
    if (form)
    {
        form->Init();
        if (form->Activate())
	{
	    activeform = form;
	    return True;
	}
    }
    return False;
}

Boolean Application::HandleEvent(EventType &event)
{
    Boolean handled = False;
    switch (event.eType)
    {
    case frmLoadEvent:
        handled = HandleLoadEvent(event.data.frmLoad.formID);
	break;
    default:
	break;
    }
    return handled;
}

DWord Application::Start()
{
    ::PrefGetPreferences(&sysPrefs);
    return OpenDatabases();
}

void Application::EventLoop()
{
    EventType event;
    do
    {
        ::EvtGetEvent(&event, EventWaitTime());
	if (!::SysHandleEvent(&event))
	{
	    Word error;
	    if (!::MenuHandleEvent(0, &event, &error))
	    {
	        if (!HandleEvent(event))
		    ::FrmDispatchEvent(&event);
	    }
	}
    } while (event.eType != appStopEvent);
}

void Application::Stop()
{
    FrmCloseAllForms();
    CloseDatabases();
}

DWord Application::Main(Word cmd, Ptr cmdPBP, Word launchflags)
{
    (void)cmdPBP;
    (void)launchflags;
    DWord error = 0; //RomVersionCompatible (version20, launchflags);
    if (error) return error;

    switch (cmd)
    {
    case sysAppLaunchCmdNormalLaunch:
        if ((error = Start()) == 0)
	{
	    TopForm()->PostLoadEvent();
	    EventLoop();
	    Stop();
	}
	break;
    case sysAppLaunchCmdFind:
    case sysAppLaunchCmdGoTo:
    case sysAppLaunchCmdSyncNotify:
    case sysAppLaunchCmdTimeChange:
    case sysAppLaunchCmdSystemReset:
    case sysAppLaunchCmdAlarmTriggered:
    case sysAppLaunchCmdDisplayAlarm:
    case sysAppLaunchCmdCountryChange:
    case sysAppLaunchCmdSyncRequest:
    case sysAppLaunchCmdSaveData:
    case sysAppLaunchCmdInitDatabase:
        error = 0;
	break;
    default:
        error = sysErrParamErr;
        break;
    }
    return error;
}

Application::Application()
{
    // assert(instance==0);
    instance = this;
}

Boolean Application::IsActive(const Form *f)
{
    return (instance && (instance->GetForm() == f));
}

Application::~Application()
{
}

void DrawDebugText(int x, int y, const char *txt)
{
    WinHandle h = WinGetDrawWindow();
    WinSetDrawWindow(WinGetDisplayWindow());
    WinDrawChars(txt, strlen(txt), x, y);
    WinSetDrawWindow(h);
}

DWord PilotMain(Word cmd, Ptr cmdPBP, Word launchflags)
{
    extern DWord RunApplication(Word cmd, Ptr cmdPBP, Word launchflags);
    if (cmd == sysAppLaunchCmdNormalLaunch)
        return RunApplication(cmd, cmdPBP, launchflags);
    return 0;
}

//------------------------------------------------------------------

