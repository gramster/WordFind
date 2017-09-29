#define GLOBAL // just used to mark globals

#include "mypilot.h"
#ifdef IS_FOR_PALM
#include "wordfindlite_res.h"
#include "fw.h"
#endif

#define MAX_PAT_LEN		(64)
#include "dict.h"

// derive the application

#define CLIENT_DB_TYPE		(0)

#define ConfigDBType		((UInt32)'pwcl')

#define NumWordListItems	(11)

#define CFGSZ			(2*MAX_PAT_LEN)

// Globals - bad in C++ but we need to avoid stack/heap usage

GLOBAL class WordFindDict *dict;

void DrawRectangle(int left, int top, int width, int height);
void ClearRectangle(int left, int top, int width, int height);
void ClearProgress();
void ShowProgress(int p);
Form *GetForm(UInt16 id);

void DrawRectangle(int left, int top, int width, int height)
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

void ClearRectangle(int left, int top, int width, int height)
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

void ClearProgress()
{
    ClearRectangle(153, 15, 5, 120);
}

void ShowProgress(int p)
{
    DrawRectangle(153, 20, 5, p);
}

class WordFindDict : public DictionaryDatabase
{
  protected:
    virtual int MustStop();
  public:
    WordFindDict() : DictionaryDatabase() {}
    virtual ~WordFindDict() {}
};

int WordFindDict::MustStop()
{
#ifdef IS_FOR_PALM
    short x, y;
    Boolean pendown;
    EvtGetPen(&x, &y, &pendown);
    return (pendown && x>130 && y > 143);
#else
    return DictionaryDatabase::MustStop();
#endif
}

//---------------------------------------------------------

// useful utility

Form *GetForm(UInt16 id)
{
    Application *a = Application::Instance();
    return a ? a->GetForm(id) : 0;
}

// XXX This could be moved to a common file, as
// it is identical in WordFind++/Pro except for the
// Start method.

class WordListSource : public ListSource
{
  private:
    int nomore;

  public:
    inline WordListSource()
      : ListSource()
    {}    
    inline int NoMore() const
    {
        return nomore;
    }
    inline void Reset()
    {
        nomore = 0;
    }
    const char *Start(const char *pattern, const char *anagram);
    virtual UInt16 NumItems();
    virtual Boolean GetItem(Int16 itemNum, Char* buf, UInt16 buflen);
};

class WordList : public List
{
  protected:
    WordListSource s;

    LISTHANDLER(WordFormWordsList)

  public:
    WordList(Form *owner_in)
        : List(owner_in, WordFormWordsList, &s),
          s()
    {
        source = &s;
    }
    int NoMore() const
    {
        return s.NoMore();
    }
    void Reset()
    {
        s.Reset();
        dict->Reset();
    }
    void NextPage()
    {
	dict->ClearProgress();
    }
    inline const char *Start(const char *tmplate, const char *anagram)
    {
        return s.Start(tmplate, anagram);
    }
    virtual ~WordList()
    {
    }
};

#ifdef IS_FOR_PALM

//--------------------------------------------------

class MainForm: public Form
{
  protected:
    Field patfield, anafield;
    char pattern[MAX_PAT_LEN];
    char anagram[MAX_PAT_LEN];
    char recall;

    void SavePatterns();

    virtual Boolean HandleOpen();
    virtual Boolean HandleKeyDown(UInt16 chr, UInt16 keyCode, UInt16 &modifiers);
    virtual Boolean HandleSelect(UInt16 objID);
    virtual Boolean HandleMenu(UInt16 menuID);
    void Go();

  public:
    //void Restore();
    //void Save();
    MainForm();
    virtual ~MainForm()
    { 
    }
};

class WordForm : public Form
{
  protected:
  
    WordList list;
    
    virtual Boolean HandleOpen();
    virtual Boolean HandleKeyDown(UInt16 chr, UInt16 keyCode, UInt16 &modifiers);
    virtual Boolean HandleSelect(UInt16 objID);
    virtual Boolean HandleUpdate();
    
    void New();
    void NextPage();
    void Restart();
    void ShowVars();

  public:
  
    WordForm()
        : Form(WordFormForm, 1),
          list(this)
    { 
    }
    void EnableNextButton()
    {
        EnableControl(WordFormNextButton);
    }
    void DisableNextButton()
    {
        DisableControl(WordFormNextButton);
    }
    const char *Start(const char *pattern, const char *anagram = 0) 
    {
        return list.Start(pattern, anagram);
    }
};

