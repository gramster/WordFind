// TODO: help
// Robustness with bad patterns, overruns
// Add a dict info dialog showing word length distributions 
//	and total number of words.

#define HISTORY
#define PERSIST

#include "mypilot.h"
#ifndef TEST
#include "ptrsc.h"
#include "fw.h"
#endif

#include "dict.h"

// derive the application

#define CLIENT_DB_TYPE		(0)

#define MAX_PAT_LEN		(64)

#define ConfigDBType		((ULong)'pwcf')

//#define NumHistoryItems		(0)
#define NumHistoryItems		(10)
#define NumWordListItems	(11)

#define CFGSZ			(7*sizeof(UInt)+((NumHistoryItems+1)*MAX_PAT_LEN))

//---------------------------------------------------------

class PalmWordDict : public DictionaryDatabase
{
  protected:
    virtual int MustStop();
  public:
#ifdef TEST
    PalmWordDict() : DictionaryDatabase() {}
#else
    PalmWordDict(class Form *owner_in) : DictionaryDatabase(owner_in) {}
#endif
    virtual ~PalmWordDict() {}
};

int PalmWordDict::MustStop()
{
#ifdef TEST
    return DictionaryDatabase::MustStop();
#else
    short x, y;
    Boolean pendown;
    EvtGetPen(&x, &y, &pendown);
    return (pendown && x>120 && y > 143);
#endif
}

class WordList
#ifndef TEST
    : public List
#endif
{
  protected:
    PalmWordDict &dict;
    char line[30];
    int nomore;
    int page;
    virtual int NumItems();

  public:
    virtual char *GetItem(UInt itemNum);
    WordList(Form *owner_in, PalmWordDict &dict_in)
       : List(owner_in, WordFormWordsList),
         dict(dict_in),
         nomore(0),
         page(0)
    {
        line[0] = 0;
    }
    int NoMore() const
    {
        return nomore;
    }
    void Reset()
    {
        nomore = page = 0;
        dict.Reset();
    }
    void NextPage()
    {
        ++page;
	dict.ClearProgress();
    }
    int Start(char *pat, UInt matchtype, UInt multi,
    			UInt minlength, UInt maxlength,
			UInt mincount, UInt maxcount);
};

#ifndef TEST

class MatchTypeDialog : public RadioGroupDialog
{
    UInt val;
  public:
    MatchTypeDialog(Form *parent_in, UInt val_in)
        : RadioGroupDialog(parent_in,
        	    	MatchTypeForm,
        	    	MatchTypeNormalCheckbox,
        	    	3,
        	    	val_in)
    {}
};

class WordLengthDialog : public RadioGroupDialog
{
  public:
    WordLengthDialog(Form *parent_in, UInt val_in, CharPtr title_in)
        : RadioGroupDialog(parent_in, 
        		   WordLengthForm,
        		   WordLengthNoLengthLimitCheckbox,
        		   14,
        		   val_in,
        		   title_in)
    {}
};

class WordCountDialog : public RadioGroupDialog
{
  public:
    WordCountDialog(Form *parent_in, UInt val_in, CharPtr title_in)
        : RadioGroupDialog(parent_in,
        	    	 WordCountForm,
        	    	 WordCountNoCountLimitCheckbox,
        	    	 14,
        	    	 val_in,
        	    	 title_in)
    {}
};

#ifdef HISTORY

class HistoryList : public List
{
  protected:
#ifdef TEST
    char *history[NumHistoryItems];
#else
    VoidHand history[NumHistoryItems];
#endif
    UInt pos;
    char rtn[MAX_PAT_LEN];
  public:
    HistoryList(Form *owner_in);
    void Set(UInt pos, const char *pat, UInt maxlen);
    void Add(const char *pat)
    {
        UInt idx = (pos+NumHistoryItems-1)%NumHistoryItems;
	if (history[idx])
	{
	    char *s = (char *)MemHandleLock(history[idx]);
	    int isdup = (s && strncmp(s, pat, MAX_PAT_LEN) == 0);
	    MemHandleUnlock(history[idx]);
	    if (!isdup) (void)Set(pos, pat, MAX_PAT_LEN);
	    pos = (pos+1)%NumHistoryItems;
	}
    }
    virtual int NumItems()
    {
        return NumHistoryItems;
    }
    virtual char *GetItem(UInt idx)
    {
        UInt p = ((NumHistoryItems-1) + pos-idx)%NumHistoryItems;
	HMemCopy(rtn, history[p], MAX_PAT_LEN);
	return rtn;
    }
    void Save(unsigned char *ptr) const;
    void Restore(unsigned char *ptr);
    virtual ~HistoryList()
    {
        for (int i = 0; i < NumHistoryItems; i++)
	    if (history[i])
	        MemHandleFree(history[i]);
    }
};

