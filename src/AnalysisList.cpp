#include "AnalysisList.h"
#include "CpptrajStdio.h"
#include <cstdlib>
// All analysis classes go here
#include "Analysis_Hist.h"

// CONSTRUCTOR
AnalysisList::AnalysisList() {
  analysisList=NULL;
  Nanalysis=0;
  debug=0;
}

// DESTRUCTOR
AnalysisList::~AnalysisList() {
  if (analysisList!=NULL) {
    for (int i=0; i<Nanalysis; i++)
      delete analysisList[i];
    free(analysisList);
  }
}

/* AnalysisList::SetDebug()
 * Set Analysis list debug level.
 */
void AnalysisList::SetDebug(int debugIn) {
  debug=debugIn;
  if (debug>0)
    mprintf("AnalysisList DEBUG LEVEL SET TO %i\n",debug);
}

/* AnalysisList::Add()
 * Add specific type of analysis to the list.
 */
int AnalysisList::Add(ArgList *argIn) {
  Analysis *Ana=NULL;
 
  if     (argIn->CommandIs("histogram",4)) { Ana = new Hist(); }
  else return 1;

  // Pass in the argument list
  Ana->SetArg(argIn);

  // Set the debug level
  if (debug>0) Ana->SetDebug(debug);

  // Store in analysis list
  analysisList = (Analysis**) realloc(analysisList, (Nanalysis+1) * sizeof(Analysis*));
  analysisList[Nanalysis] = Ana;
  Nanalysis++;

  return 0;
}

/* AnalysisList::Setup()
 * Set up all analysis in list with given datasetlist
 */
int AnalysisList::Setup(DataSetList *datasetlist) {
  if (Nanalysis==0) return 0;
  mprintf("\nANALYSIS:\n");
  mprintf("    .... Setting up %i analyses ....\n",Nanalysis);
  for (int ana=0; ana < Nanalysis; ana++) {
    mprintf("    %i: [%s]\n",ana,analysisList[ana]->CmdLine());
    analysisList[ana]->noSetup=false;
    if (analysisList[ana]->Setup(datasetlist)) {
      mprintf("    Error setting up analysis %i [%s] - skipping.\n",ana,analysisList[ana]->Name());
      analysisList[ana]->noSetup=true;
    }
  }
     
  mprintf("    ...................................................\n\n");
  return 0;
}

/* AnalysisList::Analyze()
 */
void AnalysisList::Analyze(DataFileList *datafilelist) {
  if (Nanalysis==0) return;
  mprintf("\nANALYSIS:\n");
  mprintf("    .... Performing %i analyses ....\n",Nanalysis);
  for (int ana=0; ana < Nanalysis; ana++) {
    if (!analysisList[ana]->noSetup) {
      mprintf("    %i: [%s]\n",ana,analysisList[ana]->CmdLine());
      if (analysisList[ana]->Analyze()==0) analysisList[ana]->Print(datafilelist); 
      // NOTE: Move print function ??
    }
  }
  mprintf("    ...................................................\n\n");
}
