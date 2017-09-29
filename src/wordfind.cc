// TODO: help
// WORDFIND TODO:
//
// Need to change the characters used for vowels in PalmUInt16
// to ' and " so that they don't conflict with closures in
// the regular expressions, and then support them in our
// RegExp class so that regular expressions with vowel or
// consonant character ranges are more concise and we need
// less special handling of the vowel/consonant inserts (and
// must update the html manual and the on-line help to reflect
// this change).
// The WordForm class is essentially identical in both pl.cc 
// and pt.cc; this should be moved to a common file, together
// with any other code common to WordFindPro/++ and WordFindLite.
// Robustness with bad patterns, overruns

#define GLOBAL // just used to mark globals

#include "mypilot.h"
#ifdef IS_FOR_PALM
#include "fw.h"
#endif

#include "dict.h"
#include "history.h"

// derive the application

#define CLIENT_DB_TYPE		(0)

#define ConfigDBType		((UInt32)'pwcf')

#define NumWordListItems	(11)

#define MAX_PAT_LEN	(64)

typedef enum { LOWER, UPPER } LetterCase;

#if defined(USE_REGEXP) && defined(USE_GLOB)
#define NUM_HISTS	3
#else
#if defined(USE_REGEXP) || defined(USE_GLOB)
#define NUM_HISTS	2
#endif
#endif
#ifndef NUM_HISTS
#define NUM_HISTS	1
#endif
#ifdef VARVECT
#define NUM_VARSETS	1
#else
#define NUM_VARSETS	0
#endif

#define CFGSZ			(7*sizeof(UInt16)+\
				 NUM_HISTS*(sizeof(UInt16)+(NumHistoryItems+1)*MAX_PAT_LEN)+\
				 NUM_VARSETS*26*sizeof(char)+\
				 sizeof(UInt16))

// Globals - bad in C++ but we need to avoid stack/heap usage

#ifdef VARVECT
GLOBAL class VariableSet *vars;
#endif
GLOBAL class WordFindDict *dict;
static UInt16 minlength, maxlength, mincount, maxcount, matchtype, multi;


void DrawRectangle(int left, int top, int width, int height);
void ClearRectangle(int left, int top, int width, int height);

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

#if defined(WORDFINDPRO) || defined(WORDFINDPP)

class DatabaseList : public List
{
  protected:
    DatabaseListSource s;
    
    char *name_out;
    DictionaryDatabase *db;

    //void Reset();
  public:
    DatabaseList(Form *owner_in, UInt32 creator_in, UInt32 type_in,
    		char *name_out_in = 0, DictionaryDatabase *db_in = 0,
		UInt16 listid_in = 0);
    void SetDB(DictionaryDatabase *db_in)
    {
        db = db_in;
    }
    virtual void InstallDrawHandler() const = 0;
    virtual Boolean Activate();
    virtual Boolean HandleSelect(Int16 selection);
    Boolean Delete()
    {
        return s.Delete(select);
    }
    UInt16 Card() const
    {
        return s.Card(select);
    }
    LocalID DictID() const
    {
        return s.ID(select);
    }
    void GetName(Int16 selection, char *namebuf);
    virtual ~DatabaseList();
};

DatabaseList::DatabaseList(Form *owner_in, UInt32 creator_in, 
			   UInt32 type_in,
			   char *name_out_in,
			   DictionaryDatabase *db_in,
			   UInt16 listid_in)
  : List(owner_in, listid_in, &s),
    s(creator_in, type_in), 
    name_out(name_out_in),
    db(db_in)
{
    s.InitChoices();
}

Boolean DatabaseList::HandleSelect(Int16 selection)
{
    if (name_out) 
        strcpy(name_out, s.Name(selection));
    if (db)
    {
        if (db->IsOpen())
        {
            db->FreeRecords();
            db->Close();
        }
        db->OpenByID(s.ID(selection), s.Card(selection), dmModeReadOnly);
        if (db->IsOpen())
            db->InitRecords();
    }
    return List::HandleSelect(selection);
}

Boolean DatabaseList::Activate()
{
    return List::Activate();
}

DatabaseList::~DatabaseList()
{
}

#endif

#ifdef WORDFINDPRO

class DictionaryList : public DatabaseList
{
  public:
    DictionaryList(Form *owner_in, DictionaryDatabase *db_in, UInt16 listID_in)
    	: DatabaseList(owner_in, MY_CREATOR_ID, 'dict', 0,
			db_in, listID_in)
    {}
    virtual Boolean Activate()
    {
        s.InitChoices(); // refresh list
	return DatabaseList::Activate();
    }
    virtual UInt16 TriggerID() const = 0;
    virtual ~DictionaryList()
    {}
};

class MainDictionaryList : public DictionaryList
{
    LISTHANDLER(MainFormDictionaryList);
  public:
    MainDictionaryList(Form *owner_in, DictionaryDatabase *db_in)
      : DictionaryList(owner_in, db_in, MainFormDictionaryList)
    {}
    virtual UInt16 TriggerID() const
    {
        return MainFormDictionaryPopTrigger;
    }
};

#ifdef USE_REGEXP

