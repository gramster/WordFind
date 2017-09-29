// TODO: help
// Robustness with bad patterns, overruns
// Add a dict info dialog showing word length distributions 
//	and total number of words.

#define PERSIST
#define VARVECT

#include "mypilot.h"
#ifdef IS_FOR_PALM
#include "ptrsc.h"
#include "fw.h"
#endif

#include "dict.h"
#include "history.h"

// derive the application

#define CLIENT_DB_TYPE		(0)

#define ConfigDBType		((ULong)'pwcf')

#define NumWordListItems	(11)

#define CFGSZ			(7*sizeof(UInt)+\
				(NumHistoryItems+1)*MAX_PAT_LEN+\
				 26*sizeof(char))

// Globals - bad in C++ but we need to avoid stack/heap usage

UInt minlength, maxlength, mincount, maxcount;
UInt matchtype, multi;
char pattern[MAX_PAT_LEN];
enum { LOWER, UPPER } letter_case;
VariableSet vars;
History history;

class PalmWordDict : public DictionaryDatabase
{
  protected:
    virtual int MustStop();
  public:
    PalmWordDict() : DictionaryDatabase() {}
    virtual ~PalmWordDict() {}
};

int PalmWordDict::MustStop()
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

PalmWordDict dict;

//---------------------------------------------------------

// useful utility

Form *GetForm(UInt id)
{
    Application *a = Application::Instance();
    return a ? a->GetForm(id) : 0;
}

class WordList
#ifdef IS_FOR_PALM
    : public List
#endif
{
  protected:
    char line[30];
    int nomore;
    int page;

    virtual int NumItems();

  public:
    virtual char *GetItem(UInt itemNum);
    WordList(class Form *owner_in);
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
    int Start();
    virtual ~WordList()
    {
    }
};

WordList::WordList(class Form *owner_in) :
#ifdef IS_FOR_PALM
   List(owner_in, WordFormWordsList),
#endif
   nomore(0),
   page(0)
{
    line[0] = 0;
}

#ifdef IS_FOR_PALM

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
        		   16,
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

class VarAssignDialog : public RadioGroupDialog
{
  public:
    VarAssignDialog(Form *parent_in, UInt val_in)
        : RadioGroupDialog(parent_in, 
        		   VarAssignForm,
        		   VarAssignNoAssignmentCheckbox,
        		   27,
        		   val_in,
        		   "Variable Assignment")
    {}
};

class HistoryList : public List
{
  public:
    HistoryList(Form *owner_in)
      : List(owner_in, MainFormHistoryList)
    { }
    virtual int NumItems()
    {
        return history.NumItems();
    }
    virtual char *GetItem(UInt idx)
    {
	return history.GetItem(idx);
    }
    virtual ~HistoryList()
    {}
};

class MainForm: public Form
{
  protected:
    HistoryList historylist;

    virtual List *GetList()
    {
        return &historylist;
    }
    void SavePattern();
    void SetModalLabels();
    void SetWordLengthModalControls();
    void SetWordCountModalControls();
    void SetLetterMode();

    virtual Boolean Open();
    virtual Boolean HandleSelect(UInt objID);
    virtual Boolean HandlePopupListSelect(UInt triggerID,
					UInt listID,
					UInt selection);
    virtual Boolean HandleMenu(UInt menuID);

  public:
    void Restore();
    void Save();
    MainForm();
    virtual void PostHandle(EventType &event);
    virtual ~MainForm()
    { }
};

class VariableList : public List
{
  protected:
    char rtn[40];
  public:
    VariableList(Form *owner_in)
	: List(owner_in, VariableFormVariableList)
    { }
    virtual int NumItems()
    {
        return 26;
    }
    void ResetConstraints()
    {
        vars.ResetConstraints();
    }
    void ClearAssignments()
    {
        vars.ClearAssignments();
    }
    virtual char *GetItem(UInt idx)
    {
        vars.Print(rtn, (int)idx);
	return rtn;
    }
    virtual Boolean HandleSelect(UInt sel)
    {
	char olda = vars.Assignment((int)sel);
	if (olda) olda-='A'-1;
	int x = (int)VarAssignDialog(owner, (UInt)olda).Run();
	vars.Assign((int)sel, (char)((x ? (x+'A'-1) : 0)));
	return List::HandleSelect(sel);
    }
    virtual ~VariableList()
    {
    }
};

