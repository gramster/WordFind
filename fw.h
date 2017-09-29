#ifndef __FW_H
#define __FW_H

#define MY_CREATOR_ID		'xWrd'
#define True			((Boolean)1)
#define False			((Boolean)0)

#define isspace(c)		((c)==' ' || (c)=='\t' || (c)=='\n')
#define isupper(c)		((c)>='A' && (c)<='Z')
#define islower(c)		((c)>='a' && (c)<='z')
#define toupper(c)		((char)(islower(c) ? ((c)-'a'+'A') : (c)))
#define isalpha(c)		(isupper(c) || islower(c))
#define isdigit(c)		((c)>='0' && (c)<='9')
#define InRange(v, mn, mx)	((v) >= (mn) && (v) <= (mx))

#ifndef TEST
#define strcpy(dst, src)	StrCopy(dst, src)
#define strcat(dst, src)	StrCat(dst, src)
#define strlen(s)		StrLen(s)
#define strcmp(s1, s2)		StrCompare(s1, s2)
#endif

void strncpy(char *dst, const char *src, unsigned n);
void memcpy(char *dst, const char *src, unsigned n);
void memset(char *dst, const char v, unsigned n);
int strncmp(const char *s1, const char *s2, unsigned n);

void HMemCopy(char *buf, VoidHand h, UInt maxlen);

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
    VoidHand NewRecord(UInt recnum, UInt size)
    {
	VoidHand rtn = DmNewRecord(db, &recnum, size);
        ++numrecords;
	return rtn;
    }
    VoidHand Resize(UInt recnum, UInt size)
    {
	return DmResizeRecord(db, recnum, size);
    }
    UInt CreateRecord(UInt recnum, UInt size)
    {
	(void)DmNewRecord(db, &recnum, size);
        ++numrecords;
	return recnum;
    }
    VoidHand QueryRecord(UInt recnum)
    {
        return (recnum>=0 && recnum<numrecords) ? DmQueryRecord(db, recnum) : 0;
    }
    VoidHand GetRecord(UInt recnum)
    {
        return (recnum>=0 && recnum<numrecords) ? DmGetRecord(db, recnum) : 0;
    }
    void ReleaseRecord(UInt recnum)
    {
        if (recnum>=0 && recnum<numrecords)
	    DmReleaseRecord(db, recnum, true);
    }
    void WriteRecord(void *dptr, unsigned char *ptr, UInt size, UInt offset = 0)
    {
	DmWrite(dptr, offset, ptr, size); 
    }
    ~Database();
};

class List
{
  protected:
    UInt id;
    ListPtr lst;
    class Form *owner;

    static void ListDrawEventHandler(UInt idx, RectanglePtr bounds, CharPtr *data);
    virtual int NumItems();
    virtual char *GetItem(UInt idx);
    
  public:
    List(class Form *owner_in, UInt id_in = 0);
    UInt ID() const
    {
        return id;
    }
    virtual void Init();
    virtual Boolean Activate();
    virtual Boolean HandleSelect(UInt selection);
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
    virtual Boolean HandleListSelect(UInt listID, UInt selection);
    virtual Boolean HandlePopupListSelect(UInt triggerID,
					UInt listID,
					UInt selection);
    virtual Boolean HandleKeyDown(UInt chr, UInt keyCode, UInt &modifiers);

    virtual Boolean Open(); // draw time
    virtual Boolean Update();
  public:
    Form() : frm(0) {} // shouldn't use this one
    Form(UInt id_in);
    UInt ID() const
    {
        return id;
    }
    virtual void PostHandle(EventType &event);
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
    void EnableControl(UInt id, int v = 1);
    void DisableControl(UInt id);
    void HideControl(UInt resid);
    void ShowControl(UInt resid);
    void SetControlLabel(UInt resid, const char *lbl);
    void ClearField(UInt resid);
    CharPtr ReadField(UInt resid);
    void InsertInField(UInt resid, char c);
    void SetCheckBox(UInt resid, UInt val = 1);
    UInt IsCheckBoxSet(UInt resid);
    void SetField(UInt resid, const char *text);
    void DrawRectangle(int left, int top, int width, int height);
    void ClearRectangle(int left, int top, int width, int height);
    Form *Switch(UInt resid); // activate a different form
    VoidPtr GetObject(UInt resid);
    Boolean IsActive() const;
    virtual Boolean Activate(); // pre-draw time
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
    virtual Boolean Activate();
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
    UInt GetValue();
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
    virtual void Init();
    virtual Boolean Activate();
    virtual Boolean HandleSelect(UInt objID);
    virtual UInt Run();
    virtual ~RadioGroupDialog();
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
    static Boolean IsActive(const Form *f);
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

void DrawDebugText(int x, int y, const char *txt);

#endif