class REDictionaryList : public DictionaryList
{
    LISTHANDLER(RegExpFormDictionaryList);
  public:
    REDictionaryList(Form *owner_in, DictionaryDatabase *db_in)
      : DictionaryList(owner_in, db_in, RegExpFormDictionaryList)
    {}
    virtual UInt16 TriggerID() const
    {
        return RegExpFormDictionaryPopTrigger;
    }
};

#endif USE_REGEXP

#ifdef USE_GLOB

class GlobDictionaryList : public DictionaryList
{
    LISTHANDLER(GlobFormDictionaryList);
  public:
    GlobDictionaryList(Form *owner_in, DictionaryDatabase *db_in)
      : DictionaryList(owner_in, db_in, GlobFormDictionaryList)
    {}
    virtual UInt16 TriggerID() const
    {
        return GlobFormDictionaryPopTrigger;
    }
};

#endif USE_GLOB

#endif

class DictFormDictList : public DatabaseList
{
  public:
    DictFormDictList(Form *owner_in)
    	: DatabaseList(owner_in, MY_CREATOR_ID, 'dict', 0, 0,
			 DictFormDictionariesList)
    {}
    LISTHANDLER(DictFormDictionariesList);
    virtual ~DictFormDictList()
    {}
};

//---------------------------------------------------------

// useful utility

Form *GetForm(UInt16 id)
{
    Application *a = Application::Instance();
    return a ? a->GetForm(id) : 0;
}

class WordListSource : public ListSource
{
  private:
    History history;
    int nomore;
    friend class WordList;
  public:
    inline WordListSource()
      : ListSource(), history(MAX_PAT_LEN), nomore(0)
    {}    
    const char *Start(char *pattern);
    virtual UInt16 NumItems();
    virtual Boolean GetItem(Int16 itemNum, Char* buf, UInt16 buflen);
    const char *GetItem(Int16 itemNum)
    {
        return history.Get((UInt16)itemNum);
    }
};

class WordList
#ifdef IS_FOR_PALM
    : public List
#endif
{
  protected:

    LISTHANDLER(WordFormWordsList)

    WordListSource s;
  public:
    WordList(Form *owner_in);
    int NoMore() const
    {
        return s.nomore;
    }
    void Reset()
    {
        s.nomore = 0;
        dict->Reset();
    }
    void NextPage()
    {
	dict->ClearProgress();
    }
    inline const char *Start(char *pattern)
    {
        return s.Start(pattern);
    }
    virtual Boolean HandleSelect(Int16 selection);
};

WordList::WordList(Form *owner_in) :
#ifdef IS_FOR_PALM
   List(owner_in, WordFormWordsList, &s),
#endif
   s()
{
}

Boolean WordList::HandleSelect(Int16 selection)
{
    const char *txt = s.GetItem(selection);
    ClipboardAddItem(clipboardText, txt, (UInt16)StrLen(txt));
    return True;
}

#ifdef IS_FOR_PALM

class RadioGroupDialog : public Dialog
{
    UInt16 firstcb;
    UInt16 numcbs;
    UInt16 val;
    Char* title;
    UInt16 GetValue();
  public:
    RadioGroupDialog(UInt16 id_in,
    			UInt16 firstcb_in,
    			UInt16 numcbs_in,
    			UInt16 val_in,
    			Char* title_in = 0)
        : Dialog(id_in),
          firstcb(firstcb_in),
          numcbs(numcbs_in),
          val(val_in),
          title(title_in) // must be static
    {}
    virtual Boolean HandleSelect(UInt16 objID);
    virtual Boolean Activate();
    virtual UInt16 Run();
    virtual ~RadioGroupDialog();
};

Boolean RadioGroupDialog::Activate()
{
    if (Dialog::Activate())
    {
        if (title) FrmSetTitle(frm, title);
        for (UInt16 i = 0; i < numcbs; i++)
            SetCheckBox(firstcb+i, (i==val));
        return True;
    }
    return False;
}

Boolean RadioGroupDialog::HandleSelect(UInt16 objID)
{
    if (objID >= firstcb && objID < (firstcb+numcbs))
    {
        for (UInt16 i = 0; i < numcbs; i++)
            SetCheckBox(firstcb+i, ((i==(objID-firstcb))?1u:0u));
    }
    return Dialog::HandleSelect(objID);
}

UInt16 RadioGroupDialog::Run()
{
    (void)Dialog::Run();
    return GetValue();
}

UInt16 RadioGroupDialog::GetValue()
{
    for (UInt16 i = 0; i < numcbs; i++)
    {
        if (IsCheckBoxSet(firstcb+i))
            return i;
    }
    return 0; // XXX should this be -1?
}

RadioGroupDialog::~RadioGroupDialog()
{
}

class MatchTypeDialog : public RadioGroupDialog
{
    UInt16 val;
  public:
    MatchTypeDialog(UInt16 val_in)
        : RadioGroupDialog(MatchTypeForm,
        	    	MatchTypeNormalCheckbox,
        	    	3,
        	    	val_in)
    {}
};

class WordLengthDialog : public RadioGroupDialog
{
  public:
    WordLengthDialog(UInt16 val_in, Char* title_in)
        : RadioGroupDialog(WordLengthForm,
        		   WordLengthNoLengthLimitCheckbox,
        		   16,
        		   val_in,
        		   title_in)
    {}
};