class VariableForm : public Form
{
  protected:
    VariableList list;
    UInt caller;
    virtual Boolean HandleSelect(UInt objID)
    {
        switch (objID)
        {
        case VariableFormDoneButton:
	    Switch(caller);
	    return True;
        case VariableFormClearButton:
	    list.ClearAssignments();
	    Switch(caller);
	    return True;
        case VariableFormResetButton:
	    list.ResetConstraints();
	    Switch(caller);
	    return True;
        }
	return False;
    }
  public:
    VariableForm()
        : Form(VariableFormForm),
          list(this),
	  caller(MainFormForm)
    {}
    void SetCaller(UInt caller_in)
    {
        caller = caller_in;
    }
    virtual Boolean HandleListSelect(UInt listID, UInt selection)
    {
        (void)listID;
        (void)list.HandleSelect(selection);
	Draw();
	return False;
    }
    virtual List *GetList()
    {
        return &list;
    }
};

class WordForm : public Form
{
  protected:
  
    WordList list;
    
    virtual void Init();
    virtual Boolean Open();
    virtual Boolean HandleSelect(UInt objID);
    virtual Boolean Update();
    
  public:
  
    WordForm()
        : Form(WordFormForm),
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
    void ClearProgress();
    virtual void ShowProgress(int p);
    void ShowDebug(int sp, unsigned long n);
    virtual List *GetList()
    {
        return &list;
    }
    int Start()
    {
        return list.Start();
    }
};

class PalmWordApplication : public Application
{
  protected:
    MainForm mainform;
    WordForm wordform;
    VariableForm variableform;
    virtual Form *TopForm();
    virtual void Stop();
  public:
    PalmWordApplication();
    virtual Form *GetForm(UInt formID);
    virtual ~PalmWordApplication()
    {
    }
};

#endif // IS_FOR_PALM

//---------------------------------------------------------

int WordList::Start()
{
    if (dict.StartConsult(pattern, (int)matchtype, (int)multi,
        				(int)minlength, (int)maxlength,
        				(int)mincount, (int)maxcount, 
					&vars) != 0)
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
#ifdef IS_FOR_PALM
        ((WordForm*)owner)->DisableNextButton();
#endif
    }
    return line;
}

//------------------------------------------------------------------------

#ifdef UNIX

int debug = 0;

int main(int argc, char **argv)
{
    WordList list(0);
    int typ = USEALL, multi = 0;
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
	{
	    switch (argv[i][1])
	    {
	    case 'a': typ = USEALL; break;
	    case 'p': typ = PREFIX; break;
	    case 's': typ = SUFFIX; break;
	    case 'm': multi = 1; break;
	    case 'D': ++debug; break;
	    }
	}
	else
	{
    	    list.Start(argv[i], typ, multi, 0, 0, 0, 0, &vars);
	    break;
	}
    }
    int w = 0;
    while (!list.NoMore())
    {
        puts(list.GetItem((UInt)(w++)));
    }
    vars.Show();
}

#else

//------------------------------------------------------------------------

#ifdef IS_FOR_PALM

void /*MainForm::*/Restore()
{
#ifdef PERSIST
    Database config;
    pattern[0] = 0;
    minlength = maxlength = mincount = maxcount = 0;
    matchtype = USEALL;
    multi = 0;
    letter_case = UPPER;
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
		lp += MAX_PAT_LEN;
		lp = history.Restore(lp);
#ifdef VARVECT
		for (int i = 0; i < 26; i++)
	            vars.Assign(i, *((char*)lp)++);
#endif
	    }

	    MemHandleUnlock(h);
	    if (matchtype == ALL)
		minlength = maxlength = 0;
	}
    }
