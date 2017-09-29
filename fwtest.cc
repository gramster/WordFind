#ifdef __cplusplus
extern "C" {
#endif
#include <Pilot.h>
#include <SysEvtMgr.h>
#ifdef __cplusplus
}
#endif

#include "fw.h"

Application *Application::instance = 0;

bool Database::Open(ULong type, UInt mode, ULong creator)
{
    if (!Application::HideSecretRecords())
        mode |= dmModeShowSecret;
    db = ::DmOpenDatabaseByTypeCreator(type, creator, mode);
    if (db)
    {
        numrecords = ::DmNumRecords(db);
        return true;
    }
    return false;
}

    
bool Database::Create(CharPtr name, ULong type, 
					UInt mode, ULong creator)
{
    if (!::DmCreateDatabase(0, name, creator, type, false))
        db = 0;
    else
    	(void)Open(type, mode, creator);
    return (db == 0) ? false : true;
}


bool Database::OpenOrCreate(CharPtr name, ULong type, 
						UInt mode, ULong creator)
{
    Open(type, mode, creator);
    if (!db) Create(name, type, mode, creator);
    return (db == 0) ? false : true;
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

void List::ListDrawEventHandler(UInt idx, RectanglePtr bounds, CharPtr *data)
{
    Form *form = Application::Instance()->ActiveForm();
    List *list = form ? form->GetList() : 0;
    char *item = list ? list->GetItem(idx) : 0;
    if (item) 
	WinDrawChars(item, StrLen(item), bounds->topLeft.x,
			bounds->topLeft.y);
}

void List::Init()
{
    FormPtr frm = owner->Pointer();
    lst = (ListPtr)FrmGetObjectPtr(frm, FrmGetObjectIndex(frm, id));
//    LstSetListChoices(lst, (char**)id, NumItems());
    LstSetDrawFunction(lst, ListDrawEventHandler);
}

void List::Erase()
{
    LstEraseList(lst);
}

char *List::GetItem(UInt idx)
{
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

Boolean Form::Activate()
{
    ::FrmSetActiveForm(frm);
    ::FrmSetEventHandler(frm, Form::EventHandler);
    return true;
}

Boolean Form::EventHandler(EventPtr event) // static
{
    Boolean handled = 0;
    Form *form = Application::Instance()->ActiveForm();
    if (form) switch (event->eType)
    {
    case ctlSelectEvent:
	handled = form->HandleControlSelect(event->data.ctlSelect.controlID);
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
    default:
        handled = 0; //false;
    }
    return handled;
}

Boolean Form::HandleControlSelect(UInt objID)
{
    return true;
}

Boolean Form::HandleMenu(UInt menuID)
{
    return true;
}

Boolean Form::Open()
{
    Draw();
    return true;
}

Boolean Form::Update()
{
    return false;
}

List *Form::GetList()
{
    return 0;
}

Form::~Form()
{ }

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
    return 0;
}

Form *Application::TopForm()
{
    return 0;
}

DWord Application::Start()
{
    ::PrefGetPreferences(&sysPrefs);
    DWord error = OpenDatabases();
    if (error==0)
	activeform = TopForm();
    return error;
}

int Application::EventWaitTime()
{
    return evtWaitForever;
}

bool Application::HandleLoadEvent(int formID)
{
    Form *form = GetForm(formID);
    if (form)
    {
        form->Init();
        if (form->Activate())
	{
	    activeform = form;
	    return true;
	}
    }
    return false;
}

bool Application::HandleEvent(EventType &event)
{
    Boolean handled = false;
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
    : activeform(0)
{
    // assert(instance==0);
    instance = this;
}

Application::~Application()
{
}

DWord PilotMain(Word cmd, Ptr cmdPBP, Word launchflags)
{
    extern DWord RunApplication(Word cmd, Ptr cmdPBP, Word launchflags);
    return RunApplication(cmd, cmdPBP, launchflags);
}

//------------------------------------------------------------------