class WordCountDialog : public RadioGroupDialog
{
  public:
    WordCountDialog(UInt16 val_in, Char* title_in)
        : RadioGroupDialog(WordCountForm,
        	    	 WordCountNoCountLimitCheckbox,
        	    	 14,
        	    	 val_in,
        	    	 title_in)
    {}
};

#ifdef VARVECT
class VarAssignDialog : public RadioGroupDialog
{
  public:
    VarAssignDialog(UInt16 val_in)
        : RadioGroupDialog(VarAssignForm,
        		   VarAssignNoAssignmentCheckbox,
        		   27,
        		   val_in,
        		   "Variable Assignment")
    {}
};
#endif

class HistoryListSource : public ListSource
{
    History *history;
  public:
    HistoryListSource(History *history_in)
      : ListSource(),
        history(history_in)
    { }
    virtual UInt16 NumItems()
    {
        return history->MaxItems();
    }
    virtual Boolean GetItem(Int16 itemNum, Char *buf, UInt16 buflen)
    {
	StrNCopy(buf, history->GetItem(itemNum), (Int16)(buflen-1));
	buf[buflen-1] = 0;
	return True;
    }
};

// base class for history lists; must be subclassed as LISTHANDLER must be
// form-specific

class HistoryList : public List
{
    HistoryListSource s;
  public:
    HistoryList(Form *owner_in, UInt16 resid_in, History *history_in)
      : List(owner_in, resid_in, &s),
        s(history_in)
    {}
    Boolean GetItem(Int16 itemNum, Char* buf, UInt16 buflen)
    {
        return s.GetItem(itemNum, buf, buflen);
    }
};

class MainHistoryList : public HistoryList
{
    LISTHANDLER(MainFormHistoryList)
  public:
    MainHistoryList(Form *owner_in, History *history_in)
      : HistoryList(owner_in, MainFormHistoryList, history_in)
    {}
};

#ifdef USE_REGEXP
class REHistoryList : public HistoryList
{
    LISTHANDLER(RegExpFormHistoryList)
  public:
    REHistoryList(Form *owner_in, History *history_in)
      : HistoryList(owner_in, RegExpFormHistoryList, history_in)
    {}
};
#endif

#ifdef USE_GLOB
class GlobHistoryList : public HistoryList
{
    LISTHANDLER(GlobFormHistoryList)
  public:
    GlobHistoryList(Form *owner_in, History *history_in)
      : HistoryList(owner_in, GlobFormHistoryList, history_in)
    {}
};
#endif

class DictForm: public Form
{
  protected:
    DictFormDictList dictlist;
    virtual Boolean HandleSelect(UInt16 objID)
    {
        switch (objID)
        {
        case DictFormDeleteButton:
            if (dict->Card() == dictlist.Card() && dict->ID() == dictlist.DictID())
		{
		    dict->FreeRecords();
                dict->Close(True);
		}
	    dictlist.Delete();
	    // fall through
        case DictFormCancelButton:
	    Switch(MainFormForm);
	    return True;
	}
	return False;
    }
  public:
    DictForm()
        : Form(DictFormForm, 1),
          dictlist(this)
    { }
    virtual ~DictForm()
    { }
};

//--------------------------------------------------
// Base class for the main and regexp forms

class ConsultForm: public Form
{
  protected:
    History history;
    HistoryList *historylist;
    Field patfield;
#ifdef WORDFINDPRO
    DictionaryList *dictlist;
#endif
    char pattern[MAX_PAT_LEN];
    char recall;

    virtual void SetModalLabels();

    virtual Boolean HandleOpen();
    virtual void HandleClose();
    virtual Boolean HandleKeyDown(UInt16 chr, UInt16 keyCode, UInt16 &modifiers);
    virtual Boolean HandleSelect(UInt16 objID);
    virtual Boolean HandlePopupListSelect(UInt16 triggerID,
					UInt16 listID,
					Int16 selection);
    virtual Boolean HandleMenu(UInt16 menuID);
    void Go();

  public:
#ifdef WORDFINDPRO
    ConsultForm(UInt16 resid, UInt16 patID, HistoryList *hist,
    			DictionaryList *dlist);
#else
    ConsultForm(UInt16 resid, UInt16 patID, HistoryList *hist);
#endif
    virtual void PostHandle(EventType &event);
    inline void ClearPattern()
    {
        pattern[0] = 0;
    }
    void SavePattern();
    void Restore(unsigned char *&lp);
    void Save(unsigned char *&lp);
    virtual ~ConsultForm();
};

class MainForm: public ConsultForm
{
  protected:
    MainHistoryList hlist;
#ifdef WORDFINDPRO
    MainDictionaryList dlist;
#endif
    UInt16 matchtype;
    UInt16 minlength;
    UInt16 maxlength;
    UInt16 mincount;
    UInt16 maxcount;
    UInt16 multi;
    LetterCase letter_case;

    virtual void SetModalLabels();
    void SetWordLengthModalControls();
    void SetWordCountModalControls();
    void SetLetterMode();

