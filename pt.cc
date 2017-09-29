// TODO: help
// Robustness with bad patterns, overruns
// Add a dict info dialog showing word length distributions 
//	and total number of words.

#define PERSIST
#define VARVECT

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

#define CFGSZ			(7*sizeof(UInt)+\
				(NumHistoryItems+1)*MAX_PAT_LEN+\
				 27*sizeof(unsigned long)+\
				 27*sizeof(char))

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
    return (pendown && x>130 && y > 143);
#endif
}

class WordList
#ifndef TEST
    : public List
#endif
{
  protected:
    PalmWordDict *dict;
    char line[30];
    int nomore;
    int page;
    char *assignments;

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
			unsigned long *varvector,
			char *assignments_in);
    unsigned long *GetVectorIn()
    {
        return dict->GetVectorIn();
    }
    unsigned long *GetVectorOut()
    {
        return dict->GetVectorOut();
    }
    char *GetAssignments()
    {
        return assignments;
    }
    virtual ~WordList()
    {
	delete dict;
    }
};

WordList::WordList(class Form *owner_in) :
#ifndef TEST
   List(owner_in, WordFormWordsList),
#endif
   dict(0),
   nomore(0),
   page(0),
   assignments(0)
{
    line[0] = 0;
}

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
    unsigned long *varvector;
    char *assignments;

    virtual List *GetList()
    {
        return &history;
    }
    void SavePattern();
    void SetModalLabels();
    void SetWordLengthModalControls();
    void SetWordCountModalControls();
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
    virtual ~MainForm()
    {
        delete [] varvector;
        delete [] assignments;
    }
};

class VariableList : public List
{
  protected:
    unsigned long *varvector_in;
    unsigned long *varvector_out;
    char *assignments;
    char rtn[40];
  public:
    VariableList(Form *owner_in)
	: List(owner_in, VariableFormVariableList),
	  varvector_in(0),
	  varvector_out(0),
	  assignments(0)
    { }
    void SetVectors(unsigned long *vector_in,
    			unsigned long *vector_out,
			char *assignments_in)
    {
        varvector_in = vector_in;
        varvector_out = vector_out;
	assignments = assignments_in;
    }
    virtual int NumItems()
    {
#if 1
        int rtn = 0;
	if (varvector_out)
	    for (int i = 0; i < 27; i++)
	        if (varvector_out[i]&0x3ffffffful)
		    ++rtn;
	return rtn;
#else
	return 27;
#endif
    }
    void ResetVector();
    void ClearAssignments()
    {
        if (assignments)
	    memset(assignments, 0, 27);
	ResetVector();
    }
    UInt Idx2Var(UInt idx)
    {
	if (varvector_out)
	    for (UInt i = 0; i < 27; i++)
	        if (varvector_out[i]&0x3ffffffful)
		    if (idx-- == 0)
		        return i;
	return 0;
    }
    virtual char *GetItem(UInt idx)
    {
        int pos = 0;
#ifdef VARVECT
        idx = Idx2Var(idx);
        rtn[0] = (char)('0'+idx/10);
        rtn[1] = (char)('0'+idx%10);
	if (idx>26 || varvector_out==0) { rtn[2]=0; return rtn; }
	if (assignments && assignments[idx])
	    rtn[2] = '!';
	else
            rtn[2] = ((varvector_out[idx]&0x80000000ul)==0) ? '>' : ' ';
	pos = 3;
	for (int c = 0; c < 26; c++)
	    if ((varvector_out[idx] & (1ul<<c)) != 0)
		rtn[pos++] = (char)(c+'A');
#endif
	rtn[pos] = 0;
	return rtn;
    }
    virtual Boolean HandleSelect(UInt selection)
    {
        UInt sel = Idx2Var(selection);
        if (assignments)
	{
	    assignments[sel] = (char)VarAssignDialog(owner, assignments[sel]).Run();
	    if (assignments[sel]==0)
		varvector_in[sel] = 
		varvector_out[sel] = 0xfffffffful;
	    else for (int i = 0; i < 27; i++)
	    {

	        if (i == sel)
		    varvector_out[i] = (1ul << (assignments[i]-1));
		else
		    varvector_out[i] &= ~(1ul << (assignments[i]-1));
		varvector_in[i] = varvector_out[i];
	    }
	}
	return List::HandleSelect(selection);
    }
    virtual ~VariableList()
    {
    }
};

void ResetVector(unsigned long *varvector, char *assignments)
{
#ifdef VARVECT
    if (varvector)
    {
	for (int i = 0; i < 27; i++)
	    varvector[i] = 0xfffffffful;
	if (assignments)
	{
	    for (int i = 0; i < 27; i++)
	    {
	        if (assignments && assignments[i]>0)
	        {
	            unsigned long mask = 1ul << (assignments[i]-1);
		    for (int j = 0; j < 27; j++)
		        varvector[j] &= ~mask;
		    varvector[i] = mask;
	        }
	    }
	}
    }
#endif
}