HistoryList::HistoryList(Form *owner_in)
    : List(owner_in, MainFormHistoryList)
{
    for (int i = 0; i < NumHistoryItems; i++)
	history[i] = 0;
    pos = 0;
}

void HistoryList::Set(UInt pos, const char *pat, UInt maxlen)
{
    if (history[pos])
	MemHandleFree(history[pos]);
    UInt l = strlen(pat)+1;
    if (l > maxlen) l = maxlen;
    VoidHand h = history[pos] = MemHandleNew(l);
    if (h)
    {
        char *s = (char *)MemHandleLock(h);
	if  (s) 
	{
	    strncpy(s, pat, (UInt)(l-1));
	    s[l-1] = 0;
	}
	MemHandleUnlock(h);
    }
}

void HistoryList::Save(unsigned char *ptr) const
{
    for (UInt i = 0; i < NumHistoryItems; i++, ptr += MAX_PAT_LEN)
    {
	strncpy((char*)ptr, (const char *)GetItem(i), MAX_PAT_LEN-1);
	ptr[MAX_PAT_LEN-1] = 0;
    }
    *((UInt*)ptr) = pos;
}

void HistoryList::Restore(unsigned char *ptr)
{
    for (UInt i = 0; i < NumHistoryItems; i++, ptr += MAX_PAT_LEN)
    {
        Set(i, (const char *)ptr, MAX_PAT_LEN);
    }
    pos = *((UInt*)ptr);
}

#endif // HISTORY

class MainForm: public Form
{
  protected:
    UInt minlength, maxlength, mincount, maxcount;
    UInt matchtype, multi;
    char pattern[MAX_PAT_LEN];
#ifdef HISTORY
    HistoryList history;
    virtual List *GetList()
    {
        return &history;
    }
#endif
    enum { LOWER, UPPER } letter_case;
    void SetModalLabels();
    virtual void Init();
    virtual Boolean Activate();
    virtual Boolean HandleSelect(UInt objID);
    virtual Boolean HandlePopupListSelect(UInt triggerID,
					UInt listID,
					UInt selection);
    virtual Boolean HandleMenu(UInt menuID);

  public:
    void Restore();
    void Save();
    MainForm();
};

class WordForm : public Form
{
  protected:
  
    PalmWordDict dict;
    WordList list;
    
    virtual void Init();
    virtual Boolean Activate();
    virtual Boolean Open();
    virtual Boolean HandleSelect(UInt objID);
    virtual Boolean Update();
    
  public:
  
    WordForm()
        : Form(WordFormForm),
          dict(this),
          list(this, dict)
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
    void ClearProgress();
    virtual void ShowProgress(int p);
    void ShowDebug(int sp, unsigned long n);
    virtual List *GetList()
    {
        return &list;
    }
    int Start(CharPtr pat, UInt matchtype, UInt multi,
    			UInt minlength, UInt maxlength, 
			UInt mincount, UInt maxcount)
    {
        return list.Start(pat, matchtype, multi, minlength, maxlength,
      			mincount, maxcount);
    }
};

class PalmWordApplication : public Application
{
  protected:
    MainForm mainform;
    WordForm wordform;
    virtual Form *TopForm();
    virtual void Stop();
  public:
    PalmWordApplication();
    virtual Form *GetForm(UInt formID);
};

#endif // TEST


//---------------------------------------------------------

int WordList::Start(char *pat, UInt matchtype, UInt multi,
			UInt minlength, UInt maxlength, 
			UInt mincount, UInt maxcount)
{
    if (dict.StartConsult(pat, (int)matchtype, (int)multi,
        				(int)minlength, (int)maxlength,
        				(int)mincount, (int)maxcount) != 0)
    {
        nomore = 1;
        return -1;
    }
    nomore = page = 0;
    return 0;
}

int WordList::NumItems()
{
    return NumWordListItems;
}