#endif
}

void /*MainForm::*/Save()
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
#if 0
	CharPtr pat = ReadField(MainFormPatternField);
	if (pat==0) pat=pattern; // last known value
#else
	CharPtr pat = pattern;
#endif
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
	    lp += sizeof(pattern);
	    lp = history.Save(lp);
#ifdef VARVECT
	    for (int i = 0; i < 26; i++)
	        *((char*)lp)++ = vars.Assignment(i);
#endif

	    // still to do - WordForm stack context
	    config.WriteRecord(dp, lpbuf, CFGSZ); 
	}
        MemHandleUnlock(h);
	config.ReleaseRecord(0);
    }
#endif
}

MainForm::MainForm()
    : Form(MainFormForm),
      historylist(this)
{
}

void MainForm::SetLetterMode()
{
    Boolean caps, num, aut;
    Word temp;
    GrfGetState(&caps, &num, &temp, &aut);
    caps = (letter_case==LOWER)?False:True;
    GrfSetState(caps, num, False);
}

void MainForm::SetWordLengthModalControls()
{
    int cando = (multi || matchtype != USEALL);
    if (!cando) minlength = maxlength = 0;
    SetControlLabel(MainFormMinLengthSelTrigger, DictionaryDatabase::LimitName(minlength));
    SetControlLabel(MainFormMaxLengthSelTrigger, DictionaryDatabase::LimitName(maxlength));
    EnableControl(MainFormMinLengthSelTrigger, cando);
    EnableControl(MainFormMaxLengthSelTrigger, cando);
}

void MainForm::SetWordCountModalControls()
{
    if (!multi) mincount = maxcount = 0;
    SetControlLabel(MainFormMinCountSelTrigger, DictionaryDatabase::LimitName(mincount));
    SetControlLabel(MainFormMaxCountSelTrigger, DictionaryDatabase::LimitName(maxcount));
    EnableControl(MainFormMinCountSelTrigger, (int)multi);
    EnableControl(MainFormMaxCountSelTrigger, (int)multi);
}

void MainForm::SetModalLabels()
{
    SetCheckBox(MainFormMultiCheckbox, multi);
    SetField(MainFormPatternField, pattern);
    SetControlLabel(MainFormMatchTypeSelTrigger, DictionaryDatabase::MatchTypeName(matchtype));
    SetWordCountModalControls();
    SetWordLengthModalControls();
    SetControlLabel(MainFormCaseButton, (letter_case==LOWER?"Floating":"Anchored"));
    FrmSetFocus(frm, FrmGetObjectIndex(frm, MainFormPatternField));
    SetLetterMode();
}

void MainForm::PostHandle(EventType &event)
{
    if (event.eType == keyDownEvent)
        SetLetterMode();    
}

Boolean MainForm::Open()
{
    SetModalLabels();
    return Form::Open();
}

void MainForm::SavePattern()
{
    CharPtr pat = ReadField(MainFormPatternField);
    if (pat && pat[0])
    {
        strncpy(pattern, (const char *)pat, MAX_PAT_LEN-1);
        pattern[MAX_PAT_LEN-1] = 0;
    }
}

