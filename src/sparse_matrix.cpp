#include "sparse_matrix.h"

// CONSTRUCTORS

SparseMatrix::SparseMatrix(std::string filename) {
  // Constructor from file
  load_from_file(filename);
}

SparseMatrix::SparseMatrix(float lo, float hi, int length, int elements) {
  // Constructor as a new sparse vector
  from_random_vector(lo, hi, length, elements);
}

// INITIALISERS

void SparseMatrix::load_from_file(std::string filename) {
  int ret_code;
  MM_typecode matcode;
  FILE *f;

  // Open the file descriptor
  if ((f = fopen(filename.c_str(), "r")) == NULL) {
    std::cerr << "Failed to open matrix file " << filename << std::endl;
    exit(-1);
  }
  // Read in the banner
  if (mm_read_banner(f, &matcode) != 0) {
    std::cerr << "Could not read matrix market banner" << std::endl;
    exit(-1);
  }
  std::cerr << "Matcode: " << matcode << std::endl;
  // Check the banner properties
  if (mm_is_matrix(matcode) && mm_is_coordinate(matcode) &&
      (mm_is_real(matcode) || mm_is_integer(matcode) ||
       mm_is_pattern(matcode))) {
    // TODO: Need to use float/integer conversions
    // TODO: Need to check if the matrix is general/symmetric etc
    // do matrix reading
    // Find size of matrix
    if ((ret_code = mm_read_mtx_crd_size(f, &rows, &cols, &nonz)) != 0) {
      std::cerr << "Cannot read matrix sizes and number of non-zeros"
                << std::endl;
      return;
    }
    std::cerr << "Rows " << rows << " cols " << cols << " non-zeros " << nonz
              << std::endl;
    // read the entries from the file
    int I, J;
    double val;
    int pat = mm_is_pattern(matcode);
    for (int i = 0; i < nonz; i++) {
      if (pat) {
        fscanf(f, "%d %d\n", &I, &J);
        val = 1.0;
      } else {
        fscanf(f, "%d %d %lg\n", &I, &J, &val);
      }
      // adjust from 1 based to 0 based
      I--;
      J--;
      nz_entries.push_back(std::make_tuple(I, J, val));
      if(mm_is_symmetric(matcode)){
        nz_entries.push_back(std::make_tuple(J, I, val));
      }
      if (i == 0) {
        ma_elem = mi_elem = val;
      } else {
        if (val > ma_elem) {
          ma_elem = val;
        }
        if (val < mi_elem) {
          mi_elem = val;
        }
      }
      // printf("Entry: %lg at (%d, %d)\n", val, I, J);
    }
  } else {
    std::cerr << "Cannot process this matrix type. Typecode: " << matcode
              << std::endl;
    exit(-1);
  }
}

void SparseMatrix::from_random_vector(float lo, float hi, int length,
                                      int elements) {
  std::srand(static_cast<unsigned>(time(0)));
  auto makeRandom = [](float _lo, float _hi) {
    return _lo +
           static_cast<float>(std::rand()) /
               (static_cast<float>(RAND_MAX / (_hi - _lo)));
  };
  rows = 1;
  cols = length;
  if (elements > length) {
    std::cerr << "Error: cannot intialise vector with more elements than length"
              << std::endl;
    exit(-2);
  } else {
    // initialise our temporary sparse vector
    std::vector<std::tuple<int, int, double>> elems(elements);
    if (elements == length) {
      std::clog << "Size/elements match - initialising pseudo-dense vector"
                << std::endl;
      // Initialise a dense "sparse" vector
      for (unsigned int i = 0; i < elems.size(); i++) {
        std::get<0>(elems[i]) = i;
        std::get<1>(elems[i]) = 0;
        std::get<2>(elems[i]) = makeRandom(lo, hi);
      }
    } else {
      std::clog
          << "Numbers don't match, initialising using fisher-yates shuffle"
          << std::endl;
      // Initialise a vector of <elements> elements
      std::vector<std::tuple<int, int, double>> elems(elements);
      // Initialise a dense vector of indicies of len <length>
      std::vector<int> indicies(length);
      for (unsigned int i = 0; i < indicies.size(); i++) {
        indicies[i] = i;
      }
      // Shuffle it
      int cIndexCounter = indicies.size();
      for (unsigned int i = 0; i < indicies.size(); i++, cIndexCounter--) {
        int randIndex = std::rand() % cIndexCounter;
        if (indicies[i] != indicies[randIndex]) {
          std::swap(indicies[i], indicies[cIndexCounter]);
        }
      }
      std::sort(indicies.begin(), indicies.begin() + elements);
      // Take the first <elements> indicies
      for (int i = 0; i < elements; i++) {
        std::get<0>(elems[i]) = indicies[i];
        std::get<1>(elems[i]) = 0;
        std::get<2>(elems[i]) = makeRandom(lo, hi);
      }
    }
    nz_entries = elems;
  }
}

