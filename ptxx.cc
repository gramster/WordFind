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

// derive the application

#define CLIENT_DB_TYPE		(0)

#define MAX_PAT_LEN		(64)

#define ConfigDBType		((ULong)'pwcf')

//#define NumHistoryItems		(0)
#define NumHistoryItems		(10)
#define NumWordListItems	(11)

#define CFGSZ			(7*sizeof(UInt)+\
				(NumHistoryItems+1)*MAX_PAT_LEN+\
				 26*sizeof(char))

//---------------------------------------------------------

// useful utility

Form *GetForm(UInt id)
{
    Application *a = Application::Instance();
    return a ? a->GetForm(id) : 0;
}

class PalmWordDict : public DictionaryDatabase
{
  protected:
    virtual int MustStop();
  public:
#ifdef IS_FOR_PALM
    PalmWordDict(class Form *owner_in) : DictionaryDatabase(owner_in) {}
#else
    PalmWordDict() : DictionaryDatabase() {}
#endif
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

class WordList
#ifdef IS_FOR_PALM
    : public List
#endif
{
  protected:
    PalmWordDict *dict;
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
        if (dict) dict->Reset();
    }
    void NextPage()
    {
        ++page;
	if (dict) dict->ClearProgress();
    }
    int Start(PalmWordDict *dict_in,
			char *pat, UInt matchtype, UInt multi,
    			UInt minlength, UInt maxlength,
			UInt mincount, UInt maxcount,
			VariableSet *vars);
    VariableSet *GetVariables()
    {
        return dict->GetVariables();
    }
    virtual ~WordList()
    {
	delete dict;
    }
};

WordList::WordList(class Form *owner_in) :
#ifdef IS_FOR_PALM
   List(owner_in, WordFormWordsList),
#endif
   dict(0),
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
  protected:
    VoidHand history[NumHistoryItems];
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
	    if (!isdup)
	    {
	        (void)Set(pos, pat, MAX_PAT_LEN);
	        pos = (pos+1)%NumHistoryItems;
	    }
	}
    }
    virtual int NumItems()
    {
        return NumHistoryItems;
    }
    char *Get(UInt idx)
    {
	HMemCopy(rtn, history[idx], MAX_PAT_LEN);
	return rtn;
    }
    virtual char *GetItem(UInt idx)
    {
        UInt p = ((NumHistoryItems-1) + pos-idx)%NumHistoryItems;
	return Get(p);
    }
    unsigned char *Save(unsigned char *ptr) const;
    unsigned char *Restore(unsigned char *ptr);
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

unsigned char *HistoryList::Save(unsigned char *ptr) const
{
    for (UInt i = 0; i < NumHistoryItems; i++, ptr += MAX_PAT_LEN)
    {
	strncpy((char*)ptr, (const char *)Get(i), MAX_PAT_LEN-1);
	ptr[MAX_PAT_LEN-1] = 0;
    }
    *((UInt*)ptr)++ = pos;
    return ptr;
}

unsigned char *HistoryList::Restore(unsigned char *ptr)
{
    for (UInt i = 0; i < NumHistoryItems; i++, ptr += MAX_PAT_LEN)
    {
        Set(i, (const char *)ptr, MAX_PAT_LEN);
    }
    pos = *((UInt*)ptr)++;
    return ptr;
}

class MainForm: public Form
{
  protected:
    UInt minlength, maxlength, mincount, maxcount;
    UInt matchtype, multi;
    char pattern[MAX_PAT_LEN];
    HistoryList history;
    enum { LOWER, UPPER } letter_case;
    VariableSet *vars;

