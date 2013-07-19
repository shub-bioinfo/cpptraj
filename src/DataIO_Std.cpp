#include <cstdlib> // atoi, atof
#include <cstring> // strchr
#include <cctype>  // isdigit, isalpha
#include "DataIO_Std.h"
#include "CpptrajStdio.h" 
#include "StringRoutines.h" // SetStringFormatString
#include "BufferedLine.h"
#include "Array1D.h"
#include "DataSet_2D.h"

// CONSTRUCTOR
DataIO_Std::DataIO_Std() : 
  hasXcolumn_(true), 
  writeHeader_(true), 
  square2d_(false) {}

static void PrintColumnError(int idx) {
  mprinterr("Error: Number of columns in file changes at line %i.\n", idx);
}

// TODO: Set dimension labels
// DataIO_Std::ReadData()
int DataIO_Std::ReadData(std::string const& fname, ArgList& argIn,
                         DataSetList& datasetlist, std::string const& dsname)
{
  ArgList labels;
  bool hasLabels = false;
  Array1D DsetList;
  int indexcol = argIn.getKeyInt("index", -1);
  // Column user args start from 1
  if (indexcol != -1)
    mprintf("\tUsing column %i as index column.\n", indexcol--);
  const char* SEPARATORS = " ,\t"; // whitespace, comma, or tab-delimited

  // Buffer file
  BufferedLine buffer;
  if (buffer.OpenFileRead( fname )) return 1;

  // Read the first line. Attempt to determine the number of columns
  const char* linebuffer = buffer.Line();
  if (linebuffer == 0) return 1;
  int ntoken = buffer.TokenizeLine( SEPARATORS );
  if ( ntoken == 0 ) {
    mprinterr("Error: No columns detected in %s\n", buffer.Filename().full());
    return 1;
  }

  // If first line begins with a '#', assume it contains labels. Ignore any
  // leading whitespace.
  const char* ptr = linebuffer;
  while ( *ptr != '\0' && isspace(*ptr) ) ++ptr;
  if (*ptr == '#') {
    labels.SetList(ptr+1, SEPARATORS );
    hasLabels = true;
    // If first label is Frame assume it is the index column
    if (labels[0] == "Frame" && indexcol == -1) 
      indexcol = 0;
    // Read in next non '#' line, should be data.
    while (*ptr == '#') {
      linebuffer = buffer.Line();
      if (linebuffer == 0) return 1;
      ptr = linebuffer;
      while ( *ptr != '\0' && isspace(*ptr) ) ++ptr;
    }
    if (buffer.TokenizeLine( SEPARATORS ) != ntoken) {
      PrintColumnError(buffer.LineNumber());
      return 1;
    }
  }

  // Determine the type of data stored in each column 
  for (int col = 0; col < ntoken; ++col) {
    const char* token = buffer.NextToken();
    // Determine data type
    DataSet_1D* dset = 0;
    if ( isdigit( token[0] )    || 
                  token[0]=='+' || 
                  token[0]=='-' ||
                  token[0]=='.'   )
    {
      if ( strchr( token, '.' ) != 0 ) {
        if ( col != indexcol )
          dset = (DataSet_1D*)datasetlist.AddSetIdx( DataSet::DOUBLE, dsname, col+1 );
      } else {
        if (col != indexcol)
          dset = (DataSet_1D*)datasetlist.AddSetIdx( DataSet::INTEGER, dsname, col+1 );
      }
    } else {
      // Assume string
      // STRING columns cannot be index columns
      if ( col == indexcol ) {
        mprinterr("Error: DataFile %s index column %i has string values.\n", 
                  buffer.Filename().full(), indexcol+1);
        return 1;
      }
      dset = (DataSet_1D*)datasetlist.AddSetIdx( DataSet::STRING, dsname, col+1 );
    } 
    // Set legend to label if present
    if ( dset != 0 && hasLabels)
      dset->SetLegend( labels[col] );
    // Index column is the only one that should not have a DataSet.
    if ( col != indexcol && dset == 0 ) {
      mprinterr("Error: DataFile %s: Could not identify column %i", 
                buffer.Filename().full(), col+1);
      mprinterr(" (token=%s)\n",token);
      return 1;
    }
    DsetList.push_back( dset );
  }

  // Read in data.
  int ival = 0;
  double dval = 0;
  std::vector<double> Xvals;
  int indexval = 0;
  do {
    if ( buffer.TokenizeLine( SEPARATORS ) != ntoken ) {
      PrintColumnError(buffer.LineNumber());
      break;
    } 
    // Convert data in columns
    for (int i = 0; i < ntoken; ++i) {
      const char* token = buffer.NextToken();
      if (DsetList[i] == 0) {
        // Index column - always read as double
        Xvals.push_back( atof( token ) );
      } else {
        switch ( DsetList[i]->Type() ) {
          case DataSet::INTEGER: 
            ival = atoi( token ); 
            DsetList[i]->Add( indexval, &ival );
            break;
          case DataSet::DOUBLE: 
            dval = atof( token ); 
            DsetList[i]->Add( indexval, &dval );
            break;
          case DataSet::STRING: 
            DsetList[i]->Add( indexval, token );
            break;
          default: continue; 
        }
      }
    }
    ++indexval;
  } while (buffer.Line() != 0);
  buffer.CloseFile();
  mprintf("\tDataFile %s has %i columns, %i lines.\n", buffer.Filename().full(),
          ntoken, buffer.LineNumber());
  if (hasLabels) {
    mprintf("\tDataFile contains labels:\n");
    labels.PrintList();
  }
  // Determine dimension
  if (indexcol != -1) {
    mprintf("\tIndex column is %i\n", indexcol + 1);
    if (Xvals.empty()) {
      mprinterr("Error: No indices read.\n");
      return 1;
    }
    Dimension Xdim = DataIO::DetermineXdim(Xvals);
    for (int i = 0; i < ntoken; ++i)
      if (DsetList[i] != 0)
        DsetList[i]->SetDim(Dimension::X, Xdim);
  } else {
    for (int i = 0; i < ntoken; ++i)
      if (DsetList[i] != 0)
        DsetList[i]->SetDim(Dimension::X, Dimension(1.0, 1.0, DsetList[i]->Size()));
  }
  return 0;
}

