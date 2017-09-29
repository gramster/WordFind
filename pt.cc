// TODO: help
// Robustness with bad patterns, overruns
// Add a dict info dialog showing word length distributions 
//	and total number of words.

#include "mypilot.h"
#ifndef TEST
#include "ptrsc.h"
#include "fw.h"
#endif

#include "dict.h"

// derive the application

#define CLIENT_DB_TYPE		(0)

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
#ifndef TEST
    class WordForm *owner;
#endif
    PalmWordDict &dict;
    char line[30];
    int nomore;
    int page;
    virtual int NumItems();

  public:
    virtual char *GetItem(UInt itemNum);
#ifdef TEST
    WordList(PalmWordDict &dict_in) :
#else
    WordList(class WordForm *owner_in, PalmWordDict &dict_in)
       : List(WordFormWordsList),
         owner(owner_in),
#endif
         dict(dict_in),
         nomore(0),
         page(0)
    {}
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

class HistoryList : public List
{
  protected:
#define NumHistoryItems		(10)
    char history[NumHistoryItems][64];
    int pos;
  public:
    HistoryList();
    void Add(char *pat)
    {
        if (strcmp(history[(pos+NumHistoryItems-1)%NumHistoryItems], pat))
	{
	    strcpy(history[pos], pat);
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
        return history[p];
    }
};

HistoryList::HistoryList() : List(MainFormHistoryList)
{
    for (int i = 0; i < NumHistoryItems; i++)
	strcpy(history[i], "none");
    pos = 0;
}

class MainForm: public Form
{
  protected:
    UInt minlength, maxlength, mincount, maxcount;
    UInt matchtype, multi;
    char pattern[128];
    HistoryList history;
    enum { LOWER, UPPER } letter_case;
    void SetModalLabels();
    virtual List *GetList()
    {
        return &history;
    }
    virtual Boolean Open();
    virtual Boolean HandleSelect(UInt objID);
    virtual Boolean HandlePopupListSelect(UInt triggerID,
					UInt listID,
					UInt selection);
    virtual Boolean HandleMenu(UInt menuID);

  public:
    MainForm();
};

class WordForm : public Form
{
  protected:
  
    PalmWordDict dict;
    WordList list;
    
    virtual Boolean HandleSelect(UInt objID);
    virtual Boolean Open();
    virtual Boolean Update();
    virtual void Init();
    
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
    return 10;
}

char *WordList::GetItem(UInt itemNum)
{
    int x;
    (void)itemNum;
    line[0] = 0;
    if (!nomore && (x = dict.NextMatch(line)) < 0)
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
    WordList list(dict);
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
{}

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
      matchtype(USEALL), multi(0), history(), letter_case(UPPER)
{
    pattern[0] = 0;
}

void MainForm::SetModalLabels()
{
    SetControlLabel(MainFormWildButton, (letter_case==LOWER?".":":"));
    SetControlLabel(MainFormCaseButton, (letter_case==LOWER?"Lower":"Upper"));
    SetControlLabel(MainFormVowelButton, (letter_case==LOWER?"+":"*"));
    SetControlLabel(MainFormConsonantButton, (letter_case==LOWER?"-":"="));
    FrmSetFocus(frm, FrmGetObjectIndex(frm, MainFormPatternField));
    Boolean caps, num, aut;
    Word temp;
    GrfGetState(&caps, &num, &temp, &aut);
    caps = (letter_case==LOWER)?False:True;
    GrfSetState(caps, num, 0);
}

Boolean MainForm::Open()
{
    Boolean rtn = Form::Open();
    SetCheckBox(MainFormMultiCheckbox, multi);
    pattern[0]=0;
    SetField(MainFormPatternField, pattern);
    SetControlLabel(MainFormMatchTypeSelTrigger, DictionaryDatabase::MatchTypeName(matchtype));
    SetControlLabel(MainFormMinLengthSelTrigger, DictionaryDatabase::LimitName(minlength));
    SetControlLabel(MainFormMaxLengthSelTrigger, DictionaryDatabase::LimitName(maxlength));
    SetControlLabel(MainFormMinCountSelTrigger, DictionaryDatabase::LimitName(mincount));
    SetControlLabel(MainFormMaxCountSelTrigger, DictionaryDatabase::LimitName(maxcount));
//    SetModalLabels();
    history.Init();
    SetModalLabels();
    return rtn;
}

Boolean MainForm::HandleSelect(UInt objID)
{
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
    case MainFormGoButton:
      {
	    // Get params
	    multi = IsCheckBoxSet(MainFormMultiCheckbox);
	    CharPtr pat = ReadField(MainFormPatternField);
	    if (pat && pat[0])
	    {
	        history.Add(pat);
	        Application *a = Application::Instance();
            WordForm *wordform = a ? (WordForm*)a->GetForm(WordFormForm) : 0;
            if (wordform)
            {
                if (wordform->Start(pat, matchtype, multi, minlength, maxlength,
                			mincount, maxcount) == 0)
					wordform->PostLoadEvent();
		    	else
					FrmAlert(NoDictBoxAlert); 
            }
        }
        break;
      }
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
    SetField(MainFormPatternField, pattern);
    SetControlLabel(MainFormHistoryPopTrigger, "History");
    DrawControl(MainFormHistoryPopTrigger);
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
    	case EditUndo:	FldUndo(fld);	break;
    	case EditCut:	FldCut(fld);	break;
    	case EditCopy:	FldCopy(fld);	break;
    	case EditPaste:	FldPaste(fld);	break;
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
        {
	    Switch(MainFormForm);
	    return true;
	    }
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

Boolean WordForm::Open()
{
    ControlPtr cm = (ControlPtr)GetObject(WordFormStopButton);
    if (cm) CtlDrawControl(cm);
    list.Init();
    Boolean rtn = Form::Open();
    ClearProgress();
    return rtn;
}

void WordForm::ClearProgress()
{
    ClearRectangle(153, 15, 5, 120);
}

Boolean WordForm::Update()
{
    list.Erase();
    return false;
}

Form *PalmWordApplication::TopForm()
{
    return &mainform;
}

DWord RunApplication(Word cmd, Ptr cmdPBP, Word launchflags)
{
 	PalmWordApplication app;
	return app.Main(cmd, cmdPBP, launchflags);
}

#endif // TEST