    virtual List *GetActiveList()
    {
        return &history;
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
    {
        delete vars;
    }
};

class VariableList : public List
{
  protected:
    VariableSet *vars;
    char rtn[40];
  public:
    VariableList(Form *owner_in, VariableSet *vars_in = 0)
	: List(owner_in, VariableFormVariableList),
	  vars(vars_in)
    { }
    void SetVariables(VariableSet *vars_in)
    {
        vars = vars_in;
    }
    virtual int NumItems()
    {
        return 26;
    }
    void ResetConstraints()
    {
        if (vars) vars->ResetConstraints();
    }
    void ClearAssignments()
    {
        if (vars) vars->ClearAssignments();
    }
    virtual char *GetItem(UInt idx)
    {
        if (vars) vars->Print(rtn, (int)idx);
	else rtn[0] = 0;
	return rtn;
    }
    virtual Boolean HandleSelect(UInt sel)
    {
        if (vars)
	{
	    char olda = vars->Assignment((int)sel);
	    if (olda) olda-='A'-1;
	    int x = (int)VarAssignDialog(owner, (UInt)olda).Run();
	    vars->Assign((int)sel, (char)((x ? (x+'A'-1) : 0)));
	}
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
    void SetVariables(VariableSet *vars)
    {
        list.SetVariables(vars);
    }
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
  
    WordList wordlist;
//    PalmWordDict dict;
    
    virtual void Init();
    virtual Boolean Open();
    virtual Boolean HandleSelect(UInt objID);
    virtual Boolean Update();
    
  public:
  
    WordForm()
        : Form(WordFormForm),
  //        dict(this),
          wordlist(this)
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
        return &wordlist;
    }
    int Start(CharPtr pat, UInt matchtype, UInt multi,
    	      	UInt minlength, UInt maxlength, 
	      	UInt mincount, UInt maxcount,
		VariableSet *vars)
    {
        return wordlist.Start(0 /*&dict*/,
        		pat, matchtype, multi, minlength, maxlength,
      			mincount, maxcount, vars);
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

int WordList::Start(PalmWordDict *dict_in,
			char *pat, UInt matchtype, UInt multi,
			UInt minlength, UInt maxlength, 
			UInt mincount, UInt maxcount,
			VariableSet *vars)
{
#if 0
    dict = dict_in;
#else
    (void)dict_in;
    // test dynamic allocation of dict
    if (dict == 0)
#ifdef IS_FOR_PALM
        dict = new PalmWordDict(owner);
#else
        dict = new PalmWordDict();
#endif
#endif
    if (dict == 0 ||
	dict->StartConsult(pat, (int)matchtype, (int)multi,
        				(int)minlength, (int)maxlength,
        				(int)mincount, (int)maxcount, 
					vars) != 0)
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
    if (!nomore && dict && (x = dict->NextMatch(line, MAX_PAT_LEN)) < 0)
    {
        strcpy(line, ltoa(dict->Matches()));
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
    PalmWordDict dict;
    WordList list(0);
    int typ = USEALL, multi = 0;
#ifdef VARVECT
    VariableSet vars;
#endif
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
	    PalmWordDict dict;
    	    list.Start(&dict, argv[i], typ, multi, 0, 0, 0, 0, &vars);
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

MainForm::MainForm()
    : Form(MainFormForm),
      minlength(0), maxlength(0), mincount(0), maxcount(0),
      matchtype(USEALL), multi(0),
      history(this),
      vars(0),
      letter_case(UPPER)
{
    pattern[0] = 0;
#ifdef VARVECT
    vars = new VariableSet();
#else
    vars = 0;
#endif
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
	    Save();
            WordForm *wordform = (WordForm*)::GetForm(WordFormForm);
            if (wordform)
            {
                if (wordform->Start(pattern, matchtype, multi, 
		    			minlength, maxlength,
                			mincount, maxcount, vars) == 0)
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
    (void)triggerID;
    (void)listID;
    const char *text = history.GetItem(selection);
    if (text) strcpy(pattern, text);
    SetControlLabel(MainFormHistoryPopTrigger, "History");
    DrawControl(MainFormHistoryPopTrigger);
    SetModalLabels();
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
	if (vars)
	    vars->ClearAssignments();
#endif
        return true;
    case EditResetConstraints:
#ifdef VARVECT
	if (vars)
            vars->ResetConstraints();
#endif
        return true;
    case EditShowConstraints:
#ifdef VARVECT
	f = (VariableForm*)GetForm(VariableFormForm);
	if (f) 
	{
	    f->SetVariables(vars);
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
		lp += MAX_PAT_LEN;
		lp = history.Restore(lp);
#ifdef VARVECT
		if (vars)
		    for (int i = 0; i < 26; i++)
	                vars->Assign(i, *((char*)lp)++);
#endif
	    }

	    MemHandleUnlock(h);
	    if (matchtype == ALL)
		minlength = maxlength = 0;
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
	    lp += sizeof(pattern);
	    lp = history.Save(lp);
#ifdef VARVECT
 	    if (vars)
		for (int i = 0; i < 26; i++)
	            *((char*)lp)++ = vars->Assignment(i);
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
}

Boolean WordForm::Open()
{
    if (Form::Open())
    {
        DrawControl(WordFormStopButton);
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
        if (!wordlist.NoMore())
        {
            wordlist.NextPage();
	    PostUpdateEvent();
	}
	return true;
    case WordFormRestartButton:
	wordlist.Reset();
	EnableNextButton();
	ClearProgress();
	PostUpdateEvent();
	return true;
    case WordFormVarsButton:
	f = (VariableForm*)GetForm(VariableFormForm);
	if (f) 
	{
	    wordlist.Reset();
	    EnableNextButton();
	    ClearProgress();
	    f->SetVariables(wordlist.GetVariables());
	    f->SetCaller(WordFormForm);
	    Switch(VariableFormForm);
	}
        return true;
    }
    return false;
}

Boolean WordForm::Update()
{
    wordlist.Erase();
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
    mainform.Restore();
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
    mainform.Save();
    Application::Stop();
}

DWord RunApplication(Word cmd, Ptr cmdPBP, Word launchflags)
{
    PalmWordApplication app;
    return app.Main(cmd, cmdPBP, launchflags);
}

#endif // IS_FOR_PALM
#endif // !UNIX