    virtual Boolean HandleOpen();
    virtual Boolean HandleSelect(UInt16 objID);

  public:
    MainForm();
    void Restore(unsigned char *&lp);
    void Save(unsigned char *&lp);
    virtual void PostHandle(EventType &event);
    virtual ~MainForm();
};

#ifdef USE_REGEXP

class RegExpForm: public ConsultForm
{
  protected:
    REHistoryList hlist;
    REDictionaryList dlist;
    virtual Boolean HandleSelect(UInt16 objID);

  public:
    RegExpForm();
    virtual ~RegExpForm();
};

#endif USE_REGEXP

#ifdef USE_GLOB

class GlobForm : public ConsultForm
{
  protected:
    GlobHistoryList hlist;
#ifdef WORDFINDPRO
    GlobDictionaryList dlist;
#endif
    virtual Boolean HandleSelect(UInt16 objID);

  public:
    GlobForm();
    virtual ~GlobForm();
};
    
#endif USE_GLOB

#ifdef VARVECT

class VariableListSource : public ListSource
{
  public:
    inline VariableListSource()
	: ListSource()
    { }
    virtual UInt16 NumItems()
    {
        return 26;
    }
    virtual Boolean GetItem(Int16 itemNum, Char *buf, UInt16 buflen)
    {
        (void)buflen;
        // XXX we rely on Print producing a string shorter than buflen
        vars->Print(buf, itemNum);
        return True;
    }
};
   
class VariableList : public List
{
  protected:
    VariableListSource s;
    
    LISTHANDLER(VariableFormVariableList)
  public:
    VariableList(Form *owner_in)
	: List(owner_in, VariableFormVariableList, &s), s()
    { }
    void ResetConstraints()
    {
        vars->ResetConstraints();
    }
    void ClearAssignments()
    {
        vars->ClearAssignments();
    }
    virtual Boolean HandleSelect(Int16 sel)
    {
	char olda = vars->Assignment((int)sel);
	if (olda) olda-='A'-1;
	int x = (int)VarAssignDialog((UInt16)olda).Run();
	vars->Assign((int)sel, (char)((x ? (x+'A'-1) : 0)));
	Boolean rtn = List::HandleSelect(sel);
	Form *f = Application::Instance()->GetForm();
	if (f) f->Draw();
	return rtn;
    }
};

class VariableForm : public Form
{
  protected:
    VariableList list;
    UInt16 caller;
    virtual Boolean HandleSelect(UInt16 objID)
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
        : Form(VariableFormForm, 1),
          list(this),
	  caller(MainFormForm)
    {
    }
    void SetCaller(UInt16 caller_in)
    {
        caller = caller_in;
    }
};

#endif

class WordForm : public Form
{
  protected:
  
    WordList list;
    UInt16 caller;
    
    virtual Boolean Activate();
    virtual Boolean HandleOpen();
    virtual Boolean HandleKeyDown(UInt16 chr, UInt16 keyCode, UInt16 &modifiers);
    virtual Boolean HandleSelect(UInt16 objID);
    virtual Boolean HandleUpdate();
    
    void New();
    void NextPage();
    void Restart();
#ifdef VARVECT
    void ShowVars();
#endif

  public:
  
    WordForm()
        : Form(WordFormForm, 1),
          list(this)
    { 
    }
    void SetCaller(UInt16 caller_in)
    {
        caller = caller_in;
    }
    void EnableNextButton()
    {
        EnableControl(WordFormNextButton);
    }
    void DisableNextButton()
    {
        DisableControl(WordFormNextButton);
    }
    void ShowDebug(int sp, unsigned long n);
    const char *Start(char *pattern)
    {
        return list.Start(pattern);
    }
};

class WordFindApplication : public Application
{
  protected:
    MainForm mainform;
    WordForm wordform;
#ifdef USE_REGEXP
    RegExpForm REform;
#endif
#ifdef USE_GLOB
    GlobForm globform;
#endif
    UInt16 consultformID;
#ifdef VARVECT
    VariableForm variableform;
#endif
    DictForm dictform;
    virtual void Stop();
  public:
    WordFindApplication();
    void SaveSettings();
    void RestoreSettings();
    void SetConsultFormID(UInt16 id_in)
    {
        consultformID = id_in;
    }
    virtual ~WordFindApplication()
    {
    }
};

#endif // IS_FOR_PALM

//---------------------------------------------------------

const char *WordListSource::Start(char *pattern)
{
    const char *x = dict->StartConsult(pattern, (int)matchtype, (int)multi,
        				(int)minlength, (int)maxlength,
        				(int)mincount, (int)maxcount,
#ifdef VARVECT
					vars);
#else
					0);
#endif
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
    if (!nomore)
    {
        x = dict->NextMatch(buf, (int)buflen);
        if (x < 0)
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
        else history.Set((UInt16)itemNum, buf);
    }
    return nomore ? False : True;
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
    	    list.Start(argv[i], typ, multi, 0, 0, 0, 0, vars);
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

#ifdef WORDFINDPRO
ConsultForm::ConsultForm(UInt16 resID_in, UInt16 patID_in,
			 HistoryList *hlist, DictionaryList *dlist)
    : Form(resID_in, 3),
      history(MAX_PAT_LEN),
      historylist(hlist),
      patfield(this, patID_in),
      dictlist(dlist),
      recall(0)
