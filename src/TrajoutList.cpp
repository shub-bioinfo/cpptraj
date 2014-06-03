// TrajoutList
#include "TrajoutList.h"
#include "CpptrajStdio.h"
#include "StringRoutines.h" // integerToString
#include "Trajout_Single.h"
#include "Trajout_Multi.h"

TrajoutList::TrajoutList() : debug_(0) { }

TrajoutList::~TrajoutList() {
  Clear();
}

void TrajoutList::SetDebug(int debugIn) {
  debug_ = debugIn;
  if (debug_ > 0)
    mprintf("TrajoutList debug level set to %i\n", debug_);
}

void TrajoutList::Clear() {
  for (ListType::iterator traj = trajout_.begin(); traj != trajout_.end(); ++traj) 
    delete *traj;
  trajout_.clear();
  trajoutArgs_.clear();
}

// TrajoutList::MakeEnsembleTrajout()
/** Convert all current output trajectories to ensemble output trajectories.
  */
int TrajoutList::MakeEnsembleTrajout(TopologyList const& topListIn,
                                     TrajoutList& ensembleList, int ensembleSize)
{
  ensembleList.Clear();
  for (ArgsArray::const_iterator arg = trajoutArgs_.begin();
                                 arg != trajoutArgs_.end(); ++arg)
  {
    ArgList argIn = *arg;
    // Filename must be first arg.
    std::string filename = argIn.GetStringNext();
    // Get parm from TopologyList based on args
    Topology* tempParm = topListIn.GetParm( argIn );
    if (tempParm == 0) return 1;
    // Create new multi output trajectory.
    ensembleList.trajout_.push_back( new Trajout_Multi() );
    if (ensembleList.trajout_.back() == 0) return 1;
    ensembleList.trajout_.back()->SetEnsembleInfo( ensembleSize );
    ensembleList.trajout_.back()->SetDebug( debug_ );
    if (ensembleList.trajout_.back()->InitTrajWrite(filename, *arg, tempParm,
                                                    TrajectoryFile::UNKNOWN_TRAJ))
      return 1;
  }
  return 0;
}

// TrajoutList::AddTrajout()
/** Add output trajectory to list as single output trajectory. */
int TrajoutList::AddTrajout(ArgList const& argIn, TopologyList const& topListIn) {
  // Since we need to check if this filename is in use in order to prevent
  // overwrites, determine the filename here.
  ArgList args = argIn;
  std::string filename = args.GetStringNext();
  if (filename.empty()) {
    mprinterr("Internal Error: TrajoutList::Add() called with empty filename.\n");
    return 1;
  }
  //int err = AddTrajout( filename, args, topListIn, TrajectoryFile::UNKNOWN_TRAJ );
  // Check if filename is in use
  for (ListType::const_iterator to = trajout_.begin();
                                to != trajout_.end(); ++to)
  {
    if ( (*to)->TrajFilename().Full() == filename ) { 
      mprinterr("Error: trajout: Filename %s already in use.\n",filename.c_str());
      return 1;
    }
  }
  // Get parm from TopologyList based on args
  Topology* tempParm = topListIn.GetParm( args );
  if (tempParm == 0) return 1;
  // Create trajout.
  Trajout* traj = new Trajout_Single();
  if (traj==0) {
    mprinterr("Error: TrajoutList::Add: Could not allocate memory for traj.\n");
    return 1;
  }
  traj->SetDebug(debug_);
  // Initialize output trajectory (non-topology-related setup).
  if (traj->InitTrajWrite(filename, args, tempParm, TrajectoryFile::UNKNOWN_TRAJ)) {
    mprinterr("Error: trajout: Could not set up trajectory.\n");
    delete traj;
    return 1;
  }
  // Add to trajectory file list
  trajout_.push_back(traj);
  // For potentially setting up ensemble later, save trajout arg.
  trajoutArgs_.push_back( argIn );
  return 0;
}

//TrajoutList::WriteEnsembleOut()
int TrajoutList::WriteEnsembleOut(int set, Topology* CurrentParm,
                                  Trajout::FramePtrArray const& Farray)
{
  for (ListType::const_iterator to = trajout_.begin();
                                to != trajout_.end(); ++to)
  {
    if ( (*to)->WriteEnsemble(set, CurrentParm, Farray) ) {
      mprinterr("Error writing output trajectory, frame %i.\n", set+1);
      return 1;
    }
  }
  return 0;
}

// TrajoutList::WriteTrajout()
/** Go through each output traj, call write. The first time CurrentParm
  * matches the parm the trajectory was originally set up with it will
  * be opened, no need to call BeginTraj.
  */ 
int TrajoutList::WriteTrajout(int set, Topology* CurrentParm,
                              Frame const& CurrentFrame)
{ 
  for (ListType::const_iterator traj = trajout_.begin();
                                traj != trajout_.end(); ++traj) 
  {
    if ( (*traj)->WriteFrame(set, CurrentParm, CurrentFrame) ) {
      mprinterr("Error writing output trajectory, frame %i.\n", set+1);
      return 1;
    }
  }
  return 0;
}

// TrajoutList::CloseTrajout()
/** Close output trajectories. Called after input traj processing completed. */
void TrajoutList::CloseTrajout() {
  for (ListType::const_iterator traj = trajout_.begin();
                                traj != trajout_.end(); ++traj)
    (*traj)->EndTraj();
  Clear();
}

// TrajoutList::List()
void TrajoutList::List() const {
  if (!trajout_.empty()) {
    mprintf("\nOUTPUT TRAJECTORIES:\n");
    for (ListType::const_iterator traj = trajout_.begin(); traj != trajout_.end(); ++traj)
      (*traj)->PrintInfo( 1 );
  }
}