// READERS

template <typename T>
SparseMatrix::ellpack_matrix<T> SparseMatrix::asELLPACK(void) {
  // allocate a sparse matrix of the right height
  ellpack_matrix<T> ellmatrix(height(), ellpack_row<T>(0));
  // iterate over the raw entries, and push them into the correct rows
  for (auto entry : nz_entries) {
    // y is entry._1 (right?)
    int x = std::get<0>(entry);
    int y = std::get<1>(entry);
    int val = static_cast<T>(std::get<2>(entry));
    std::pair<int, T> r_entry(x, val);
    ellmatrix[y].push_back(r_entry);
  }
  // sort the rows by the x value
  for (auto row : ellmatrix) {
    std::sort(row.begin(), row.end(),
              [](std::pair<int, T> a, std::pair<int, T> b) {
                return a.first < b.first;
              });
  }
  // return the matrix
  return ellmatrix;
}

template SparseMatrix::ellpack_matrix<double> SparseMatrix::asELLPACK(void);
template SparseMatrix::ellpack_matrix<float> SparseMatrix::asELLPACK(void);
template SparseMatrix::ellpack_matrix<int> SparseMatrix::asELLPACK(void);

template <typename T>
SparseMatrix::soa_ellpack_matrix<T> SparseMatrix::asSOAELLPACK(void) {
  // allocate a sparse matrix of the right height
  SparseMatrix::soa_ellpack_matrix<T> soaellmatrix(
      std::vector<std::vector<int>>(height(), std::vector<int>(0)),
      std::vector<std::vector<T>>(height(), std::vector<T>(0)));

  // build a zipped (AOS) ellpack matrix, and then unzip it
  auto aosellmatrix = asELLPACK<T>();

  // traverse the zipped matrix and push it into our unzipped form
  int row_idx = 0;
  for (auto row : aosellmatrix) {
    for (auto elem : row) {
      soaellmatrix.first[row_idx].push_back(elem.first);
      soaellmatrix.second[row_idx].push_back(elem.second);
    }
    row_idx++;
  }
  return soaellmatrix;
}

template SparseMatrix::soa_ellpack_matrix<double>
SparseMatrix::asSOAELLPACK(void);

template SparseMatrix::soa_ellpack_matrix<float>
SparseMatrix::asSOAELLPACK(void);

template SparseMatrix::soa_ellpack_matrix<int> SparseMatrix::asSOAELLPACK(void);

template <typename T>
SparseMatrix::soa_ellpack_matrix<T>
SparseMatrix::asPaddedSOAELLPACK(T zero, int modulo) {
  // get an unpadded soaell matrix
  auto soaellmatrix = asSOAELLPACK<T>();
  // get our padlength - it's the maximum row length
  auto max_length = getMaxRowEntries();
  // and pad that out
  auto padded_length = max_length + (modulo - (max_length % modulo));
  std::cerr << "Max length: " << max_length << ", padded (by " << modulo
            << "): " << padded_length << std::endl;
  // iterate over the rows and pad them out
  for (auto &idx_row : soaellmatrix.first) {
    idx_row.resize(padded_length, -1);
  }
  for (auto &elem_row : soaellmatrix.second) {
    elem_row.resize(padded_length, zero);
  }
  // finally, return our resized matrix
  return soaellmatrix;
}

template SparseMatrix::soa_ellpack_matrix<double>
SparseMatrix::asPaddedSOAELLPACK(double, int);

template SparseMatrix::soa_ellpack_matrix<float>
SparseMatrix::asPaddedSOAELLPACK(float, int);

template SparseMatrix::soa_ellpack_matrix<int>
SparseMatrix::asPaddedSOAELLPACK(int, int);

// ellpack_matrix<float> SparseMatrix::asFloatELLPACK() {
//     return SparseMatrix::asELLPACK<float>();
// }

// ellpack_matrix<double> SparseMatrix::asFloatELLPACK() {
//     return SparseMatrix::asELLPACK<double>();
// }

// ellpack_matrix<int> SparseMatrix::asIntELLPACK() {
//     return SparseMatrix::asELLPACK<int>();
// }