#else
ConsultForm::ConsultForm(UInt16 resID_in, UInt16 patID_in,
			 HistoryList *hlist)
    : Form(resID_in, 3),
      history(MAX_PAT_LEN),
      historylist(hlist),
      patfield(this, patID_in),
      recall(0)
#endif
{
    pattern[0] = 0;
}

void ConsultForm::SetModalLabels()
{
    patfield.Set(pattern);
#ifdef WORDFINDPRO
    if (dict->Name()[0])
        SetControlLabel(dictlist->TriggerID(), dict->Name());
    else
        SetControlLabel(dictlist->TriggerID(), "No Dictionary");
#endif
}

Boolean ConsultForm::HandleOpen()
{
    SetModalLabels();
    Form::HandleOpen();
    SetFocus(patfield.ID());
    ((WordFindApplication*)Application::Instance())->SetConsultFormID(id);
    return True;
}

void ConsultForm::SavePattern()
{
    patfield.Read(pattern, MAX_PAT_LEN);
}

void ConsultForm::HandleClose()
{
    SavePattern();
    Form::HandleClose();
}

Boolean ConsultForm::HandleKeyDown(UInt16 chr, UInt16 keyCode, UInt16 &modifiers)
{
    (void)keyCode;
    (void)modifiers;
    if (chr == '~')
    {
        recall = 1;
	return True;
    }
    else if (chr >= '0' && chr <= '9' && recall)
    {
        recall = 0;
	historylist->GetItem((Int16)(chr-'0'), pattern, MAX_PAT_LEN);
	SetModalLabels();
	return True;
    }
    else recall = 0;
    if (chr =='\n')
    {
        Go();
	return True;
    }
    return False;
}

Boolean ConsultForm::HandleSelect(UInt16 objID)
{
    return Form::HandleSelect(objID);
}

Boolean ConsultForm::HandlePopupListSelect(UInt16 triggerID,
					UInt16 listID,
					Int16 selection)
{
    (void)listID;
    if (listID == historylist->ID())
    {
        historylist->GetItem(selection, pattern, MAX_PAT_LEN);
        SetControlLabel(triggerID, "History");
        DrawControl(triggerID);
    }
#ifdef WORDFINDPRO
    else if (listID == dictlist->ID())
    {
        dictlist->HandleSelect(selection);
    }
#endif
    SetModalLabels();
    return True; 
}

Boolean ConsultForm::HandleMenu(UInt16 menuID)
{
    switch(menuID)
    {
#ifdef HelpAboutWordFind
    case HelpAboutWordFind:
#endif
#ifdef HelpAboutWordFindPro
    case HelpAboutWordFindPro:
#endif
        FrmAlert(AboutBoxAlert);
        return true;
#ifdef VARVECT
    case EditClearAssignments:
	vars->ClearAssignments();
        return true;
    case EditResetConstraints:
        vars->ResetConstraints();
        return true;
    case EditShowConstraints:
      {
	VariableForm *f = (VariableForm*)GetForm(VariableFormForm);
	if (f) 
	{
	    f->SetCaller(MainFormForm);
	    Switch(VariableFormForm);
	}
        return True;
      }
#endif
    case EditDeleteDictionary:
	Switch(DictFormForm);
	return True;
    default:
        return Form::HandleMenu(menuID);
    }
    return false;
}

void ConsultForm::PostHandle(EventType &event)
{
    (void)event;
}

void ConsultForm::Go()
{
    SavePattern();
    if (pattern[0])
    {
        history.Add(pattern);
        ((WordFindApplication*)Application::Instance())->SaveSettings();
        WordForm *wordform = (WordForm*)::GetForm(WordFormForm);
        if (wordform)
        {
            const char *x = wordform->Start(pattern);
            if (x == 0)
            {
                wordform->SetCaller(ID());
    	        Switch(WordFormForm);
    	    }
    	    else FrmCustomAlert(ErrorAlert, x, "", "");
        }
    }
}

void ConsultForm::Save(unsigned char *&lp)
{
    strncpy((char *)lp, (const char *)pattern, MAX_PAT_LEN);
    lp += MAX_PAT_LEN;
    lp = history.Save(lp);
}

void ConsultForm::Restore(unsigned char *&lp)
{
    strncpy(pattern, (const char *)lp, MAX_PAT_LEN);
    pattern[MAX_PAT_LEN-1] = 0;
    lp += MAX_PAT_LEN;
    lp = history.Restore(lp);
}

ConsultForm::~ConsultForm()
{ 
}

//---------------------------------------------------------------------------
// XXX arguably, we should be using prefSetAppPreferences and
// prefGetAppPreferences for this, rather than a separate 
// config database.

MainForm::MainForm()
#ifdef WORDFINDPRO
    : ConsultForm(MainFormForm, MainFormPatternField, &hlist, &dlist),
      hlist(this, &history),
      dlist(this, dict)
#else
    : ConsultForm(MainFormForm, MainFormPatternField, &hlist),
      hlist(this, &history)
