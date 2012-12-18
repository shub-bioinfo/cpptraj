#ifndef INC_CLUSTERMATRIX_H
#define INC_CLUSTERMATRIX_H
#include "TriangleMatrix.h"
class ClusterMatrix : public TriangleMatrix {
  public:
    ClusterMatrix();
    ClusterMatrix(int);
    ClusterMatrix(const ClusterMatrix&);
    ClusterMatrix& operator=(const ClusterMatrix&);

    int SaveFile(std::string const&) const;
    int LoadFile(std::string const&, int);
    int SetupIgnore();
    /// Indicate given row/col should be ignored.
    void Ignore(int row) { ignore_[row] = true; }
    double FindMin(int&, int&) const;
    void PrintElements() const;
  private:
    static const unsigned char Magic_[];
    std::vector<bool> ignore_; ///< If true, ignore the row/col when printing/searching etc
};
#endif