// DataIO_Std::processWriteArgs()
int DataIO_Std::processWriteArgs(ArgList &argIn) {
  hasXcolumn_ = !argIn.hasKey("noxcol");
  writeHeader_ = !argIn.hasKey("noheader");
  square2d_ = argIn.hasKey("square2d");
  return 0;
}

// -----------------------------------------------------------------------------
// WriteNameToBuffer()
void DataIO_Std::WriteNameToBuffer(CpptrajFile& fileIn, std::string const& label,
                                   int width,  bool leftAlign) 
{
  std::string temp_name = label;
  // If left aligning, add '#' to name; 
  if (leftAlign) {
    if (temp_name[0]!='#')
      temp_name.insert(0,"#");
  }
  // Ensure that name will not be larger than column width.
  if ((int)temp_name.size() > width)
    temp_name.resize( width );
  // Replace any spaces with underscores
  for (std::string::iterator tc = temp_name.begin(); tc != temp_name.end(); ++tc)
    if ( *tc == ' ' )
      *tc = '_';
  // If not left-aligning there needs to be a leading blank space.
  // TODO: No truncation
  if (!leftAlign && width == (int)temp_name.size()) {
    temp_name = " " + temp_name;
    temp_name.resize( width );
  }
  // Set up header format string
  std::string header_format = SetStringFormatString(width, leftAlign);
  fileIn.Printf(header_format.c_str(), temp_name.c_str());
}
// -----------------------------------------------------------------------------

// DataIO_Std::WriteData()
int DataIO_Std::WriteData(std::string const& fname, DataSetList const& SetList)
{
  std::string x_col_format;
  int xcol_width = 8;
  int xcol_precision = 3;

  // Hold all 1D data sets.
  Array1D Sets( SetList );
  if (Sets.empty()) return 1;
  // For this output to work the X-dimension of all sets needs to match.
  // The most important things for output are min and step so just check that.
  // Use X dimension of set 0 for all set dimensions.
  Sets.CheckXDimension();
  // TODO: Check for empty dim.
  Dimension const& Xdim = static_cast<Dimension const&>(Sets[0]->Dim(0));

  // Determine size of largest DataSet.
  size_t maxFrames = Sets.DetermineMax();

  // Set up X column.
  if (hasXcolumn_) {
    // Create format string for X column based on dimension in first data set.
    if (Xdim.Step() == 1.0) xcol_precision = 0;
    x_col_format = SetupCoordFormat( maxFrames, Xdim, xcol_width, xcol_precision ); 
  } else {
    // If not writing an X-column, set the format for the first dataset
    // to left-aligned.
    Sets[0]->SetDataSetFormat( true );
  }

  // Open output file.
  CpptrajFile file;
  if (file.OpenWrite( fname )) return 1;

  // Write header to buffer
  if (writeHeader_) {
    // If x-column present, write x-label
    if (hasXcolumn_)
      WriteNameToBuffer( file, Xdim.Label(), xcol_width, true );
    // Write dataset names to header, left-aligning first set if no X-column
    Array1D::const_iterator set = Sets.begin();
    if (!hasXcolumn_)
      WriteNameToBuffer( file, (*set)->Legend(), (*set)->ColumnWidth(), true  );
    else
      WriteNameToBuffer( file, (*set)->Legend(), (*set)->ColumnWidth(), false );
    ++set;
    for (; set != Sets.end(); ++set) 
      WriteNameToBuffer( file, (*set)->Legend(), (*set)->ColumnWidth(), false );
    file.Printf("\n"); 
  }

  // Write Data
  for (size_t frame=0L; frame < maxFrames; frame++) {
    // Output Frame for each set
    if (hasXcolumn_)
      file.Printf(x_col_format.c_str(), Xdim.Coord(frame));
    for (Array1D::const_iterator set = Sets.begin(); set != Sets.end(); ++set) 
      (*set)->WriteBuffer(file, frame);
    file.Printf("\n"); 
  }
  file.CloseFile();
  return 0;
}