char *WordList::GetItem(UInt itemNum)
{
    int x;
    (void)itemNum;
    line[0] = line[MAX_PAT_LEN-1] = 0;
    if (!nomore && (x = dict.NextMatch(line, MAX_PAT_LEN)) < 0)
    {
        strcpy(line, ltoa(dict.Matches()));
        strcat(line, " matches");
        if (x == -2) strcat(line, "(interrupted)");
        nomore = 1;
#ifndef TEST
        ((WordForm*)owner)->DisableNextButton();
#endif
    }
    return line;
}

//------------------------------------------------------------------------

#ifdef TEST

int main(int argc, char **argv)
{
    PalmWordDict dict;
    WordList list(0, dict);
    list.Start((argc==2 ? argv[1] : ":.:"), USEALL, 0, 0, 0, 0, 0);
    int i = 0;
    while (!list.NoMore())
    {
        puts(list.GetItem((UInt)(i++)));
    }
}

#else

//------------------------------------------------------------------------

PalmWordApplication::PalmWordApplication()
  : Application(), mainform(), wordform()
{
//    MemSetDebugMode(memDebugModeCheckOnAll);
}

Form *PalmWordApplication::GetForm(UInt formID)
{
    switch (formID)
    {
    case MainFormForm:
        return &mainform;
    case WordFormForm:
	return &wordform;
    }
    return 0;
}

//------------------------------------------------------------------------

MainForm::MainForm()
    : Form(MainFormForm),
      minlength(0), maxlength(0), mincount(0), maxcount(0),
      matchtype(USEALL), multi(0),
#ifdef HISTORY
      history(this),
#endif
      letter_case(UPPER)
{
    pattern[0] = 0;
    Restore();
}

void MainForm::SetModalLabels()
{
    SetCheckBox(MainFormMultiCheckbox, multi);
    SetField(MainFormPatternField, pattern);
    SetControlLabel(MainFormMatchTypeSelTrigger, DictionaryDatabase::MatchTypeName(matchtype));
    SetControlLabel(MainFormMinLengthSelTrigger, DictionaryDatabase::LimitName(minlength));
    SetControlLabel(MainFormMaxLengthSelTrigger, DictionaryDatabase::LimitName(maxlength));
    SetControlLabel(MainFormMinCountSelTrigger, DictionaryDatabase::LimitName(mincount));
    SetControlLabel(MainFormMaxCountSelTrigger, DictionaryDatabase::LimitName(maxcount));
    SetControlLabel(MainFormCaseButton, (letter_case==LOWER?"Floating Mode":"Anchored Mode"));
    FrmSetFocus(frm, FrmGetObjectIndex(frm, MainFormPatternField));
    Boolean caps, num, aut;
    Word temp;
    GrfGetState(&caps, &num, &temp, &aut);
    caps = (letter_case==LOWER)?False:True;
    GrfSetState(caps, num, 0);
}

void MainForm::Init()
{
    Form::Init();
#ifdef HISTORY
    history.Init();
#endif
}

Boolean MainForm::Activate()
{
    if (Form::Activate())
    {
        SetModalLabels();
#ifdef HISTORY
	return history.Activate();
#else
	ControlPtr b = (ControlPtr)GetObject(MainFormHistoryPopTrigger);
        if (b) CtlSetUsable(b, 0);
        return True;
#endif
    }
    return False;
}

