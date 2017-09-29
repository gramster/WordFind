#ifndef __FW_H
#define __FW_H

#define MY_CREATOR_ID	'xWrd'
#define True			((Boolean)1)
#define False			((Boolean)0)

#define isspace(c)	((c)==' ' || (c)=='\t' || (c)=='\n')
#define isupper(c)	((c)>='A' && (c)<='Z')
#define islower(c)	((c)>='a' && (c)<='z')
#define toupper(c)	((char)(islower(c) ? ((c)-'a'+'A') : (c)))
#define isalpha(c)	(isupper(c) || islower(c))
#define InRange(v, mn, mx)	((v) >= (mn) && (v) <= (mx))

unsigned strlen(char *s);
void strcpy(char *dst, const char *src);
void strcat(char *dst, const char *src);
int strcmp(char *s1, char *s2);
const char *ltoa(unsigned long l);

struct formEventMap
{
    Int	formID;
    Boolean (*handler)(EventPtr event);
};

extern struct formEventMap fmap[];

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
    int IsOpen() const
    {
        return (db != 0);
    }
    int NumRecords() const
    {
        return numrecords;
    }
    Boolean Open(ULong type, UInt mode = dmModeReadWrite,
    				ULong creator = MY_CREATOR_ID);
    Boolean Create(CharPtr name, ULong type, UInt mode = dmModeReadWrite, 
    				ULong creator = MY_CREATOR_ID);
    Boolean OpenOrCreate(CharPtr name, ULong type, UInt mode = dmModeReadWrite,
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
    List(UInt id_in = 0);
    UInt ID() const
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
    virtual Boolean HandleScroll(UInt objID, Short oldv, Short newv);
    virtual Boolean HandleSelect(UInt objID);
    virtual Boolean HandleMenu(UInt menuID);
//    virtual Boolean HandleListSelect(UInt listID, UInt selection);
    virtual Boolean HandlePopupListSelect(UInt triggerID,
					UInt listID,
					UInt selection);
    virtual Boolean Open();
    virtual Boolean Update();
  public:
    Form() : frm(0) {} // shouldn't use this one
    Form(UInt id_in);
    UInt ID() const
    {
        return id;
    }
    virtual void Init();
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
    void DrawControl(UInt id);
    void EnableControl(UInt id);
    void DisableControl(UInt id);
    void SetControlLabel(UInt resid, char *lbl);
    void ClearField(UInt resid);
    CharPtr ReadField(UInt resid);
    void InsertInField(UInt resid, char c);
    void SetField(UInt resid, char *txt);
    void SetCheckBox(UInt resid, UInt val = 1);
    UInt IsCheckBoxSet(UInt resid);
    void SetField(UInt resid, const char *text);
    void DrawRectangle(int left, int top, int width, int height);
    void ClearRectangle(int left, int top, int width, int height);
    void Switch(UInt resid); // activate a different form
    VoidPtr GetObject(UInt resid);
    Boolean Activate();
    virtual List *GetList();
    virtual void ShowProgress(int p);
    virtual ~Form();
};

class Dialog : public Form
{
    Form *parent;
  public:
    Dialog(Form *parent_in, UInt resid)
        : Form(resid), parent(parent_in)
    {}
    virtual void Init();
    virtual Boolean HandleSelect(UInt objID);
    virtual Boolean HandleScroll(UInt objid, Short oldv, Short newv);
    virtual UInt Run();
    virtual ~Dialog();
};

class RadioGroupDialog : public Dialog
{
    UInt firstcb;
    UInt numcbs;
    UInt val;
    CharPtr title;
  public:
    RadioGroupDialog(Form *parent_in,
    				UInt resid,
    				UInt firstcb_in,
    				UInt numcbs_in,
    				UInt val_in,
    				CharPtr title_in = 0)
        : Dialog(parent_in, resid),
          firstcb(firstcb_in),
          numcbs(numcbs_in),
          val(val_in),
          title(title_in) // must be static
    {}
    virtual void Init()
    {
    	Dialog::Init();
    	for (UInt i = 0; i < numcbs; i++)
    	{
            ControlPtr s = (ControlPtr)GetObject(firstcb+i);
	    if (s)
	    	CtlSetValue(s, (Short)((i==val)?1:0));
	}
        if (title) FrmSetTitle(frm, title);
    }
    UInt GetValue();
    virtual UInt Run()
    {
        (void)Dialog::Run();
		return GetValue();
    }
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
    Boolean HandleLoadEvent(UInt formID);
    Boolean HandleEvent(EventType &event);
    void EventLoop();
    virtual void Stop();
  public:
    Application();
    static void SetActiveForm(Form *f);
    static Application *Instance();
    static Boolean HideSecretRecords();
   
    virtual Form *GetForm(UInt formID);
    Form *GetForm() const
    {
        return activeform;
    }
    DWord Main(Word cmd, Ptr cmdPBP, Word launchflags);
    virtual ~Application();
};

#endif