Boolean MainForm::HandleSelect(UInt objID)
{
    switch (objID)
    {
    case MainFormMatchTypeSelTrigger:
    	matchtype = MatchTypeDialog(this, matchtype).Run();
	SetControlLabel(MainFormMatchTypeSelTrigger, DictionaryDatabase::MatchTypeName(matchtype));
	SetWordLengthModalControls();
	break;
    case MainFormMinLengthSelTrigger:
        //if (matchtype == USEALL) break;
	minlength = WordLengthDialog(this, minlength, "Min Word Length").Run();
	SetControlLabel(MainFormMinLengthSelTrigger, DictionaryDatabase::LimitName(minlength));
	break;
    case MainFormMaxLengthSelTrigger:
        //if (matchtype == USEALL) break;
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
	SavePattern();
	SetModalLabels();
	break;
    case MainFormClearButton:
        ClearField(MainFormPatternField);
	pattern[0] = 0;
        break;
    case MainFormBreakButton:
        InsertInField(MainFormPatternField, '|');
        break;
    case MainFormSpaceButton:
        InsertInField(MainFormPatternField, ' ');
        break;
    case MainFormSlashButton:
        InsertInField(MainFormPatternField, '/');
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
    case MainFormRangeButton:
        InsertInField(MainFormPatternField, '-');
        break;
    case MainFormNegateButton:
        InsertInField(MainFormPatternField, '^');
        break;
    case MainFormMultiCheckbox:
        multi = !multi;
	SetWordCountModalControls();
	break;
    case MainFormGoButton:
	SavePattern();
	if (pattern[0])
	{
	    history.Add(pattern);
	    ::Save();
            WordForm *wordform = (WordForm*)::GetForm(WordFormForm);
            if (wordform)
            {
                if (wordform->Start() == 0)
		    wordform->PostLoadEvent();
		else
		    FrmAlert(NoDictBoxAlert); 
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
    (void)listID;
    if (triggerID == MainFormHistoryPopTrigger)
    {
        const char *text = historylist.GetItem(selection);
        if (text) strcpy(pattern, text);
        SetControlLabel(MainFormHistoryPopTrigger, "History");
        DrawControl(MainFormHistoryPopTrigger);
        SetModalLabels();
    }
    else if (triggerID == MainFormDictionaryPopTrigger)
    {
        dict.Reopen(dictlist.GetDB(selection));
//	dict.Init();
    }
    return True; 
}

Boolean MainForm::HandleMenu(UInt menuID)
{
    VariableForm *f;
    switch(menuID)
    {
    case HelpAboutPalmWord:
        FrmAlert(AboutBoxAlert);
        return true;
    case EditClearAssignments:
#ifdef VARVECT
	vars.ClearAssignments();
#endif
        return true;
    case EditResetConstraints:
#ifdef VARVECT
        vars.ResetConstraints();
#endif
        return true;
    case EditShowConstraints:
#ifdef VARVECT
	f = (VariableForm*)GetForm(VariableFormForm);
	if (f) 
	{
	    f->SetCaller(MainFormForm);
	    Switch(VariableFormForm);
	}
#endif
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

//------------------------------------------------------------------------

void WordForm::Init()
{
    Form::Init();
    EnableNextButton();
}

Boolean WordForm::Open()
{
    if (Form::Open())
    {
        DrawControl(WordFormStopButton);
        dict.SetProgressOwner(this);
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
    VariableForm *f;
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
    case WordFormVarsButton:
	f = (VariableForm*)GetForm(VariableFormForm);
	if (f) 
	{
	    list.Reset();
	    EnableNextButton();
	    ClearProgress();
	    f->SetCaller(WordFormForm);
	    Switch(VariableFormForm);
	}
        return true;
    }
    return false;
}

Boolean WordForm::Update()
{
    list.Erase();
    return Form::Update();
}

//------------------------------------------------------------------------

PalmWordApplication::PalmWordApplication()
  : Application(),
    mainform(),
    wordform(),
    variableform()
{
//    MemSetDebugMode(memDebugModeCheckOnAll);
    /*mainform.*/Restore();
}

Form *PalmWordApplication::GetForm(UInt formID)
{
    switch (formID)
    {
    case MainFormForm:
        return &mainform;
    case WordFormForm:
	return &wordform;
    case VariableFormForm:
	return &variableform;
    }
    return 0;
}

Form *PalmWordApplication::TopForm()
{
    return &mainform;
}

void PalmWordApplication::Stop()
{
    /*mainform.*/Save();
    Application::Stop();
}

DWord RunApplication(Word cmd, Ptr cmdPBP, Word launchflags)
{
    history.Init();
    vars.Init();
    dict.Init();
    PalmWordApplication app;
    return app.Main(cmd, cmdPBP, launchflags);
}

#endif // IS_FOR_PALM
#endif // !UNIX