Boolean MainForm::HandleSelect(UInt objID)
{
    CharPtr pat;
    switch (objID)
    {
    case MainFormMatchTypeSelTrigger:
    	matchtype = MatchTypeDialog(this, matchtype).Run();
	SetControlLabel(MainFormMatchTypeSelTrigger, DictionaryDatabase::MatchTypeName(matchtype));
	break;
    case MainFormMinLengthSelTrigger:
	minlength = WordLengthDialog(this, minlength, "Min Word Length").Run();
	SetControlLabel(MainFormMinLengthSelTrigger, DictionaryDatabase::LimitName(minlength));
	break;
    case MainFormMaxLengthSelTrigger:
	maxlength = WordLengthDialog(this, maxlength, "Max Word Length").Run();
	SetControlLabel(MainFormMaxLengthSelTrigger, DictionaryDatabase::LimitName(maxlength));
	break;
    case MainFormMinCountSelTrigger:
	mincount = WordCountDialog(this, mincount, "Min Word Count").Run();
    	SetControlLabel(MainFormMinCountSelTrigger, DictionaryDatabase::LimitName(mincount));
    	break;
    case MainFormMaxCountSelTrigger:
	maxcount = WordCountDialog(this, maxcount, "Max Word Count").Run();
        SetControlLabel(MainFormMaxCountSelTrigger, DictionaryDatabase::LimitName(maxcount));
	break;
    case MainFormCaseButton: // toggle case and labels
	letter_case = (letter_case==LOWER) ? UPPER : LOWER;
	SetModalLabels();
	break;
    case MainFormClearButton:
        ClearField(MainFormPatternField);
        break;
    case MainFormWildButton:
        InsertInField(MainFormPatternField, (letter_case == LOWER) ? '.' : ':');
        break;
    case MainFormVowelButton:
        InsertInField(MainFormPatternField, (letter_case == LOWER) ? '+' : '*');
        break;
    case MainFormConsonantButton:
        InsertInField(MainFormPatternField, (letter_case == LOWER) ? '-' : '=');
        break;
    case MainFormLeftBracketButton:
        InsertInField(MainFormPatternField, '[');
        break;
    case MainFormRightBracketButton:
        InsertInField(MainFormPatternField, ']');
        break;
    case MainFormNegateButton:
        InsertInField(MainFormPatternField, '!');
        break;
    case MainFormMultiCheckbox:
        multi = !multi;
	break;
    case MainFormGoButton:
	pat = ReadField(MainFormPatternField);
	if (pat && pat[0])
    	{
            strncpy(pattern, (const char *)pat, MAX_PAT_LEN-1);
            pattern[MAX_PAT_LEN-1] = 0;
	    if (pattern[0])
	    {
#ifdef HISTORY
	        history.Add(pattern);
#endif
		Save();
	        Application *a = Application::Instance();
                WordForm *wordform = a ? (WordForm*)a->GetForm(WordFormForm) : 0;
                if (wordform)
                {
                    if (wordform->Start(pattern, matchtype, multi, 
		    			minlength, maxlength,
                			mincount, maxcount) == 0)
					wordform->PostLoadEvent();
		    else
			FrmAlert(NoDictBoxAlert); 
                }
            }
	}
	break;
    default:
	return False;
    }
    return True;
}

Boolean MainForm::HandlePopupListSelect(UInt triggerID,
					UInt listID,
					UInt selection)
{
    (void)triggerID;
    (void)listID;
#ifdef HISTORY
    const char *text = history.GetItem(selection);
    if (text) strcpy(pattern, text);
    SetControlLabel(MainFormHistoryPopTrigger, "History");
    DrawControl(MainFormHistoryPopTrigger);
#endif
    SetModalLabels();
    return True; 
}

Boolean MainForm::HandleMenu(UInt menuID)
{
    switch(menuID)
    {
    case HelpAboutPalmWord:
        FrmAlert(AboutBoxAlert);
        return true;
    case EditUndo:
    case EditCut:
    case EditCopy:
    case EditPaste:
    case EditSelectAll:
      {
	Word focus = FrmGetFocus(frm);
        if (focus == noFocus)
            return false;
        FormObjectKind objtype = FrmGetObjectType(frm, focus);
        if (objtype != frmFieldObj)
            return false;
        FieldPtr fld = (FieldPtr)FrmGetObjectPtr(frm, focus);
	if (!fld) return false;
	switch(menuID)
	{
    	case EditUndo:
    	    FldUndo(fld);
    	    break;
    	case EditCut:
    	    FldCut(fld);
    	    break;
    	case EditCopy:
    	    FldCopy(fld);
    	    break;
    	case EditPaste:
    	    FldPaste(fld);
    	    break;
    	case EditSelectAll:
	    FldSetSelection(fld, 0, FldGetTextLength(fld));
	    break;
      	}
      	return true;
      }
    case EditKeyboard:
        SysKeyboardDialog(kbdDefault);
	return true;
    case EditGraffiti:
        SysGraffitiReferenceDialog(referenceDefault);
	return true;
    }
    return false;
}

