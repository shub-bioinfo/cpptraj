// Action_DihedralScan
#include <cmath>
#include <ctime> // time
#include "Action_DihedralScan.h"
#include "CpptrajStdio.h"
#include "Constants.h" // DEGRAD
#include "DistRoutines.h"

// Activate DEBUG info
//#define DEBUG_DIHEDRALSCAN
#ifdef DEBUG_DIHEDRALSCAN
#include "TrajectoryFile.h"
#endif

// CONSTRUCTOR
Action_DihedralScan::Action_DihedralScan() :
  mode_(RANDOM),
  check_for_clashes_(false),
  outframe_(0),
  interval_(60.0),
  max_rotations_(0),
  max_factor_(2),
  cutoff_(0.64), // 0.8^2
  rescutoff_(100.0), // 10.0^2
  backtrack_(5),
  increment_(1),
  max_increment_(360),
  debug_(0),
  CurrentParm_(0),
  number_of_problems_(0)
{} 

Action_DihedralScan::~Action_DihedralScan() {
  outtraj_.EndTraj();
}

void Action_DihedralScan::Help() {
  mprintf("dihedralscan <mask> {interval | random | impose}\n");
  mprintf("\tOptions for 'random': [rseed <rseed>]\n");
  mprintf("\t\t[ check [cutoff <cutoff>] [rescutoff <rescutoff>]\n");
  mprintf("\t\t  [backtrack <backtrack>] [increment <increment>] [maxfactor <max_factor>] ]\n");
  mprintf("\tOptions for 'interval': <interval deg> [outtraj <filename> [<outfmt>]]\n");
  mprintf("\tOptions for 'impose': <impose deg>\n");
}

// Action_DihedralScan::init()
Action::RetType Action_DihedralScan::Init(ArgList& actionArgs, TopologyList* PFL, FrameList* FL,
                          DataSetList* DSL, DataFileList* DFL, int debugIn)
{
  TrajectoryFile::TrajFormatType outfmt;
  Topology* outtop;
  int iseed;

  debug_ = debugIn;
  // Get mask
  Mask1_.SetMaskString( actionArgs.GetMaskNext() );
  // Get Keywords - first determine mode
  if (actionArgs.hasKey("random"))
    mode_ = RANDOM;
  else if (actionArgs.hasKey("interval"))
    mode_ = INTERVAL;
  else if (actionArgs.hasKey("impose"))
    mode_ = RANDOM;
  if ( mode_ != RANDOM ) 
    interval_ = actionArgs.getNextDouble(60.0);
  if (mode_ == INTERVAL) {
    outfilename_ = actionArgs.GetStringKey("outtraj");
    if (!outfilename_.empty()) {
      outfmt = TrajectoryFile::GetFormatFromArg( actionArgs );
      outtop = PFL->GetParm( actionArgs );
      if (outtop == 0) {
        mprinterr("Error: dihedralscan: No topology for output traj.\n");
        return Action::ERR;
      }
    }
  }
  if (mode_ == RANDOM) {
    check_for_clashes_ = actionArgs.hasKey("check");
    cutoff_ = actionArgs.getKeyDouble("cutoff",0.8);
    rescutoff_ = actionArgs.getKeyDouble("rescutoff",10.0);
    backtrack_ = actionArgs.getKeyInt("backtrack",4);
    increment_ = actionArgs.getKeyInt("increment",1);
    max_factor_ = actionArgs.getKeyInt("maxfactor",2);
    // Check validity of args
    if (cutoff_ < SMALL) {
      mprinterr("Error: cutoff too small.\n");
      return Action::ERR;
    }
    if (rescutoff_ < SMALL) {
      mprinterr("Error: rescutoff too small.\n");
      return Action::ERR;
    }
    if (backtrack_ < 0) {
      mprinterr("Error: backtrack value must be >= 0\n");
      return Action::ERR;
    }
    if ( increment_<1 || (360 % increment_)!=0 ) {
      mprinterr("Error: increment must be a factor of 360.\n");
      return Action::ERR;
    }
    // Calculate max increment
    max_increment_ = 360 / increment_;
    // Seed random number gen
    iseed = actionArgs.getKeyInt("rseed",-1);
    RN_.rn_set( iseed );
  }
  // Output file for # of problems
  std::string problemFile = actionArgs.GetStringKey("out");

  // Dataset to store number of problems
  number_of_problems_ = DSL->AddSet(DataSet::INT, actionArgs.GetStringNext(),"Nprob");
  if (number_of_problems_==0) return Action::ERR;
  // Add dataset to data file list
  DFL->AddSetToFile(problemFile,number_of_problems_);

  mprintf("    DIHEDRALSCAN: Dihedrals in mask [%s]\n",Mask1_.MaskString());
  switch (mode_) {
    case RANDOM:
      mprintf("\tDihedrals will be rotated to random values.\n");
      if (iseed==-1)
        mprintf("\tRandom number generator will be seeded using time.\n");
      else
        mprintf("\tRandom number generator will be seeded using %i\n",iseed);
      if (check_for_clashes_) {
        mprintf("\tWill attempt to recover from bad steric clashes.\n");
        mprintf("\tAtom cutoff %.2lf, residue cutoff %.2lf, backtrack = %i\n",
                cutoff_, rescutoff_, backtrack_);
        mprintf("\tWhen clashes occur dihedral will be incremented by %i\n",increment_);
        mprintf("\tMax # attempted rotations = %i times number dihedrals.\n",
                max_factor_);
      }
      break;
    case INTERVAL:
      mprintf("\tDihedrals will be rotated at intervals of %.2lf degrees.\n",
              interval_);
      if (!outfilename_.empty())
        mprintf("\tCoordinates output to %s, format %s\n",outfilename_.c_str(), 
                TrajectoryFile::FormatString(outfmt));
      break;
    case IMPOSE: return Action::ERR;
  }
  // Setup output trajectory
  if (!outfilename_.empty()) {
    if (outtraj_.SetupTrajWrite(outfilename_, 0, outtop, outfmt)) return Action::ERR;
    outframe_ = 0;
  } 
  // Square cutoffs to compare to dist^2 instead of dist
  cutoff_ *= cutoff_;
  rescutoff_ *= rescutoff_;
  // Increment backtrack by 1 since we need to skip over current res
  ++backtrack_;
  // Initialize CheckStructure
  ArgList cs_args("noimage nobondcheck");
  if (checkStructure_.Init( cs_args, PFL, FL, DSL, DFL, debug_) != Action::OK) {
    mprinterr("Error: Could not set up structure check for DIHEDRALSCAN.\n");
    return Action::ERR;
  }

  return Action::OK;
}