int SparseMatrix::width() { return cols; }

int SparseMatrix::height() { return rows; }

int SparseMatrix::nonZeros() { return nonz; }

double SparseMatrix::maxElement() { return ma_elem; }

double SparseMatrix::minElement() { return mi_elem; }

std::vector<std::tuple<int, int, double>> SparseMatrix::getEntries() {
  return nz_entries;
}

// std::vector<Apart_Tuple> SparseMatrix::asSparseMatrix()
// {
//     // handy holder for the total number of non zero entries
//     int entries = nz_entries.size();
//     // the output vector - the sparse matrix itself
//     std::vector<Apart_Tuple> outVect(entries);
//     // the entry count for each row
//     std::vector<int> ecount = rowEntries();
//     //
//     std::vector<int> ptrs(ecount.size(), 0);
//     std::partial_sum(ecount.begin(), ecount.end(), ptrs.begin()+1);
//     return outVect;
// }

// std::vector<Apart_Tuple> SparseMatrix::asPaddedSparseMatrix(int modulo_pad)
// {
//     // the maximum number of entries in a row
//     int mxr_entries = maxRowEntries();
//     mxr_entries = mxr_entries + (mxr_entries % modulo_pad);
//     // a "null" padding tuple
//     Apart_Tuple nt = (Apart_Tuple){-1,-1};
//     // the output vector - the sparse matrix itself
//     std::vector<Apart_Tuple> matrix(mxr_entries*rows, nt);
//     // pointers into the ``flattened'' array
//     std::vector<int> ptrs(rows, 0);
//     for(int i = 1;i<rows;i++){
//         ptrs[i] = i*mxr_entries;
//     }
//     // iterate over the entries, update the pointers, and write into the
//     array
//     int y;
//     Apart_Tuple tt;
//     for(unsigned int i = 0;i<nz_entries.size();i++){
//         tt._0 = std::get<0>(nz_entries[i]);
//         y     = std::get<1>(nz_entries[i]);
//         tt._1 = std::get<2>(nz_entries[i]);
//         matrix[ptrs[y]] = tt;
//         ptrs[y]++;
//     }
//     auto compareTupleFloatFloat = [](Apart_Tuple a, Apart_Tuple b){
//         return (a._0 < b._0);
//     };
//     for(int i = 0;i<rows;i++){
//         std::sort(matrix.begin() + (mxr_entries*i),
//             matrix.begin() + ptrs[i],
//             compareTupleFloatFloat
//             );
//     }
//     return matrix;
// }

// std::vector<int> SparseMatrix::rowLengths(){
//     std::vector<int> lens(rows, 0);
//     int x,y;
//     for(unsigned int i = 0; i<nz_entries.size(); i++){
//         // get the x/y entries of this coordinate
//         x = std::get<0>(nz_entries[i]);
//         y = std::get<1>(nz_entries[i]);
//         // the length is the max element + 1 (as zero indexed)
//         // perform CAS with x + 1
//         if( lens[y] < x + 1  ) {
//             lens[y] = x + 1 ;
//         }
//     }
//     return lens;
// }

// int SparseMatrix::maxRowLength(){
//     auto lens = rowLengths();
//     return *std::max_element(lens.begin(), lens.end());
// }

std::vector<int> SparseMatrix::getRowLengths() {
  if (!(row_lengths.size() > 0)) {
    std::cerr << "Building row entries for first time." << std::endl;
    std::vector<int> entries(rows, 0);
    int y;
    for (unsigned int i = 0; i < nz_entries.size(); i++) {
      // get the x/y entries of this coordinate
      y = std::get<1>(nz_entries[i]);
      // increment the entry count for this row
      entries[y]++;
    }
    row_lengths = entries;
  }
  return row_lengths;
}

int SparseMatrix::getMaxRowEntries() {
  if (max_row_entries == -1) {
    auto entries = getRowLengths();
    max_row_entries = *std::max_element(entries.begin(), entries.end());
  }
  return max_row_entries;
}

int SparseMatrix::getMinRowEntries() {
  if (min_row_entries == -1) {
    auto entries = getRowLengths();
    min_row_entries = *std::min_element(entries.begin(), entries.end());
  }
  return min_row_entries;
}

int SparseMatrix::getMeanRowEntries() {
  if (mean_row_entries == -1) {
    auto entries = getRowLengths();
    mean_row_entries =
        std::accumulate(entries.begin(), entries.end(), 0) / entries.size();
  }
  return mean_row_entries;
}