#endif
{
    minlength = maxlength = mincount = maxcount = 0;
    matchtype = USEALL;
    multi = 0;
    letter_case = UPPER;
}

void MainForm::Save(unsigned char *&lp)
{
    *((UInt16*)lp)++ = minlength;
    *((UInt16*)lp)++ = maxlength;
    *((UInt16*)lp)++ = mincount;
    *((UInt16*)lp)++ = maxcount;
    *((UInt16*)lp)++ = matchtype;
    *((UInt16*)lp)++ = multi;
    // must maintain even addresses, so cast to UInt16 below...
    *((UInt16*)lp)++ = (UInt16)letter_case;
    ConsultForm::Save(lp);
#ifdef VARVECT
    if (vars)
        for (int i = 0; i < 26; i++)
            *((char*)lp)++ = vars->Assignment(i);
#endif
}

void MainForm::Restore(unsigned char *&lp)
{
    minlength = *((UInt16*)lp)++;
    maxlength = *((UInt16*)lp)++;
    mincount = *((UInt16*)lp)++;
    maxcount = *((UInt16*)lp)++;
    matchtype = *((UInt16*)lp)++;
    multi = *((UInt16*)lp)++;
    letter_case = (LetterCase)(*((UInt16*)lp)++);
    ConsultForm::Restore(lp);
#ifdef VARVECT
    if (vars)
        for (int i = 0; i < 26; i++)
            vars->Assign(i, *((char*)lp)++);
#endif
    if (matchtype == ALL)
	minlength = maxlength = 0;
}

void MainForm::SetLetterMode()
{
    Application::SetLetterCapsMode((letter_case==LOWER)?False:True);
}

void MainForm::SetWordLengthModalControls()
{
    int cando = (multi || (matchtype != USEALL));
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
    ConsultForm::SetModalLabels();
    SetCheckBox(MainFormMultiCheckbox, multi);
    SetControlLabel(MainFormMatchTypeSelTrigger, DictionaryDatabase::MatchTypeName(matchtype));
    SetWordCountModalControls();
    SetWordLengthModalControls();
    SetControlLabel(MainFormCaseButton, (letter_case==LOWER?"Floating":"Anchored"));
    SetLetterMode();
}

Boolean MainForm::HandleOpen()
{
    Boolean rtn = ConsultForm::HandleOpen();
    SetLetterMode();
    return rtn;
}

void MainForm::PostHandle(EventType &event)
{
    if (event.eType == keyDownEvent)
        SetLetterMode();    
}

Boolean MainForm::HandleSelect(UInt16 objID)
{
    switch (objID)
    {
#if defined(USE_REGEXP) || defined(USE_GLOB)
    case MainFormPatPromptButton:
#ifdef USE_REGEXP
        Switch(RegExpFormForm);
#else
	Switch(GlobFormForm);
#endif
        break;
#endif
    case MainFormMatchTypeSelTrigger:
    	matchtype = MatchTypeDialog(matchtype).Run();
	SetControlLabel(MainFormMatchTypeSelTrigger, DictionaryDatabase::MatchTypeName(matchtype));
	SetWordLengthModalControls();
	break;
    case MainFormMinLengthSelTrigger:
        //if (matchtype == USEALL) break;
	minlength = WordLengthDialog(minlength, "Min Word Length").Run();
	SetControlLabel(MainFormMinLengthSelTrigger, DictionaryDatabase::LimitName(minlength));
	break;
    case MainFormMaxLengthSelTrigger:
        //if (matchtype == USEALL) break;
	maxlength = WordLengthDialog(maxlength, "Max Word Length").Run();
	SetControlLabel(MainFormMaxLengthSelTrigger, DictionaryDatabase::LimitName(maxlength));
	break;
    case MainFormMinCountSelTrigger:
	mincount = WordCountDialog(mincount, "Min Word Count").Run();
    	SetControlLabel(MainFormMinCountSelTrigger, DictionaryDatabase::LimitName(mincount));
    	break;
    case MainFormMaxCountSelTrigger:
	maxcount = WordCountDialog(maxcount, "Max Word Count").Run();
        SetControlLabel(MainFormMaxCountSelTrigger, DictionaryDatabase::LimitName(maxcount));
	break;
    case MainFormCaseButton: // toggle case and labels
	letter_case = (letter_case==LOWER) ? UPPER : LOWER;
	SavePattern();
	SetModalLabels();
	break;
    case MainFormClearButton:
        patfield.Clear();
	pattern[0] = 0;
        break;
    case MainFormBreakButton:
        patfield.Insert('|');
        break;
    case MainFormSpaceButton:
        patfield.Insert(' ');
        break;
    case MainFormSlashButton:
        patfield.Insert('/');
        break;
    case MainFormWildButton:
        patfield.Insert((letter_case == LOWER) ? '.' : ':');
        break;
    case MainFormVowelButton:
        patfield.Insert((letter_case == LOWER) ? '+' : '*');
        break;
    case MainFormConsonantButton:
        patfield.Insert((letter_case == LOWER) ? '-' : '=');
        break;
    case MainFormLeftBracketButton:
        patfield.Insert('[');
        break;
    case MainFormRightBracketButton:
        patfield.Insert(']');
        break;
    case MainFormRangeButton:
        patfield.Insert('-');
        break;
    case MainFormNegateButton:
        patfield.Insert('^');
        break;
    case MainFormMultiCheckbox:
        multi = !multi;
	SetWordCountModalControls();
	SetWordLengthModalControls();
	break;
    case MainFormGoButton:
        ::minlength = minlength;
        ::maxlength = maxlength;
        ::mincount = mincount;
        ::maxcount = maxcount;
        ::matchtype = matchtype;
        ::multi = multi;
        Go();
	break;
    default:
	return ConsultForm::HandleSelect(objID);
    }
    return True;
}