// GetBondedAtomIdx()
static int GetBondedAtomIdx(Topology const& topIn, int atom, NameType nameIn) {
  for (Atom::bond_iterator bndatm = topIn[atom].bondbegin();
                           bndatm != topIn[atom].bondend(); ++bndatm)
  {
    if ( topIn[*bndatm].Name() == nameIn ) return *bndatm;
  }
  return -1;
}

// VisitAtom()
static void VisitAtom( Topology const& topIn, int atm, std::vector<char>& Visited )
{
  // If this atom has already been visited return
  if (Visited[atm] == 'T') return;
  // Mark this atom as visited
  Visited[atm] = 'T';
  // Visit each atom bonded to this atom
  for (Atom::bond_iterator bondedatom = topIn[atm].bondbegin();
                           bondedatom != topIn[atm].bondend(); ++bondedatom)
    VisitAtom(topIn, *bondedatom, Visited);
}

// Action_DihedralScan::setup()
/** Determine from selected mask atoms which dihedrals will be rotated. */
Action::RetType Action_DihedralScan::Setup(Topology* currentParm, Topology** parmAddress) {
  DihedralScanType dst;
  // Set up Character mask
  if ( currentParm->SetupCharMask( Mask1_ ) ) return Action::ERR;
  Mask1_.MaskInfo();
  if (Mask1_.None()) {
    mprintf("    Error: DihedralScan::setup: Mask has no atoms.\n");
    return Action::ERR;
  }
  // For now just focus on backbone phi/psi dihedrals:
  //   C-N-CA-C  N-CA-C-N
  std::vector<char> Visited( currentParm->Natom(), 'F' );
  for (int atom=0; atom < currentParm->Natom(); atom++) {
    if (Mask1_.AtomInCharMask(atom)) {
      int atom2 = -1;
      // PHI: C-N-CA-C
      if ((*currentParm)[atom].Name() == "N   ") {
        atom2 = GetBondedAtomIdx(*currentParm, atom, "CA  ");
      // PSI: N-CA-C-N
      } else if ((*currentParm)[atom].Name() == "CA  ") {
        atom2 = GetBondedAtomIdx(*currentParm, atom, "C   ");
      }
      // If a second atom was found dihedral is defined and in mask, store it
      if (atom2 != -1 && Mask1_.AtomInCharMask(atom2)) {
        // Set up mask of atoms that will move upon rotation of dihedral.
        // Also set up mask of atoms in this residue that will not move
        // upon rotation of dihedral, including atom2
        //if (currentParm->MaskOfAtomsAroundBond(atom, atom2, tmpMask)) return Action::ERR;
        dst.Rmask.ResetMask();
        Visited.assign( currentParm->Natom(), 'F' );
        // Mark atom as already visited
        Visited[atom] = 'T';
        for (Atom::bond_iterator bndatm = (*currentParm)[atom2].bondbegin();
                                 bndatm != (*currentParm)[atom2].bondend(); ++bndatm)
        {
          if ( *bndatm != atom )
            VisitAtom( *currentParm, *bndatm, Visited );
        }
        dst.checkAtoms.clear();
        int a1res = (*currentParm)[atom].ResNum();
        int a1res_start = currentParm->ResFirstAtom( a1res );
        int a1res_stop = currentParm->ResLastAtom( a1res );
        for (int maskatom = 0; maskatom < (int)Visited.size(); maskatom++) {
          if (Visited[maskatom]=='T')
            dst.Rmask.AddAtom(maskatom);
          else {
            // If this atom is in the same residue but will not move, it needs
            // to be checked for clashes since further rotations will not
            // help it.
            if (maskatom >= a1res_start && maskatom < a1res_stop   )
              dst.checkAtoms.push_back( maskatom );
          }
        }
        dst.checkAtoms.push_back( atom2 ); // atom already included from tmpMask
        dst.atom1 = atom;
        dst.atom2 = atom2;
        // Since only the second atom and atoms it is
        // bonded to move during rotation, base the check
        // on the residue of the second atom.
        dst.resnum = (*currentParm)[atom2].ResNum();
        dst.currentVal = 0;
        dst.interval = interval_;
        dst.maxVal = (int) (360.0 / interval_);
        BB_dihedrals_.push_back(dst);
      }
    } // END if atom in char mask
  }

  // DEBUG: List defined dihedrals
  if (debug_>0) {
    mprintf("DEBUG: Dihedrals (central 2 atoms only):\n");
    for (unsigned int dih = 0; dih < BB_dihedrals_.size(); dih++) {
      mprintf("\t%8i%4s %8i%4s %8i%4s\n",
              BB_dihedrals_[dih].atom1+1, (*currentParm)[BB_dihedrals_[dih].atom1].c_str(), 
              BB_dihedrals_[dih].atom2+1, (*currentParm)[BB_dihedrals_[dih].atom2].c_str(), 
              BB_dihedrals_[dih].resnum+1, currentParm->Res(BB_dihedrals_[dih].resnum).c_str());
      if (debug_>1) {
        mprintf("\t\tCheckAtoms=");
        for (std::vector<int>::iterator ca = BB_dihedrals_[dih].checkAtoms.begin();
                                        ca != BB_dihedrals_[dih].checkAtoms.end(); ca++)
        {
          mprintf(" %i",*ca + 1);
        }
        mprintf("\n");
      }
      if (debug_>2) {
        mprintf("\t\t");
        BB_dihedrals_[dih].Rmask.PrintMaskAtoms("Rmask:");
      }
    }
  }
  // Set up CheckStructure for this parm
  if (checkStructure_.Setup(currentParm, &currentParm) != Action::OK)
    return Action::ERR;

  // Set the overall max number of rotations to try
  max_rotations_ = (int) BB_dihedrals_.size();
  max_rotations_ *= max_factor_;

  // CheckStructure can take quite a long time. Set up an alternative
  // structure check. First step is coarse; check distances between a 
  // certain atom in each residue (first, COM, CA, some other atom?) 
  // to see if residues are in each others neighborhood. Second step
  // is to check the atoms in each close residue.
  if (check_for_clashes_) {
    ResidueCheckType rct;
    int Nres = currentParm->FinalSoluteRes();
    for (int res = 0; res < Nres; res++) {
      rct.resnum = res;
      rct.start = currentParm->ResFirstAtom( res );
      rct.stop = currentParm->ResLastAtom( res );
      rct.checkatom = rct.start;
      ResCheck_.push_back(rct);
    }
  }
  CurrentParm_ = currentParm;
  return Action::OK;  
}

