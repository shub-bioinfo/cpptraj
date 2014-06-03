#ifndef INC_TRAJOUT_MULTI_H
#define INC_TRAJOUT_MULTI_H
#include "Trajout.h"
/// Class for writing ensemble as separate files.
class Trajout_Multi : public Trajout {
  public:
    Trajout_Multi();
    ~Trajout_Multi();
    // ----- Inherited functions -----------------
    inline int InitTrajWrite(std::string const&, ArgList const&, Topology*,
                             TrajectoryFile::TrajFormatType);
    void EndTraj();
    int WriteFrame(int, Topology*, Frame const&) { return 1; }
    int WriteEnsemble(int,Topology*,FramePtrArray const&);
    void PrintInfo(int) const;
    void SetEnsembleInfo(int i) { ensembleSize_ = i; }
    // -------------------------------------------
  private:
    void Clear();

    typedef std::vector<TrajectoryIO*> IOarrayType;
    IOarrayType ioarray_;
    typedef std::vector<std::string> Sarray;
    Sarray fileNames_;
    int ensembleSize_;
#   ifndef MPI
    std::vector<int> tIndex_;
#   endif
};
#endif