MainForm::~MainForm()
{ 
}

//----------------------------------------------------------------

#ifdef USE_REGEXP

RegExpForm::RegExpForm()
#ifdef WORDFINDPRO
    : ConsultForm(RegExpFormForm, RegExpFormPatternField, &hlist, &dlist),
      hlist(this, &history),
      dlist(this, dict)
#else
    : ConsultForm(RegExpFormForm, RegExpFormPatternField, &hlist),
      hlist(this, history)
#endif
{
}

Boolean RegExpForm::HandleSelect(UInt16 objID)
{
    switch (objID)
    {
    case RegExpFormREPromptButton:
#ifdef USE_GLOB
	Switch(GlobFormForm);
#else
        Switch(MainFormForm);
#endif
        break;
    case RegExpFormClearButton:
        patfield.Clear();
	pattern[0] = 0;
        break;
    case RegExpFormWildButton:
        patfield.Insert('.');
        break;
    case RegExpFormVowelButton:
        patfield.Insert("[aeiou]");
        break;
    case RegExpFormConsonantButton:
        patfield.Insert("[^aeiou]");
        break;
    case RegExpFormLeftBracketButton:
        patfield.Insert('[');
        break;
    case RegExpFormRightBracketButton:
        patfield.Insert(']');
        break;
    case RegExpFormRangeButton:
        patfield.Insert('-');
        break;
    case RegExpFormNegateButton:
        patfield.Insert('^');
        break;
    case RegExpFormZeroOrMoreButton:
        patfield.Insert('*');
        break;
    case RegExpFormOneOrMoreButton:
        patfield.Insert('+');
        break;
    case RegExpFormZeroOrOneButton:
        patfield.Insert('?');
        break;
    case RegExpFormWordEndButton:
        patfield.Insert('$');
        break;
    case RegExpFormGoButton:
        minlength = maxlength = mincount = maxcount = multi = 0;
        matchtype = REGEXP;
        Go();
	break;
    default:
	return ConsultForm::HandleSelect(objID);
    }
    return True;
}

RegExpForm::~RegExpForm()
{ 
}

#endif USE_REGEXP

#ifdef USE_GLOB

GlobForm::GlobForm()
#ifdef WORDFINDPRO
    : ConsultForm(GlobFormForm, GlobFormPatternField, &hlist, &dlist),
      hlist(this, &history),
      dlist(this, dict)
#else
    : ConsultForm(GlobFormForm, GlobFormPatternField, &hlist),
      hlist(this, &history)
#endif
{
}

Boolean GlobForm::HandleSelect(UInt16 objID)
{
    switch (objID)
    {
    case GlobFormGlobPromptButton:
        Switch(MainFormForm);
        break;
    case GlobFormClearButton:
        patfield.Clear();
	pattern[0] = 0;
        break;
    case GlobFormVowelButton:
        patfield.Insert("[aeiou]");
        break;
    case GlobFormConsonantButton:
        patfield.Insert("[^aeiou]");
        break;
    case GlobFormLeftBracketButton:
        patfield.Insert('[');
        break;
    case GlobFormRightBracketButton:
        patfield.Insert(']');
        break;
    case GlobFormRangeButton:
        patfield.Insert('-');
        break;
    case GlobFormNegateButton:
        patfield.Insert('^');
        break;
    case GlobFormWildButton:
        patfield.Insert('*');
        break;
    case GlobFormAnyButton:
        patfield.Insert('?');
        break;
    case GlobFormGoButton:
        minlength = maxlength = mincount = maxcount = multi = 0;
        matchtype = GLOB;
        Go();
	break;
    default:
	return ConsultForm::HandleSelect(objID);
    }
    return True;
}

GlobForm::~GlobForm()
{ 
}

#endif USE_GLOB

//------------------------------------------------------------------------

Boolean WordForm::Activate()
{
    if (Form::Activate())
    {
        EnableNextButton();
        return True;
    }
    return False;
}

Boolean WordForm::HandleOpen()
{
    if (Form::HandleOpen())
    {
        DrawControl(WordFormStopButton);
        ClearProgress();
	return True;
    }
    return False;
}


#if 0
void WordForm::ShowDebug(int sp, unsigned long n)
{
    static char l[40];
    strcpy(l, "sp ");
    StrIToA(l+3, sp);
    strcat(l, "  n ");
    StrIToA(l+strlen(l), (long)n);
    FieldPtr fld = (FieldPtr)GetObject(WordFormDebugField);
    if (fld)
    {
        FldSetTextPtr(fld, l);
        FldSetDirty(fld, 1);
        FldDrawField(fld);
    }
}
#endif