class WordFindApplication : public Application
{
  protected:
    MainForm mainform;
    WordForm wordform;
    virtual void Stop();
  public:
    WordFindApplication();
    virtual ~WordFindApplication()
    {
    }
};

#endif // IS_FOR_PALM

//---------------------------------------------------------

const char *WordListSource::Start(const char *pattern, const char *anagram)
{
    const char *x = dict->StartConsult(pattern, anagram);
    nomore = (x != 0);
    return x;
}

UInt16 WordListSource::NumItems()
{
    return NumWordListItems;
}

Boolean WordListSource::GetItem(Int16 itemNum, Char* buf, UInt16 buflen)
{
    int x;
    (void)itemNum;
    buf[0] = buf[buflen-1] = 0;
    if (!nomore && (x = dict->NextMatch(buf, (int)buflen)) < 0)
    {
        // making assumptions about the size below! XXX
        StrIToA(buf, dict->Matches());
        strcat(buf, " matches");
        if (x == -2) strcat(buf, "(interrupted)");
        nomore = 1;
#ifdef IS_FOR_PALM
        Form *owner = Application::Instance()->GetForm();
        if (owner) ((WordForm*)owner)->DisableNextButton();
#endif
        return True;
    }
    return nomore ? False : True;
}

//------------------------------------------------------------------------

#ifdef UNIX

int debug = 0;

int main(int argc, char **argv)
{
    WordList list(0);
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
	{
	    switch (argv[i][1])
	    {
	    case 'D': ++debug; break;
	    }
	}
	else
	{
    	    list.Start(argv[i], ((i+1)<argc) ? argv[i+1] : 0));
	    break;
	}
    }
    int w = 0;
    while (!list.NoMore())
    {
        puts(list.GetItem((UInt)(w++)));
    }
    vars->Show();
}

#else

//------------------------------------------------------------------------

#ifdef IS_FOR_PALM

#if 0
void MainForm::Restore()
{
    Database config;
    pattern[0] = 0;
    recall = 0;
    if (config.OpenByType(ConfigDBType) == True)
    {
        MemHandle h = config.QueryRecord(0);
        if (h)
        {
	    unsigned char *lp = (unsigned char*)MemHandleLock(h);
	    if (lp && MemHandleSize(h) >= CFGSZ)
	    {
	    	strncpy(pattern, (const char *)lp, MAX_PAT_LEN);
	    	pattern[MAX_PAT_LEN-1] = 0;
	    	strncpy(anagram, (const char *)lp, MAX_PAT_LEN);
	    	anagram[MAX_PAT_LEN-1] = 0;
		lp += MAX_PAT_LEN;
	    }
	    MemHandleUnlock(h);
	}
	config.Close();
    }
}

void MainForm::Save()
{
    Database config;
    MemHandle h = 0;
    if (config.OpenByType(ConfigDBType, dmModeReadWrite, "plcfg") == True)
	h = config.GetRecord(0);
    if (h == 0)
	h = config.NewRecord(0, CFGSZ);
    else if (MemHandleSize(h) != CFGSZ)
        h = config.Resize(0, CFGSZ);
    if (h)
    {
	unsigned char lpbuf[CFGSZ];
	unsigned char *dp = (unsigned char*)MemHandleLock(h);
	if (dp)
	{
	    memcpy((char*)lpbuf, (const char *)dp, (unsigned)CFGSZ);
	    unsigned char *lp = lpbuf;
	    strncpy((char *)lp, (const char *)pattern, MAX_PAT_LEN);
	    lp += sizeof(pattern);
	    strncpy((char *)lp, (const char *)anagram, MAX_PAT_LEN);
	    lp += sizeof(anagram);
	    config.WriteRecord(dp, lpbuf, CFGSZ); 
	}
        MemHandleUnlock(h);
	config.ReleaseRecord(0);
	config.Close();
    }
}
#endif

MainForm::MainForm()
    : Form(MainFormForm, 2),
      patfield(this, MainFormTemplateField),
      anafield(this, MainFormAnagramField),
      recall(0)
{
    pattern[0] = anagram[0] = 0;
}

Boolean MainForm::HandleOpen()
{
    patfield.Clear();
    anafield.Clear();
    pattern[0] = anagram[0] = 0;
    (void)Form::HandleOpen();
    SetFocus(MainFormTemplateField);
    return True;
}

void MainForm::SavePatterns()
{
    patfield.Read(pattern, MAX_PAT_LEN);
    anafield.Read(anagram, MAX_PAT_LEN);
}

