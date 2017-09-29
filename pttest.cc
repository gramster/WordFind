#ifdef __cplusplus
extern "C" {
#endif
#include <Pilot.h>
#ifdef __cplusplus
}
#include "ptrsc.h"
#endif

#include "fw.h"

class WordList : public List
{
  protected:
    char line[30];
    virtual char *GetItem(UInt itemNum);
    virtual int NumItems();
  public:
    WordList(Form *owner_in)
       : List(owner_in, WordFormWordsList)
    {
        line[0] = 0;
    }
    void Start(char *pat)
    {
        line[0] = 0;
        if (pat)
        {
            int l = 0;
            for (int l = 0; l < 29; l++)
                if (pat[l] == 0)
                    break;
                else
                    line[l] = pat[l];
            line[29]=0;
	    }
    }
};

char *WordList::GetItem(UInt itemNum)
{
    return line;
}

int WordList::NumItems()
{
    return 10;
}

class WordForm : public Form
{
  protected:
  
    WordList list;
    class MainForm &mf;
    
    virtual Boolean HandleControlSelect(UInt objID);
    virtual Boolean Open();
    virtual Boolean Update();
    
  public:
  
    WordForm(MainForm &mf_in)
        : Form(WordFormForm),
          list(this),
          mf(mf_in)
    { 
    }
    virtual List *GetList()
    {
        return &list;
    }
    void Start(CharPtr pat)
    {
        list.Start(pat);
    }
};

class MainForm : public Form
{
  protected:
    WordForm &wf;
    virtual Boolean HandleControlSelect(UInt objID);
    virtual Boolean Open()
    {
        Boolean rtn = Form::Open();
        FrmSetFocus(frm, FrmGetObjectIndex(frm, MainFormPatternField));
        return rtn;
    }
  public:
    MainForm(WordForm &wf_in)
        : Form(MainFormForm), wf(wf_in)
    {
    }
};

class PalmWordApplication : public Application
{
  protected:
    MainForm mainform;
    WordForm wordform;
    virtual Form *TopForm();
  public:
    PalmWordApplication()
      : Application(), mainform(wordform), wordform(mainform)
    {}
    virtual Form *GetForm(UInt formID);
};

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

Boolean MainForm::HandleControlSelect(UInt objID)
{
    switch (objID)
    {
    case MainFormGoButton:
        {
            FieldPtr fld = (FieldPtr)FrmGetObjectPtr(frm, 
	    			FrmGetObjectIndex(frm, MainFormPatternField));
	        if (fld)
	        {
	            CharPtr pat = FldGetTextPtr(fld);
                wf.Start(pat);
                wf.PostLoadEvent();
	            return true;
	        }
	    }
    }
    return false;
}

Boolean WordForm::HandleControlSelect(UInt objID)
{
    switch (objID)
    {
    case WordFormNewButton:
        {
        mf.PostLoadEvent();
	    return true;
	    }
    case WordFormNextButton:
        list.Init();
        PostUpdateEvent();
	    return true;
    }
    return false;
}

Boolean WordForm::Open()
{
    list.Init();
    return Form::Open();
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