void WordForm::New()
{
    ConsultForm *f = (ConsultForm*)GetForm(caller);
    if (f) f->ClearPattern();
    Switch(caller);
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
#if 0
    list.Reset();
    EnableNextButton();
    ClearProgress();
    PostUpdateEvent();
#else
    ConsultForm *f = (ConsultForm*)GetForm(caller);
    Switch(caller);
#endif
}

#ifdef VARVECT
void WordForm::ShowVars()
{
    VariableForm *f = (VariableForm*)GetForm(VariableFormForm);
    if (f) 
    {
        list.Reset();
        EnableNextButton();
        ClearProgress();
        f->SetCaller(WordFormForm);
        Switch(VariableFormForm);
    }
}
#endif

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
#ifdef VARVECT
    case 'v':
    case 'V':
        ShowVars();
	break;
#endif
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
#ifdef VARVECT
    case WordFormVarsButton:
        ShowVars();
        return true;
#endif
    }
    return false;
}

Boolean WordForm::HandleUpdate()
{
    list.Erase();
    return Form::HandleUpdate();
}

//------------------------------------------------------------------------

#ifdef VARVECT
#define VARFORMS	1
#else
#define VARFORMS	0
#endif
#ifdef USE_REGEXP
#define REFORMS		1
#else
#define REFORMS		0
#endif
#ifdef USE_GLOB
#define GLOBFORMS	1
#else
#define GLOBFORMS	0
#endif

WordFindApplication::WordFindApplication()
  : Application(3+VARFORMS+REFORMS+GLOBFORMS, MainFormForm),
    mainform(),
    wordform(),
#ifdef USE_REGEXP
    REform(),
#endif
#ifdef USE_GLOB
    globform(),
#endif
    consultformID(MainFormForm),
#ifdef VARVECT
    variableform(),
#endif
    dictform()
{
#ifdef VARVECT
    vars = new VariableSet();
//    MemSetDebugMode(memDebugModeCheckOnAll);
#endif
    RestoreSettings();
}

void WordFindApplication::SaveSettings()
{
    Database config;
    MemHandle h = 0;
    int x = CFGSZ;
    x = x*4;
    x = x/2;
    if (config.OpenByType(ConfigDBType, dmModeReadWrite, "WordFindCfg") == True)
	h = config.GetRecord(0);
    if (h == 0)
	h = config.NewRecord(0, (UInt16)x/2/*CFGSZ*/);
    else if (MemHandleSize(h) != CFGSZ)
        h = config.Resize(0, CFGSZ);
    if (h)
    {
	unsigned char *lpbuf = (unsigned char *)MemPtrNew(CFGSZ);
	unsigned char *dp = (unsigned char*)MemHandleLock(h);
	if (lpbuf && dp)
	{
	    unsigned char *lp = lpbuf;
	    mainform.Save(lp);
#ifdef USE_REGEXP
	    REform.Save(lp);
#endif
#ifdef USE_GLOB
	    globform.Save(lp);
#endif
	    *((UInt16*)lp)++ = consultformID;
	    // still to do - WordForm stack context
	    config.Write(dp, lpbuf, CFGSZ); 
	}
        MemHandleUnlock(h);
        if (lpbuf) MemPtrFree(lpbuf);
	config.ReleaseRecord(0);
	config.Close();
    }
}

void WordFindApplication::RestoreSettings()
{
    Database config;
    if (config.OpenByType(ConfigDBType) == True)
    {
        MemHandle h = config.QueryRecord(0);
        if (h)
        {
	    unsigned char *lp = (unsigned char*)MemHandleLock(h);
	    if (lp && MemHandleSize(h) >= CFGSZ)
	    {
	        mainform.Restore(lp);
#ifdef USE_REGEXP
		REform.Restore(lp);
#endif
#ifdef USE_GLOB
		globform.Restore(lp);
#endif
		consultformID = startformID = *((UInt16*)lp)++;
	    }
	    MemHandleUnlock(h);
	}
	config.Close();
    }
}

void WordFindApplication::Stop()
{
    if (activeform == &mainform)
        mainform.SavePattern();
#ifdef USE_REGEXP
    else if (activeform == &REform)
        REform.SavePattern();
#endif
#ifdef USE_GLOB
    else if (activeform == &globform)
        globform.SavePattern();
#endif    
    SaveSettings();
#ifdef VARVECT
    delete vars;
    vars = 0;
#endif
    Application::Stop();
}

//------------------------------------------------------------------------

UInt32 RunApplication(MemPtr cmdPBP, UInt16 launchflags)
{
    UInt32 rtn = 0;
    // dict must be created before the application is, so that it
    // can be passed to the dictlist in main form
#ifdef VARVECT
    vars = 0;
#endif
    dict = new WordFindDict();
    if (dict)
    {
        dict->InitDB();
        WordFindApplication *app = new WordFindApplication();
	rtn = app->Main(cmdPBP, launchflags);
	delete app;
	delete dict;
	dict = 0;
    }
    return rtn;
}

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


#endif // IS_FOR_PALM
#endif // !UNIX