void MainForm::Go()
{
    SavePatterns();
    if (pattern[0] || anagram[0])
    {
        //Save();
        WordForm *wordform = (WordForm*)::GetForm(WordFormForm);
        if (wordform)
        {
            const char *x = wordform->Start(pattern, anagram);
            if (x == 0)
    	        Switch(WordFormForm);
    	    else FrmCustomAlert(ErrorAlert, x, "", "");
        }
    }
}

Boolean MainForm::HandleKeyDown(UInt16 chr, UInt16 keyCode, UInt16 &modifiers)
{
    (void)keyCode;
    (void)modifiers;
    if (chr =='\n')
    {
        Go();
	return True;
    }
    return False;
}

Boolean MainForm::HandleSelect(UInt16 objID)
{
    // need to determine which field has the focus. Breaks and wilds
    // should only be inserted if the pattern has the focus, while
    // letter buttons should be inserted in whichever field has the 
    // focus
    
    if (objID >= MainFormLetterAButton && objID <= MainFormLetterZButton)
    {
        Field *f = GetFieldWithFocus();
        if (f == 0) f = &patfield;
        f->Insert((char)('A'+objID-MainFormLetterAButton));
    }
    else switch (objID)
    {
    case MainFormTemplateClearButton:
        patfield.Clear();
	pattern[0] = 0;
        break;
    case MainFormAnagramClearButton:
        anafield.Clear();
	anagram[0] = 0;
        break;
    case MainFormBreak1Button:
        patfield.Insert('|');
        break;
    case MainFormWild1Button:
        patfield.Insert('?');
        break;
    case MainFormBreak2Button:
        anafield.Insert('|');
        break;
//    case MainFormBackspaceButton:
//        if (focid == MainFormAnagramForm)
//        else if (focid == MainFormTemplateForm)
//        break;
    case MainFormGoButton:
        Go();
	break;
    default:
	return False;
    }
    return True;
}

Boolean MainForm::HandleMenu(UInt16 menuID)
{
    if (menuID == HelpAboutWordFindLite)
    {
        FrmAlert(AboutAlert);
        return true;
    }
    return Form::HandleMenu(menuID);
}

//------------------------------------------------------------------------

Boolean WordForm::HandleOpen()
{
    if (Form::HandleOpen())
    {
    	EnableNextButton();
        DrawControl(WordFormStopButton);
//        dict->SetProgressOwner(this);
        ClearProgress();
	return True;
    }
    return False;
}

void WordForm::New()
{
    Switch(MainFormForm);
}

void WordForm::NextPage()
{
    if (!list.NoMore())
    {
        list.NextPage();
	PostUpdateEvent();
    }
}

void WordForm::Restart()
{
    list.Reset();
    EnableNextButton();
    ClearProgress();
    PostUpdateEvent();
}

Boolean WordForm::HandleKeyDown(UInt16 chr, UInt16 keyCode, UInt16 &modifiers)
{
    (void)keyCode;
    (void)modifiers;
    switch (chr)
    {
    case '\n':
    case pageDownChr:
        NextPage();
	break;
    case 'r':
    case 'R':
    case pageUpChr:
        Restart();
	break;
    case 'n':
    case 'N':
    case 8:
        New();
	break;
    default:
        return False;
    }
    return True;
}

Boolean WordForm::HandleSelect(UInt16 objID)
{
    switch (objID)
    {
    case WordFormNewButton:
        New();
	return true;
    case WordFormNextButton:
        NextPage();
	return true;
    case WordFormRestartButton:
        Restart();
	return true;
    }
    return false;
}

Boolean WordForm::HandleUpdate()
{
    list.Erase();
    return Form::HandleUpdate();
}

//------------------------------------------------------------------------

WordFindApplication::WordFindApplication()
  : Application(2, MainFormForm),
    mainform(),
    wordform()
{
//    mainform.Restore();
}

void WordFindApplication::Stop()
{
//    mainform.Save();
    Application::Stop();
}

UInt32 RunApplication(MemPtr cmdPBP, UInt16 launchflags);

UInt32 RunApplication(MemPtr cmdPBP, UInt16 launchflags)
{
    UInt32 rtn = 0;
    // dict must be created before the application is, so that it
    // can be passed to the dictlist in main form
    dict = new WordFindDict();
    if (dict)
    {
        WordFindApplication *app = new WordFindApplication();
	rtn = app ? app->Main(cmdPBP, launchflags) : 0;
	delete app;
	delete dict;
	dict = 0;
    }
    return rtn;
}

#endif // IS_FOR_PALM
#endif // !UNIX

