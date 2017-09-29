#ifndef __FW_H
#define __FW_H

#define MY_CREATOR_ID	'xWrd'

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

class Form;

class List
{
    UInt id;
    ListPtr lst;
    Form *owner;

  protected:
    static void ListDrawEventHandler(UInt idx, RectanglePtr bounds, CharPtr *data);
    virtual int NumItems();
    virtual char *GetItem(UInt idx);
    
  public:
    List(Form *owner_in) :
       id(0), lst(0), owner(owner_in)
    {}
    List(Form *owner_in, int id_in)
        : id(id_in), lst(0), owner(owner_in)
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
    FormPtr Pointer() const
    {
        return frm;
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
        if (frm) ::FrmDrawForm(frm);
    }
    Boolean Activate();
    virtual List *GetList();
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
    virtual Form *GetForm(UInt formID);
    DWord Main(Word cmd, Ptr cmdPBP, Word launchflags);
    virtual ~Application();
};

#endif