// DataIO_Std::WriteDataInverted()
int DataIO_Std::WriteDataInverted(std::string const& fname, DataSetList const& SetList)
{
  // Hold all 1D data sets.
  Array1D Sets( SetList );
  if (Sets.empty()) return 1;
  // Determine size of largest DataSet.
  size_t maxFrames = Sets.DetermineMax();
  // Open output file
  CpptrajFile file;
  if (file.OpenWrite( fname )) return 1;
  // Write each set to a line.
  for (Array1D::const_iterator set = Sets.begin(); set != Sets.end(); ++set) {
    // Write dataset name as first column.
    WriteNameToBuffer( file, (*set)->Legend(), (*set)->ColumnWidth(), false); 
    // Write each frame to subsequent columns
    for (size_t frame=0L; frame < maxFrames; frame++) 
      (*set)->WriteBuffer(file, frame);
    file.Printf("\n");
  }
  file.CloseFile();
  return 0;
}

// DataIO_Std::WriteData2D()
int DataIO_Std::WriteData2D( std::string const& fname, DataSet const& setIn) 
{
  if (setIn.Ndim() != 2) {
    mprinterr("Internal Error: DataSet %s in DataFile %s has %zu dimensions, expected 2.\n",
              setIn.Legend().c_str(), fname.c_str(), setIn.Ndim());
    return 1;
  }
  DataSet_2D const& set = static_cast<DataSet_2D const&>( setIn );
  int xcol_width = 8;
  int xcol_precision = 3;
  Dimension const& Xdim = static_cast<Dimension const&>(set.Dim(0));
  Dimension const& Ydim = static_cast<Dimension const&>(set.Dim(1));
  if (Xdim.Step() == 1.0) xcol_precision = 0;
  // Open output file
  CpptrajFile file;
  if (file.OpenWrite( fname )) return 1;
  
  if (square2d_) {
    std::string ycoord_fmt;
    // Print XY values in a grid:
    // x0y0 x1y0 x2y0
    // x0y1 x1y1 x2y1
    // x0y2 x1y2 x2y2
    // If file has header, top-left value will be '#<Xlabel>-<Ylabel>',
    // followed by X coordinate values.
    if (writeHeader_) {
      ycoord_fmt = SetupCoordFormat( set.Nrows(), Ydim, xcol_width, xcol_precision );
      std::string header;
      if (Xdim.Label().empty() && Ydim.Label().empty())
        header = "#Frame";
      else
        header = "#" + Xdim.Label() + "-" + Ydim.Label();
      WriteNameToBuffer( file, header, xcol_width, true );
      std::string xcoord_fmt = SetupCoordFormat( set.Ncols(), Xdim, set.ColumnWidth(), 
                                                 xcol_precision );
      for (size_t ix = 0; ix < set.Ncols(); ix++)
        file.Printf(xcoord_fmt.c_str(), Xdim.Coord( ix ));
      file.Printf("\n");
    }
    for (size_t iy = 0; iy < set.Nrows(); iy++) {
      if (writeHeader_)
        file.Printf(ycoord_fmt.c_str(), Ydim.Coord( iy ));
      for (size_t ix = 0; ix < set.Ncols(); ix++)
        set.Write2D( file, ix, iy );
      file.Printf("\n");
    }
  } else {
    // Print X Y Values
    // x y val(x,y)
    if (writeHeader_)
      file.Printf("#%s %s %s\n", Xdim.Label().c_str(), 
                  Ydim.Label().c_str(), set.Legend().c_str());
    std::string col_fmt = SetupCoordFormat( set.Ncols(), Xdim, 8, 3 ) + " " +
                          SetupCoordFormat( set.Nrows(), Ydim, 8, 3 ); 
    for (size_t iy = 0; iy < set.Nrows(); ++iy) {
      for (size_t ix = 0; ix < set.Ncols(); ++ix) {
        file.Printf(col_fmt.c_str(), Xdim.Coord( ix ), Ydim.Coord( iy ));
        set.Write2D( file, ix, iy );
        file.Printf("\n");
      }
    }
  }
  return 0;
}