// Action_DihedralScan::CheckResidue()
/** \return 1 if a new dihedral should be tried, 0 if no clashes, -1 if
  * \return further rotations will not help.
  */
int Action_DihedralScan::CheckResidue( Frame const& FrameIn, DihedralScanType &dih, 
                                       int nextres, double *clash ) 
{
  int resnumIn = dih.resnum;
  int rstart = ResCheck_[ resnumIn ].start;
  int rstop = ResCheck_[ resnumIn ].stop;
  int rcheck = ResCheck_[ resnumIn ].checkatom;
  // Check for clashes with self
#ifdef DEBUG_DIHEDRALSCAN
  mprintf("\tChecking residue %i\n",resnumIn+1);
  mprintf("\tATOMS %i to %i\n",rstart+1,rstop);
#endif
  for (int atom1 = rstart; atom1 < rstop - 1; atom1++) {
    for (int atom2 = atom1 + 1; atom2 < rstop; atom2++) {
      double atomD2 = DIST2_NoImage(FrameIn.XYZ(atom1), FrameIn.XYZ(atom2));
      if (atomD2 < cutoff_) {
#ifdef DEBUG_DIHEDRALSCAN 
        mprintf("\t\tRes %i Atoms %i@%s and %i@%s are close (%.3lf)\n", resnumIn+1, 
                atom1+1, currentParm->AtomName(atom1),
                atom2+1, currentParm->AtomName(atom2), sqrt(atomD2));
#endif
        *clash = atomD2;
        return 1;
      }
    }
  }
  // Check for clashes with previous residues, as well as clashes up to and
  // including the next residue in which a dihedral will be rotated.
  for (int res = 0; res <= nextres; res++) {
    if (res == resnumIn) continue;
    int rstart2 = ResCheck_[ res ].start;
    int rstop2 = ResCheck_[ res ].stop;
    int rcheck2 = ResCheck_[ res ].checkatom;
    double resD2 = DIST2_NoImage(FrameIn.XYZ(rcheck), FrameIn.XYZ(rcheck2));
    // If residues are close enough check each atom
    if (resD2 < rescutoff_) { 
#ifdef DEBUG_DIHEDRALSCAN
      mprintf("\tRES %i ATOMS %i to %i\n",res+1,rstart2+2,rstop2);
#endif
      for (int atom1 = rstart; atom1 < rstop; atom1++) {
        for (int atom2 = rstart2; atom2 < rstop2; atom2++) {
          double D2 = DIST2_NoImage(FrameIn.XYZ(atom1), FrameIn.XYZ(atom2));
          if (D2 < cutoff_) {
#ifdef DEBUG_DIHEDRALSCAN
            mprintf("\t\tRes %i atom %i@%s and res %i atom %i@%s are close (%.3lf)\n", resnumIn+1,
                    atom1+1, currentParm->AtomName(atom1), res+1,
                    atom2+1, currentParm->AtomName(atom2), sqrt(D2));
#endif
            *clash = D2;
            // If the clash involves any atom that will not be moved by further
            // rotation, indicate it is not possible to resolve clash by
            // more rotation by returning -1.
            //if (atom1 == dih.atom2 || atom1 == dih.atom1) return -1;
            for (std::vector<int>::iterator ca = dih.checkAtoms.begin();
                                            ca != dih.checkAtoms.end(); ca++) 
            {
              if (atom1 == *ca) return -1;
            }
            return 1;
          }
        }
      }
    }
  }
  return 0;
} 

