#define MY_CREATOR_ID	'xWrd'


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
    Form *form;
    if (activeform->ID() == formID)
        form = activeform;
    else
	form = AllocForm(formID);
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

bool HandleEvent(EventType &event)
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

void EventLoop()
{
    do
    {
        EventType event;
        EvtGetEvent(&event, evtWaitForever);
	if (!SysHandleEvent(&event))
	{
	    Word error;
	    if (!MenuHandleEvent(0, &event, &error))
	    {
	        if (!HandleEvent(&event))
		    FrmDispatchEvent(&event);
	    }
	}
    } while (event.eType != appStopEvent);
}

void Stop()
{
    FrmCloseAllForms();
    CloseDatabases();
}

DWord PilotMain(Word cmd, Ptr cmdPBP, Word launchflags)
{
    DWord error = 0; //RomVersionCompatible (version20, launchflags);
    if (error) return error;

    switch (cmd)
    {
    case sysAppLaunchCmdNormalLaunch:
        if ((error = Start()) == 0)
	{
	    activeform->PostLoadEvent();
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
//    case sysAppLaunchCmdCallApplication:
    default:
        error = sysErrParamErr;
        break;
    }
    return error;
}


class Database
{
  protected:
    DmOpenRef	db;
    ULong type;
    UInt mode;
    ULong creator;
    int numrecords;
  public:
    Database()
    {}
    DmOpenRef GetOpenRef() const
    {
        return db;
    }
    UInt NumRecords() const
    {
        return numrecords;
    }
    bool Open(ULong type, UInt mode = dmModeReadWrite,
    				ULong creator = MY_CREATOR_ID);
    bool Create(CharPtr name, ULong type, UInt mode = dmModeReadWrite, 
    				ULong creator = MY_CREATOR_ID);
    bool OpenOrCreate(CharPtr name, ULong type, UInt mode = dmModeReadWrite,
    				ULong creator = MY_CREATOR_ID);
    DWord Close();
    ~Database();
};

class List
{
    UInt id;
    ListPtr lst;

  protected:
    static void ListDrawEventHandler(UInt idx, RectanglePtr bounds, CharPtr *data);
    virtual int NumItems();
    virtual char *GetItem(UInt idx);
    
  public:
    List() :
       id(0), lst(0)
    {}
    List(int id_in)
        : id(id_in), lst(0)
    { 
    }
    int ID() const
    {
        return id;
    }
    void Init();
    void Erase();
    virtual ~List();
};

class Form
{
  protected:
    UInt id;
    FormPtr frm;

    static Boolean EventHandler(EventPtr event);
    virtual Boolean HandleControlSelect(UInt objID);
    virtual Boolean HandleMenu(UInt menuID);
    virtual Boolean Open();
    virtual Boolean Update();
  public:
    Form() : frm(0) {} // shouldn't use this one
    Form(int id_in)
        : id(id_in), frm(0)
    { 
    }
    int ID() const
    {
        return id;
    }
    void Init()
    {
        frm = ::FrmInitForm(id);
    }
    void PostLoadEvent()
    {
        ::FrmGotoForm(id);
    }
    void PostUpdateEvent()
    {
        FrmUpdateForm(id, frmRedrawUpdateCode);
	}
    void Draw()
    {
        ::FrmDrawForm(frm);
    }
    Boolean Activate();
    virtual List *GetList(int res_id);
    virtual ~Form();
};

class Application
{
    //    We get linker errors if we define pure virtual methods

  protected:
    static Application *instance;

    Form *activeform;
    SystemPreferencesType sysPrefs;

    Err RomVersionCompatible(DWord requiredVersion, Word launchFlags);

    virtual int EventWaitTime();

    virtual DWord OpenDatabases();
    virtual DWord CloseDatabases();
    virtual Form *TopForm();
    virtual DWord Start();
//    virtual void SetEventHandler(FormPtr frm, Int formID);
    bool HandleLoadEvent(int formID);
    bool HandleEvent(EventType &event);
    void EventLoop();
    virtual void Stop();
  public:
    Application();
    static Application *Instance()
    {
        return instance;
    }
    static Boolean HideSecretRecords()
    {
        return instance ? instance->sysPrefs.hideSecretRecords : 0;
    }
    static Form *ActiveForm()
    {
        return instance ? instance->activeform : 0;
    }
    virtual Form *AllocForm(UInt formID);
    DWord Main(Word cmd, Ptr cmdPBP, Word launchflags);
    virtual ~Application();
};

#endif