void VariableList::ResetVector()
{
    ::ResetVector(varvector_in, assignments);
}

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
	    list.ResetVector();
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
    void SetVectors(unsigned long *varvector_in,
    		    unsigned long *varvector_out,
		    char *assignments)
    {
        list.SetVectors(varvector_in, varvector_out, assignments);
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
  
    WordList list;
//    PalmWordDict dict;
    
    virtual void Init();
    virtual Boolean Open();
    virtual Boolean HandleSelect(UInt objID);
    virtual Boolean Update();
    
  public:
  
    WordForm()
        : Form(WordFormForm),
  //        dict(this),
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
    int Start(CharPtr pat, UInt matchtype, UInt multi,
    	      	UInt minlength, UInt maxlength, 
	      	UInt mincount, UInt maxcount,
		unsigned long *varvector,
		char *assignments)
    {
        return list.Start(0 /*&dict*/,
        		pat, matchtype, multi, minlength, maxlength,
      			mincount, maxcount, varvector,
			assignments);
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

#endif // TEST


//---------------------------------------------------------

int WordList::Start(PalmWordDict *dict_in,
			char *pat, UInt matchtype, UInt multi,
			UInt minlength, UInt maxlength, 
			UInt mincount, UInt maxcount,
			unsigned long *varvector,
			char *assignments_in)
{
    assignments = assignments_in;
#if 0
    dict = dict_in;
#else
    (void)dict_in;
    // test dynamic allocation of dict
    if (dict == 0)
#ifdef TEST
        dict = new PalmWordDict();
#else
        dict = new PalmWordDict(owner);
#endif
#endif
    if (dict == 0 ||
	dict->StartConsult(pat, (int)matchtype, (int)multi,
        				(int)minlength, (int)maxlength,
        				(int)mincount, (int)maxcount,
					varvector) != 0)
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
#ifndef TEST
        ((WordForm*)owner)->DisableNextButton();
#endif
    }
    return line;
}

//------------------------------------------------------------------------

#ifdef TEST

int debug = 0;

int main(int argc, char **argv)
{
    PalmWordDict dict;
    WordList list(0);
    int typ = USEALL, multi = 0;
#ifdef VARVECT
    unsigned long varvector[27];
    memset((char*)varvector, ((char)0xff), (unsigned)(27*sizeof(varvector[0])));
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
    	    list.Start(&dict, argv[i], typ, multi, 0, 0, 0, 0, varvector, 0);
	    break;
	}
    }
    int w = 0;
    while (!list.NoMore())
    {
        puts(list.GetItem((UInt)(w++)));
    }
    ShowVector(varvector);
}

#else

//------------------------------------------------------------------------

MainForm::MainForm()
    : Form(MainFormForm),
      minlength(0), maxlength(0), mincount(0), maxcount(0),
      matchtype(USEALL), multi(0),
      history(this),
      varvector(0),
      assignments(0),
      letter_case(UPPER)
{
    pattern[0] = 0;
#ifdef VARVECT
    varvector = new unsigned long[27];
    if (varvector)
        memset((char*)varvector, (char)0xff, 27*sizeof(long));
    assignments = new char[27];
    if (assignments)
        memset((char*)assignments, 0, 27);
#else
    varvector = 0;
    assignments = 0;
#endif
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
    Boolean caps, num, aut;
    Word temp;
    GrfGetState(&caps, &num, &temp, &aut);
    caps = (letter_case==LOWER)?False:True;
    GrfSetState(caps, num, 0);
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
                			mincount, maxcount, varvector,
					assignments) == 0)
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
	if (assignments)
	    memset((char*)assignments, 0, 27);
#endif
	// fall through
    case EditResetConstraints:
#ifdef VARVECT
	::ResetVector(varvector, assignments);
#endif
        return true;
    case EditShowConstraints:
#ifdef VARVECT
	f = (VariableForm*)GetForm(VariableFormForm);
	if (f) 
	{
	    f->SetVectors(varvector, varvector, assignments);
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
		if (varvector)
		    for (int i = 0; i < 27; i++)
	                varvector[i] = *((unsigned long*)lp)++;
		if (assignments)
		    for (int i = 0; i < 27; i++)
	                assignments[i] = *((char *)lp)++;
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
 	    if (varvector)
		for (int i = 0; i < 27; i++)
	            *((unsigned long*)lp)++ = varvector[i];
		if (assignments)
		    for (int i = 0; i < 27; i++)
	                *((char *)lp)++ = assignments[i];
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
	    f->SetVectors(list.GetVectorIn(), list.GetVectorOut(),
	    			list.GetAssignments());
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

#endif // TEST