// Action_DihedralScan::IntervalAngles()
void Action_DihedralScan::IntervalAngles(Frame& currentFrame) {
  Matrix_3x3 rotationMatrix;
  // Write original frame
  if (!outfilename_.empty())
    outtraj_.WriteFrame(outframe_++, CurrentParm_, currentFrame);
  for (std::vector<DihedralScanType>::iterator dih = BB_dihedrals_.begin();
                                               dih != BB_dihedrals_.end();
                                               dih++)
  {
    // Set axis of rotation
    Vec3 axisOfRotation = currentFrame.SetAxisOfRotation((*dih).atom1, (*dih).atom2);
    double theta_in_radians = (*dih).interval * DEGRAD;
    // Calculate rotation matrix for interval 
    rotationMatrix.CalcRotationMatrix(axisOfRotation, theta_in_radians);
    if (debug_ > 0) {
      std::string a1name = CurrentParm_->TruncResAtomName( (*dih).atom1 );
      std::string a2name = CurrentParm_->TruncResAtomName( (*dih).atom2 );
      mprintf("\tRotating Dih %s-%s by %.2f deg %i times.\n",
               a1name.c_str(), a2name.c_str(), (*dih).interval, (*dih).maxVal); 
    }
    for (int rot = 0; rot < (*dih).maxVal; ++rot) {
      // Rotate around axis
      currentFrame.Rotate(rotationMatrix, (*dih).Rmask);
      // Write output trajectory
      if (outtraj_.TrajIsOpen())
        outtraj_.WriteFrame(outframe_++, CurrentParm_, currentFrame);
    }
  }
}