void MainForm::Restore()
{
#ifdef PERSIST
    Database config;
    if (config.Open(ConfigDBType) == True)
    {
        VoidHand h = config.QueryRecord(0);
        if (h)
        {
	    unsigned char *lp = (unsigned char*)MemHandleLock(h);
	    if (lp && MemHandleSize(h) >= CFGSZ)
	    {
	        minlength = *((UInt*)lp)++;
	        maxlength = *((UInt*)lp)++;
	        mincount = *((UInt*)lp)++;
	        maxcount = *((UInt*)lp)++;
	        matchtype = *((UInt*)lp)++;
	        multi = *((UInt*)lp)++;
	    	strncpy(pattern, (const char *)lp, MAX_PAT_LEN);
	    	pattern[MAX_PAT_LEN-1] = 0;
#ifdef HISTORY
		lp += MAX_PAT_LEN;
		history.Restore(lp);
#endif
		// still to do - WordForm stack context
	    }
	    MemHandleUnlock(h);
	}
    }
#endif
}

void MainForm::Save()
{
#ifdef PERSIST
    Database config;
    VoidHand h = 0;
    if (config.Open(ConfigDBType) == True)
	h = config.GetRecord(0);
    else if (config.Create("pwcfg", ConfigDBType) != True)
        return;
    if (h == 0)
	h = config.NewRecord(0, CFGSZ);
    else if (MemHandleSize(h) != CFGSZ)
        h = config.Resize(0, CFGSZ);
    if (h)
    {
	CharPtr pat = ReadField(MainFormPatternField);
	if (pat==0) pat=pattern; // last known value
	unsigned char lpbuf[CFGSZ];
	unsigned char *dp = (unsigned char*)MemHandleLock(h);
	if (dp)
	{
	    memcpy((char*)lpbuf, (const char *)dp, (unsigned)CFGSZ);
	    unsigned char *lp = lpbuf;
	    *((UInt*)lp)++ = minlength;
	    *((UInt*)lp)++ = maxlength;
	    *((UInt*)lp)++ = mincount;
	    *((UInt*)lp)++ = maxcount;
	    *((UInt*)lp)++ = matchtype;
	    *((UInt*)lp)++ = multi;
	    strncpy((char *)lp, (const char *)pat, MAX_PAT_LEN);
#ifdef HISTORY
	    lp += sizeof(pattern);
	    history.Save(lp);
#endif
	    // still to do - WordForm stack context
	    config.WriteRecord(dp, lpbuf, CFGSZ); 
	}
        MemHandleUnlock(h);
	config.ReleaseRecord(0);
    }
#endif
}

//------------------------------------------------------------------------

void WordForm::Init()
{
    Form::Init();
    EnableNextButton();
    list.Init();
}

Boolean WordForm::Activate()
{
    if (Form::Activate())
        return list.Activate();
    return False;
}

Boolean WordForm::Open()
{
    if (Form::Open())
    {
        ControlPtr cm = (ControlPtr)GetObject(WordFormStopButton);
        if (cm) CtlDrawControl(cm);
        ClearProgress();
	return True;
    }
    return False;
}

void WordForm::ClearProgress()
{
    ClearRectangle(153, 15, 5, 120);
}

void WordForm::ShowProgress(int p)
{
    DrawRectangle(153, 15, 5, p);
}

void WordForm::ShowDebug(int sp, unsigned long n)
{
    static char l[40];
    strcpy(l, "sp ");
    strcat(l, ltoa((unsigned long)sp));
    strcat(l, "  n ");
    strcat(l, ltoa(n));
    FieldPtr fld = (FieldPtr)GetObject(WordFormDebugField);
    if (fld)
    {
        FldSetTextPtr(fld, l);
        FldSetDirty(fld, 1);
        FldDrawField(fld);
    }
}

Boolean WordForm::HandleSelect(UInt objID)
{
    switch (objID)
    {
    case WordFormNewButton:
	Switch(MainFormForm);
	return true;
    case WordFormNextButton:
        if (!list.NoMore())
        {
            list.NextPage();
	    PostUpdateEvent();
	}
	return true;
    case WordFormRestartButton:
	list.Reset();
	EnableNextButton();
	ClearProgress();
	PostUpdateEvent();
	return true;
    }
    return false;
}

Boolean WordForm::Update()
{
    list.Erase();
    return Form::Update();
}

Form *PalmWordApplication::TopForm()
{
    return &mainform;
}

void PalmWordApplication::Stop()
{
    mainform.Save();
    Application::Stop();
}

DWord RunApplication(Word cmd, Ptr cmdPBP, Word launchflags)
{
    PalmWordApplication app;
    return app.Main(cmd, cmdPBP, launchflags);
}

#endif // TEST