// Action_DihedralScan::RandomizeAngles()
void Action_DihedralScan::RandomizeAngles(Frame& currentFrame) {
  Matrix_3x3 rotationMatrix;
#ifdef DEBUG_DIHEDRALSCAN
  // DEBUG
  int debugframenum=0;
  Trajout DebugTraj;
  DebugTraj.SetupTrajWrite("debugtraj.nc",0,currentParm,TrajectoryFile::AMBERNETCDF);
  DebugTraj.WriteFrame(debugframenum++,currentParm,currentFrame);
#endif
  int next_resnum;
  int bestLoop = 0;
  int number_of_rotations = 0;

  std::vector<DihedralScanType>::iterator next_dih = BB_dihedrals_.begin();
  next_dih++;
  for (std::vector<DihedralScanType>::iterator dih = BB_dihedrals_.begin();
                                               dih != BB_dihedrals_.end();
                                               dih++)
  {
    ++number_of_rotations;
    // Get the residue atom of the next dihedral. Residues up to and
    // including this residue will be checked for bad clashes 
    if (next_dih!=BB_dihedrals_.end()) 
      next_resnum = (*next_dih).resnum;
    else
      next_resnum = (*dih).resnum-1;
    // Set axis of rotation
    Vec3 axisOfRotation = currentFrame.SetAxisOfRotation((*dih).atom1, (*dih).atom2);
    // Generate random value to rotate by in radians
    // Guaranteed to rotate by at least 1 degree.
    // NOTE: could potentially rotate 360 - prevent?
    // FIXME: Just use 2PI and rn_gen, get everything in radians
    double theta_in_degrees = ((int)(RN_.rn_gen()*100000) % 360) + 1;
    double theta_in_radians = theta_in_degrees * DEGRAD;
    // Calculate rotation matrix for random theta
    rotationMatrix.CalcRotationMatrix(axisOfRotation, theta_in_radians);
    int loop_count = 0;
    double clash = 0;
    double bestClash = 0;
    if (debug_>0) mprintf("DEBUG: Rotating res %8i:\n",(*dih).resnum+1);
    bool rotate_dihedral = true;
    while (rotate_dihedral) {
      if (debug_>0) {
        mprintf("\t%8i %8i%4s %8i%4s, +%.2lf degrees (%i).\n",(*dih).resnum+1,
                (*dih).atom1+1, (*CurrentParm_)[(*dih).atom1].c_str(),
                (*dih).atom2+1, (*CurrentParm_)[(*dih).atom2].c_str(),
                theta_in_degrees,loop_count);
      }
      // Rotate around axis
      currentFrame.Rotate(rotationMatrix, (*dih).Rmask);
#ifdef DEBUG_DIHEDRALSCAN
      // DEBUG
      DebugTraj.WriteFrame(debugframenum++,currentParm,*currentFrame);
#endif
      // If we dont care about sterics exit here
      if (!check_for_clashes_) break;
      // Check resulting structure for issues
      int checkresidue = CheckResidue(currentFrame, *dih, next_resnum, &clash);
      if (checkresidue==0)
        rotate_dihedral = false;
      else if (checkresidue==-1) {
        dih--; //  0
        dih--; // -1
        next_dih = dih;
        next_dih++;
        if (debug_>0)
          mprintf("\tCannot resolve clash with further rotations, trying previous again.\n");
        break;
      }
      if (clash > bestClash) {bestClash = clash; bestLoop = loop_count;}
      //n_problems = CheckResidues( currentFrame, second_atom );
      //if (n_problems > -1) {
      //  mprintf("%i\tCheckResidues: %i problems.\n",frameNum,n_problems);
      //  rotate_dihedral = false;
      //} else if (loop_count==0) {
      if (loop_count==0 && rotate_dihedral) {
        if (debug_>0)
          mprintf("\tTrying dihedral increments of +%i\n",increment_);
        // Instead of a new random dihedral, try increments
        theta_in_degrees = (double)increment_;
        theta_in_radians = theta_in_degrees * DEGRAD;
        // Calculate rotation matrix for new theta
        rotationMatrix.CalcRotationMatrix(axisOfRotation, theta_in_radians);
      }
      ++loop_count;
      if (loop_count == max_increment_) {
        if (debug_>0)
          mprintf("%i iterations! Best clash= %.3lf at %i\n",max_increment_,
                  sqrt(bestClash),bestLoop);
        for (int bt = 0; bt < backtrack_; bt++)
          dih--;
        next_dih = dih;
        next_dih++;
        if (debug_>0)
          mprintf("\tCannot resolve clash with further rotations, trying previous %i again.\n",
                  backtrack_ - 1);
        break;
        // Calculate how much to rotate back in order to get to best clash
        /*int num_back = bestLoop - 359;
        theta_in_degrees = (double) num_back;
        theta_in_radians = theta_in_degrees * DEGRAD;
        // Calculate rotation matrix for theta
        calcRotationMatrix(rotationMatrix, axisOfRotation, theta_in_radians);
        // Rotate back to best clash
        currentFrame->RotateAroundAxis(rotationMatrix, theta_in_radians, (*dih).Rmask);
        // DEBUG
        DebugTraj.WriteFrame(debugframenum++,currentParm,*currentFrame);
        // Sanity check
        CheckResidue(currentFrame, *dih, second_atom, &clash);
        rotate_dihedral=false;*/
        //DebugTraj.EndTraj();
        //return 1;
      }
    } // End dihedral rotation loop
    next_dih++;
    // Safety valve - number of defined dihedrals times 2
    if (number_of_rotations > max_rotations_) {
      mprinterr("Error: DihedralScan: # of rotations (%i) exceeds max rotations (%i), exiting.\n",
                number_of_rotations, max_rotations_);
//#ifdef DEBUG_DIHEDRALSCAN
//      DebugTraj.EndTraj();
//#endif
      // Return gracefully for now
      break;
      //return 1;
    }
  } // End loop over dihedrals
#ifdef DEBUG_DIHEDRALSCAN
  DebugTraj.EndTraj();
  mprintf("\tNumber of rotations %i, expected %u\n",number_of_rotations,BB_dihedrals_.size());
#endif
}

// Action_DihedralScan::action()
Action::RetType Action_DihedralScan::DoAction(int frameNum, Frame* currentFrame, Frame** frameAddress) {
  switch (mode_) {
    case RANDOM: RandomizeAngles(*currentFrame); break;
    case INTERVAL: IntervalAngles(*currentFrame); break;
    case IMPOSE: break;
  }
  // Check the resulting structure
  int n_problems = checkStructure_.CheckFrame( frameNum+1, *currentFrame );
  //mprintf("%i\tResulting structure has %i problems.\n",frameNum,n_problems);
  number_of_problems_->Add(frameNum, &n_problems);

  return Action::OK;
} 

