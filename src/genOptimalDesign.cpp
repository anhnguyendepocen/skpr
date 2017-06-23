#define ARMA_DONT_PRINT_ERRORS
#include <RcppArmadillo.h>
#include <RcppArmadilloExtensions/sample.h>
// [[Rcpp::depends(RcppArmadillo)]]
using namespace Rcpp;

double delta(arma::mat V, arma::mat x, arma::mat y) {
  return(as_scalar(-x*V*x.t() + y*V*y.t() + (y*V*x.t())*(y*V*x.t()) - (x*V*x.t())*(y*V*y.t())));
}

double calculateDOptimality(arma::mat currentDesign) {
  return(arma::det(currentDesign.t()*currentDesign));
}

double calculateIOptimality(arma::mat currentDesign, const arma::mat momentsMatrix) {
  return(trace(inv_sympd(currentDesign.t()*currentDesign)*momentsMatrix));
}

double calculateGOptimality(arma::mat currentDesign, const arma::mat candidateSet) {
  arma::mat results = candidateSet*inv_sympd(currentDesign.t()*currentDesign)*candidateSet.t();
  return(results.diag().max());
}

double calculateTOptimality(arma::mat currentDesign) {
  return(trace(currentDesign.t()*currentDesign));
}

double calculateEOptimality(arma::mat currentDesign) {
  arma::vec eigval;
  arma::eig_sym(eigval,currentDesign.t()*currentDesign);
  return(eigval.min());
}

double calculateAOptimality(arma::mat currentDesign) {
  return(trace(inv_sympd(currentDesign.t()*currentDesign)));
}

double calculateAliasTrace(arma::mat currentDesign, arma::mat aliasMatrix) {
  arma::mat A = inv_sympd(currentDesign.t()*currentDesign)*currentDesign.t()*aliasMatrix;
  return(trace(A.t() * A));
}

double calculateAliasTracePseudoInv(arma::mat currentDesign, arma::mat aliasMatrix) {
  arma::mat A = arma::pinv(currentDesign.t()*currentDesign)*currentDesign.t()*aliasMatrix;
  return(trace(A.t() * A));
}


double calculateDEff(arma::mat currentDesign) {
  return(pow(arma::det(currentDesign.t()*currentDesign),(1.0/double(currentDesign.n_cols)))/double(currentDesign.n_rows));
}

double calculateDEffNN(arma::mat currentDesign) {
  return(pow(arma::det(currentDesign.t()*currentDesign),(1.0/double(currentDesign.n_cols))));
}

template <typename T>
Rcpp::NumericVector arma2vec(const T& x) {
  return Rcpp::NumericVector(x.begin(), x.end());
}



//`@title genOptimalDesign
//`@param x an x
//`@return stufff
// [[Rcpp::export]]
List genOptimalDesign(arma::mat initialdesign, arma::mat candidatelist,const std::string condition,
                      const arma::mat momentsmatrix, NumericVector initialRows,
                      arma::mat aliasdesign, arma::mat aliascandidatelist, double minDopt) {
  RNGScope rngScope;
  unsigned int check = 0;
  unsigned int nTrials = initialdesign.n_rows;
  unsigned int maxSingularityChecks = nTrials*100;
  unsigned int totalPoints = candidatelist.n_rows;
  arma::vec candidateRow(nTrials);
  arma::mat test(initialdesign.n_cols,initialdesign.n_cols,arma::fill::zeros);
  if(nTrials < candidatelist.n_cols) {
    throw std::runtime_error("Too few runs to generate initial non-singular matrix: increase the number of runs or decrease the number of parameters in the matrix");
  }
  for(unsigned int j = 1; j < candidatelist.n_cols; j++) {
    if(all(candidatelist.col(0) == candidatelist.col(j))) {
      throw std::runtime_error("Singular model matrix from factor aliased into intercept, revise model");
    }
  }
  //Checks if the initial matrix is singular. If so, randomly generates a new design maxSingularityChecks times.
  while(check < maxSingularityChecks) {
    if(!inv_sympd(test,initialdesign.t() * initialdesign)) {
      if (nTrials < totalPoints) {
        arma::vec shuffledindices = RcppArmadillo::sample(arma::regspace(0,totalPoints-1),totalPoints,false);
        arma::vec indices(nTrials);
        for(unsigned int i = 0; i < nTrials; i++) {
          indices(i) = shuffledindices(i);
        }
        for(unsigned int row = 0; row < nTrials; row++) {
          initialdesign.row(row) = candidatelist.row(indices(row));
          aliasdesign.row(row) = aliascandidatelist.row(indices(row));
        }
      } else {
        arma::vec randomrows = RcppArmadillo::sample(arma::regspace(0,totalPoints-1), nTrials, true);
        for(unsigned int i = 0; i < nTrials; i++) {
          initialdesign.row(i) = candidatelist.row(randomrows(i));
          aliasdesign.row(i) = aliascandidatelist.row(randomrows(i));
        }
      }
      check++;
    } else {
      break;
    }
  }
  //If still no non-singular design, throws and error and exits.
  // if (!inv_sympd(test,initialdesign.t() * initialdesign)) {
  //   throw std::runtime_error("All initial attempts to generate a non-singular matrix failed. Try again or reconfigure inputs.");
  // }
  bool found = FALSE;
  double del = 0;
  int entryx = 0;
  int entryy = 0;
  double newOptimum = 0;
  double priorOptimum = 0;
  double minDelta = 10e-5;
  arma::mat V;
  double newdel;
  //Generate a D-optimal design
  if(condition == "D") {
    arma::mat temp;
    del = calculateDOptimality(initialdesign);
    newOptimum = del;
    priorOptimum = newOptimum/2;
    while((newOptimum - priorOptimum)/priorOptimum > minDelta) {
      priorOptimum = newOptimum;
      for (unsigned int i = 0; i < nTrials; i++) {
        found = FALSE;
        entryx = 0;
        entryy = 0;
        temp = initialdesign;
        for (unsigned int j = 0; j < totalPoints; j++) {
          temp.row(i) = candidatelist.row(j);
          newdel = calculateDOptimality(temp);
          if(newdel > del) {
            found = TRUE;
            entryx = i; entryy = j;
            del = newdel;
          }
        }
        if (found) {
          initialdesign.row(entryx) = candidatelist.row(entryy);
          candidateRow[i] = entryy+1;
          initialRows[i] = entryy+1;
        } else {
          candidateRow[i] = initialRows[i];
        }
      }
      newOptimum = calculateDOptimality(initialdesign);
    }
  }
  //Generate an I-optimal design
  if(condition == "I") {
    arma::mat temp;
    del = calculateIOptimality(initialdesign,momentsmatrix);
    newOptimum = del;
    priorOptimum = del*2;
    while((newOptimum - priorOptimum)/priorOptimum < -minDelta) {
      priorOptimum = newOptimum;
      for (unsigned int i = 0; i < nTrials; i++) {
        found = FALSE;
        entryx = 0;
        entryy = 0;
        temp = initialdesign;
        for (unsigned int j = 0; j < totalPoints; j++) {
          //Checks for singularity; If singular, moves to next candidate in the candidate set
          try {
            temp.row(i) = candidatelist.row(j);
            newdel = calculateIOptimality(temp,momentsmatrix);
            if(newdel < del) {
              found = TRUE;
              entryx = i; entryy = j;
              del = newdel;
            }
          } catch (std::runtime_error& e) {
            continue;
          }
        }
        if (found) {
          initialdesign.row(entryx) = candidatelist.row(entryy);
          candidateRow[i] = entryy+1;
          initialRows[i] = entryy+1;
        } else {
          candidateRow[i] = initialRows[i];
        }
      }
      newOptimum = calculateIOptimality(initialdesign,momentsmatrix);
    }
  }
  //Generate an A-optimal design
  if(condition == "A") {
    arma::mat temp;
    del = calculateAOptimality(initialdesign);
    newOptimum = del;
    priorOptimum = del*2;
    while((newOptimum - priorOptimum)/priorOptimum < -minDelta) {
      priorOptimum = newOptimum;
      for (unsigned int i = 0; i < nTrials; i++) {
        found = FALSE;
        entryx = 0;
        entryy = 0;
        temp = initialdesign;
        for (unsigned int j = 0; j < totalPoints; j++) {
          //Checks for singularity; If singular, moves to next candidate in the candidate set
          try {
            temp.row(i) = candidatelist.row(j);
            newdel = calculateAOptimality(temp);
            if(newdel < del) {
              found = TRUE;
              entryx = i; entryy = j;
              del = newdel;
            }
          } catch (std::runtime_error& e) {
            continue;
          }
        }
        if (found) {
          initialdesign.row(entryx) = candidatelist.row(entryy);
          candidateRow[i] = entryy+1;
          initialRows[i] = entryy+1;
        } else {
          candidateRow[i] = initialRows[i];
        }
      }
      newOptimum = calculateAOptimality(initialdesign);
    }
  }
  //Generate an Alias optimal design
  if(condition == "Alias") {

    arma::mat temp;
    arma::mat tempalias;
    del = calculateDOptimality(initialdesign);
    newOptimum = del;
    priorOptimum = newOptimum/2;

    while((newOptimum - priorOptimum)/priorOptimum > minDelta) {
      priorOptimum = newOptimum;
      for (unsigned int i = 0; i < nTrials; i++) {
        found = FALSE;
        entryx = 0;
        entryy = 0;
        temp = initialdesign;
        for (unsigned int j = 0; j < totalPoints; j++) {
          temp.row(i) = candidatelist.row(j);
          newdel = calculateDOptimality(temp);
          if(newdel > del) {
            found = TRUE;
            entryx = i; entryy = j;
            del = newdel;
          }
        }
        if (found) {
          initialdesign.row(entryx) = candidatelist.row(entryy);
          aliasdesign.row(entryx) = aliascandidatelist.row(entryy);

          candidateRow[i] = entryy+1;
          initialRows[i] = entryy+1;
        } else {
          candidateRow[i] = initialRows[i];
        }
      }
      newOptimum = calculateDOptimality(initialdesign);
    }

    double firstA = calculateAliasTrace(initialdesign,aliasdesign);
    double initialD = calculateDEffNN(initialdesign);
    double currentA = firstA;
    double currentD = initialD;
    double wdelta = 0.05;
    double aliasweight = 1;
    double bestA = firstA;
    double optimum;
    double first = 1;

    arma::vec candidateRowTemp = candidateRow;
    arma::vec initialRowsTemp = initialRows;
    arma::mat initialdesignTemp = initialdesign;

    arma::vec bestcandidaterow = candidateRowTemp;
    arma::mat bestaliasdesign = aliasdesign;
    arma::mat bestinitialdesign = initialdesign;

    while(firstA != 0 && currentA != 0 && aliasweight > wdelta) {

      aliasweight = aliasweight - wdelta;
      optimum = aliasweight*currentD/initialD + (1-aliasweight)*(1-currentA/firstA);
      first = 1;

      while((optimum - priorOptimum)/priorOptimum > minDelta || first == 1) {
        first++;
        priorOptimum = optimum;
        for (unsigned int i = 0; i < nTrials; i++) {
          found = FALSE;
          entryx = 0;
          entryy = 0;
          temp = initialdesignTemp;
          tempalias = aliasdesign;
          for (unsigned int j = 0; j < totalPoints; j++) {
            temp.row(i) = candidatelist.row(j);
            tempalias.row(i) = aliascandidatelist.row(j);
            currentA = calculateAliasTrace(temp,tempalias);
            currentD = calculateDEffNN(temp);

            newdel = aliasweight*currentD/initialD + (1-aliasweight)*(1-currentA/firstA);

            if(newdel > optimum && calculateDEff(temp) > minDopt) {
              found = TRUE;
              entryx = i; entryy = j;
              optimum = newdel;
            }
          }
          if (found) {
            initialdesignTemp.row(entryx) = candidatelist.row(entryy);
            aliasdesign.row(entryx) = aliascandidatelist.row(entryy);
            candidateRowTemp[i] = entryy+1;
            initialRowsTemp[i] = entryy+1;
          } else {
            candidateRowTemp[i] = initialRowsTemp[i];
          }
        }
        currentD = calculateDEffNN(initialdesignTemp);
        currentA = calculateAliasTrace(initialdesignTemp,aliasdesign);
        optimum = aliasweight*currentD/initialD + (1-aliasweight)*(1-currentA/firstA);
      }

      if(currentA < bestA) {
        bestA = currentA;
        bestaliasdesign = aliasdesign;
        bestinitialdesign = initialdesignTemp;
        bestcandidaterow = candidateRowTemp;
      }
    }
    initialdesign = bestinitialdesign;
    candidateRow = bestcandidaterow;
    aliasdesign = bestaliasdesign;
    newOptimum = bestA;
  }
  if(condition == "G") {
    arma::mat temp;
    del = calculateGOptimality(initialdesign,candidatelist);
    newOptimum = del;
    priorOptimum = del*2;
    while((newOptimum - priorOptimum)/priorOptimum < -minDelta) {
      priorOptimum = newOptimum;
      for (unsigned int i = 0; i < nTrials; i++) {
        found = FALSE;
        entryx = 0;
        entryy = 0;
        temp = initialdesign;
        for (unsigned int j = 0; j < totalPoints; j++) {
          //Checks for singularity; If singular, moves to next candidate in the candidate set
          try {
            temp.row(i) = candidatelist.row(j);
            newdel = calculateGOptimality(temp,candidatelist);
            if(newdel < del) {
              found = TRUE;
              entryx = i; entryy = j;
              del = newdel;
            }
          } catch (std::runtime_error& e) {
            continue;
          }
        }
        if (found) {
          initialdesign.row(entryx) = candidatelist.row(entryy);
          candidateRow[i] = entryy+1;
          initialRows[i] = entryy+1;
        } else {
          candidateRow[i] = initialRows[i];
        }
      }
      newOptimum = calculateGOptimality(initialdesign,candidatelist);
    }
  }
  if(condition == "T") {
    arma::mat temp;
    del = calculateTOptimality(initialdesign);
    newOptimum = del;
    priorOptimum = newOptimum/2;
    while((newOptimum - priorOptimum)/priorOptimum > minDelta) {
      priorOptimum = newOptimum;
      for (unsigned int i = 0; i < nTrials; i++) {
        found = FALSE;
        entryx = 0;
        entryy = 0;
        temp = initialdesign;
        for (unsigned int j = 0; j < totalPoints; j++) {
          temp.row(i) = candidatelist.row(j);
          newdel = calculateTOptimality(temp);
          if(newdel > del) {
            found = TRUE;
            entryx = i; entryy = j;
            del = newdel;
          }
        }
        if (found) {
          initialdesign.row(entryx) = candidatelist.row(entryy);
          candidateRow[i] = entryy+1;
          initialRows[i] = entryy+1;
        } else {
          candidateRow[i] = initialRows[i];
        }
      }
      newOptimum = calculateTOptimality(initialdesign);
    }
  }
  if(condition == "E") {
    arma::mat temp;
    del = calculateEOptimality(initialdesign);
    newOptimum = del;
    priorOptimum = newOptimum/2;
    while((newOptimum - priorOptimum)/priorOptimum > minDelta) {
      priorOptimum = newOptimum;
      for (unsigned int i = 0; i < nTrials; i++) {
        found = FALSE;
        entryx = 0;
        entryy = 0;
        temp = initialdesign;
        for (unsigned int j = 0; j < totalPoints; j++) {
          temp.row(i) = candidatelist.row(j);
          newdel = calculateEOptimality(temp);
          if(newdel > del) {
            found = TRUE;
            entryx = i; entryy = j;
            del = newdel;
          }
        }
        if (found) {
          initialdesign.row(entryx) = candidatelist.row(entryy);
          candidateRow[i] = entryy+1;
          initialRows[i] = entryy+1;
        } else {
          candidateRow[i] = initialRows[i];
        }
      }
      newOptimum = calculateEOptimality(initialdesign);
    }
  }
  //return the model matrix and a list of the candidate list indices used to construct the run matrix
  return(List::create(_["indices"] = candidateRow, _["modelmatrix"] = initialdesign, _["criterion"] = newOptimum));
}

//**********************************************************
//Everything below is for generating blocked optimal designs
//**********************************************************

double calculateBlockedDOptimality(const arma::mat currentDesign, const arma::mat gls) {
  return(arma::det(currentDesign.t()*gls*currentDesign));
}

double calculateBlockedIOptimality(const arma::mat currentDesign, const arma::mat momentsMatrix,const arma::mat gls) {
  return(trace(inv_sympd(currentDesign.t()*gls*currentDesign)*momentsMatrix));
}

//Function to calculate the A-optimality
double calculateBlockedAOptimality(arma::mat currentDesign,const arma::mat gls) {
  return(trace(inv_sympd(currentDesign.t()*gls*currentDesign)));
}

double calculateBlockedAliasTrace(arma::mat currentDesign, arma::mat aliasMatrix,const arma::mat gls) {
  arma::mat A = inv_sympd(currentDesign.t()*gls*currentDesign)*currentDesign.t()*aliasMatrix;
  return(trace(A.t() * A));
}

double calculateBlockedGOptimality(arma::mat currentDesign, const arma::mat candidateSet, const arma::mat gls) {
  arma::mat results = candidateSet*inv_sympd(currentDesign.t()*gls*currentDesign)*candidateSet.t();
  return(results.diag().max());
}

double calculateBlockedTOptimality(arma::mat currentDesign,const arma::mat gls) {
  return(trace(currentDesign.t()*gls*currentDesign));
}

double calculateBlockedEOptimality(arma::mat currentDesign,const arma::mat gls) {
  arma::vec eigval;
  arma::eig_sym(eigval,currentDesign.t()*gls*currentDesign);
  return(eigval.min());
}

double calculateBlockedDEff(arma::mat currentDesign,const arma::mat gls) {
  return(pow(arma::det(currentDesign.t()*gls*currentDesign),(1.0/double(currentDesign.n_cols)))/double(currentDesign.n_rows));
}

double calculateBlockedDEffNN(arma::mat currentDesign,const arma::mat gls) {
  return(pow(arma::det(currentDesign.t()*gls*currentDesign),(1.0/double(currentDesign.n_cols))));
}

//`@title genBlockedOptimalDesign
//`@param x an x
//`@return stufff
// [[Rcpp::export]]
List genBlockedOptimalDesign(arma::mat initialdesign, arma::mat candidatelist, const arma::mat blockeddesign,
                             const std::string condition, const arma::mat momentsmatrix, NumericVector initialRows,
                             const arma::mat blockedVar,
                             arma::mat aliasdesign, arma::mat aliascandidatelist, double minDopt) {
  RNGScope rngScope;

  //Generate blocking structure inverse covariance matrix
  const arma::mat vInv = inv_sympd(blockedVar);
  //Checks if the initial matrix is singular. If so, randomly generates a new design nTrials times.
  for(unsigned int j = 1; j < candidatelist.n_cols; j++) {
    if(all(candidatelist.col(0) == candidatelist.col(j))) {
      throw std::runtime_error("Singular model matrix from factor aliased into intercept, revise model");
    }
  }
  //Remove intercept term, as it's now located in the blocked
  candidatelist.shed_col(0);
  initialdesign.shed_col(0);

  unsigned int check = 0;
  unsigned int nTrials = initialdesign.n_rows;
  unsigned int maxSingularityChecks = nTrials*100;
  unsigned int totalPoints = candidatelist.n_rows;
  unsigned int blockedCols = blockeddesign.n_cols;
  int designCols = initialdesign.n_cols;
  arma::mat combinedDesign(nTrials,blockedCols+designCols,arma::fill::zeros);
  combinedDesign(arma::span::all,arma::span(0,blockedCols-1)) = blockeddesign;
  combinedDesign(arma::span::all,arma::span(blockedCols,blockedCols+designCols-1)) = initialdesign;
  IntegerVector candidateRow(nTrials);
  arma::mat test(combinedDesign.n_cols,combinedDesign.n_cols,arma::fill::zeros);
  if(nTrials < candidatelist.n_cols + blockedCols) {
    throw std::runtime_error("Too few runs to generate initial non-singular matrix: increase the number of runs or decrease the number of parameters in the matrix");
  }
  while(check < maxSingularityChecks) {
    if(!inv_sympd(test,combinedDesign.t() * combinedDesign)) {
      if (nTrials < totalPoints) {
        arma::vec shuffledindices = RcppArmadillo::sample(arma::regspace(0,totalPoints-1),totalPoints,false);
        arma::vec indices(nTrials);
        for(unsigned int i = 0; i < nTrials; i++) {
          indices(i) = shuffledindices(i);
        }
        for(unsigned int i = 0; i < nTrials; i++) {
          combinedDesign(i,arma::span(blockedCols,blockedCols+designCols-1)) = candidatelist.row(indices(i));
        }
      } else {
        arma::vec randomrows = RcppArmadillo::sample(arma::regspace(0,totalPoints-1), nTrials, true);
        for(unsigned int i = 0; i < nTrials; i++) {
          combinedDesign(i,arma::span(blockedCols,blockedCols+designCols-1)) = candidatelist.row(randomrows(i));
        }
      }
      check++;
    } else {
      break;
    }
  }
  //If still no non-singular design, throws and error and exits.
  // if (!inv_sympd(test,combinedDesign.t() * combinedDesign)) {
  //   throw std::runtime_error("All initial attempts to generate a non-singular matrix failed");
  // }
  double del = 0;
  bool found = FALSE;
  int entryx = 0;
  int entryy = 0;
  double newOptimum = 0;
  double priorOptimum = 0;
  double minDelta = 10e-5;
  double newdel;
  //Generate a D-optimal design, fixing the blocking factors
  if(condition == "D") {
    arma::mat temp;
    newOptimum = calculateBlockedDOptimality(combinedDesign, vInv);
    priorOptimum = newOptimum/2;
    while((newOptimum - priorOptimum)/priorOptimum > minDelta) {
      priorOptimum = newOptimum;
      del = calculateBlockedDOptimality(combinedDesign,vInv);
      for (unsigned int i = 0; i < nTrials; i++) {
        found = FALSE;
        entryx = 0;
        entryy = 0;
        temp = combinedDesign;
        for (unsigned int j = 0; j < totalPoints; j++) {
          temp(i,arma::span(blockedCols,blockedCols+designCols-1)) = candidatelist.row(j);
          newdel = calculateBlockedDOptimality(temp, vInv);
          if(newdel > del) {
            found = TRUE;
            entryx = i; entryy = j;
            del = newdel;
          }
        }
        if (found) {
          combinedDesign(entryx,arma::span(blockedCols,blockedCols+designCols-1)) = candidatelist.row(entryy);
          candidateRow[i] = entryy+1;
          initialRows[i] = entryy+1;
        } else {
          candidateRow[i] = initialRows[i];
        }
      }
      newOptimum = calculateBlockedDOptimality(combinedDesign, vInv);
    }
  }
  //Generate an I-optimal design, fixing the blocking factors
  if(condition == "I") {
    //Potential issue here: Any interactions between the blocked and regular runs need to be
    //reflected in the moments matrix correctly
    arma::mat temp;
    del = calculateBlockedIOptimality(combinedDesign,momentsmatrix,vInv);
    newOptimum = del;
    priorOptimum = del/2;
    while((newOptimum - priorOptimum)/priorOptimum > minDelta) {
      priorOptimum = newOptimum;
      for (unsigned int i = 0; i < nTrials; i++) {
        found = FALSE;
        entryx = 0;
        entryy = 0;
        temp = combinedDesign;
        for (unsigned int j = 0; j < totalPoints; j++) {
          //Checks for singularity; If singular, moves to next candidate in the candidate set
          try {
            temp(i,arma::span(blockedCols,blockedCols+designCols-1)) = candidatelist.row(j);
            newdel = calculateBlockedIOptimality(temp,momentsmatrix,vInv);
            if(newdel < del) {
              found = TRUE;
              entryx = i; entryy = j;
              del = newdel;
            }
          } catch (std::runtime_error& e) {
            continue;
          }
        }
        if (found) {
          combinedDesign(entryx,arma::span(blockedCols,blockedCols+designCols-1)) = candidatelist.row(entryy);
          candidateRow[i] = entryy+1;
          initialRows[i] = entryy+1;
        } else {
          candidateRow[i] = initialRows[i];
        }
      }
      newOptimum = calculateBlockedIOptimality(combinedDesign,momentsmatrix,vInv);
    }
  }
  //Generate an A-optimal design, fixing the blocking factors
  if(condition == "A") {
    arma::mat temp;
    del = calculateBlockedAOptimality(combinedDesign,vInv);
    newOptimum = del;
    priorOptimum = del*2;
    while((newOptimum - priorOptimum)/priorOptimum < -minDelta) {
      priorOptimum = newOptimum;
      for (unsigned int i = 0; i < nTrials; i++) {
        found = FALSE;
        entryx = 0;
        entryy = 0;
        temp = combinedDesign;
        for (unsigned int j = 0; j < totalPoints; j++) {
          //Checks for singularity; If singular, moves to next candidate in the candidate set
          try {
            temp(i,arma::span(blockedCols,blockedCols+designCols-1)) = candidatelist.row(j);
            newdel = calculateBlockedAOptimality(temp,vInv);
            if(newdel < del) {
              found = TRUE;
              entryx = i; entryy = j;
              del = newdel;
            }
          } catch (std::runtime_error& e) {
            continue;
          }
        }
        if (found) {
          combinedDesign(entryx,arma::span(blockedCols,blockedCols+designCols-1)) = candidatelist.row(entryy);
          candidateRow[i] = entryy+1;
          initialRows[i] = entryy+1;
        } else {
          candidateRow[i] = initialRows[i];
        }
      }
      newOptimum = calculateBlockedAOptimality(combinedDesign,vInv);
    }
  }
  //Commented out until bugs worked out
  /*if(condition == "G") {
  arma::mat temp;
  newOptimum = calculateBlockedGOptimality(combinedDesign, candidatelist, vInv);
  priorOptimum = newOptimum/2;
  while((newOptimum - priorOptimum)/priorOptimum > minDelta) {
  priorOptimum = newOptimum;
  del = calculateBlockedGOptimality(combinedDesign,candidatelist,vInv);
  for (unsigned int i = 0; i < nTrials; i++) {
  found = FALSE;
  entryx = 0;
  entryy = 0;
  temp = combinedDesign;
  for (unsigned int j = 0; j < totalPoints; j++) {
  temp(i,arma::span(blockedCols,blockedCols+designCols-1)) = candidatelist.row(j);
  newdel = calculateBlockedGOptimality(temp, candidatelist, vInv);
  if(newdel > del) {
  found = TRUE;
  entryx = i; entryy = j;
  del = newdel;
  }
  }
  if (found) {
  combinedDesign(entryx,arma::span(blockedCols,blockedCols+designCols-1)) = candidatelist.row(entryy);
  candidateRow[i] = entryy+1;
  initialRows[i] = entryy+1;
  } else {
  candidateRow[i] = initialRows[i];
  }
  }
  newOptimum = calculateBlockedGOptimality(combinedDesign, candidatelist, vInv);
  }
}
  if(condition == "T") {
  arma::mat temp;
  newOptimum = calculateBlockedTOptimality(combinedDesign, vInv);
  priorOptimum = newOptimum/2;
  while((newOptimum - priorOptimum)/priorOptimum > minDelta) {
  priorOptimum = newOptimum;
  del = calculateBlockedTOptimality(combinedDesign,vInv);
  for (unsigned int i = 0; i < nTrials; i++) {
  found = FALSE;
  entryx = 0;
  entryy = 0;
  temp = combinedDesign;
  for (unsigned int j = 0; j < totalPoints; j++) {
  temp(i,arma::span(blockedCols,blockedCols+designCols-1)) = candidatelist.row(j);
  newdel = calculateBlockedTOptimality(temp, vInv);
  if(newdel > del) {
  found = TRUE;
  entryx = i; entryy = j;
  del = newdel;
  }
  }
  if (found) {
  combinedDesign(entryx,arma::span(blockedCols,blockedCols+designCols-1)) = candidatelist.row(entryy);
  candidateRow[i] = entryy+1;
  initialRows[i] = entryy+1;
  } else {
  candidateRow[i] = initialRows[i];
  }
  }
  newOptimum = calculateBlockedTOptimality(combinedDesign, vInv);
  }
  }
  */
  //Generate an E-optimal design, fixing the blocking factors
  if(condition == "E") {
    arma::mat temp;
    newOptimum = calculateBlockedEOptimality(combinedDesign, vInv);
    priorOptimum = newOptimum/2;
    while((newOptimum - priorOptimum)/priorOptimum > minDelta) {
      priorOptimum = newOptimum;
      del = calculateBlockedEOptimality(combinedDesign,vInv);
      for (unsigned int i = 0; i < nTrials; i++) {
        found = FALSE;
        entryx = 0;
        entryy = 0;
        temp = combinedDesign;
        for (unsigned int j = 0; j < totalPoints; j++) {
          temp(i,arma::span(blockedCols,blockedCols+designCols-1)) = candidatelist.row(j);
          newdel = calculateBlockedEOptimality(temp, vInv);
          if(newdel > del) {
            found = TRUE;
            entryx = i; entryy = j;
            del = newdel;
          }
        }
        if (found) {
          combinedDesign(entryx,arma::span(blockedCols,blockedCols+designCols-1)) = candidatelist.row(entryy);
          candidateRow[i] = entryy+1;
          initialRows[i] = entryy+1;
        } else {
          candidateRow[i] = initialRows[i];
        }
      }
      newOptimum = calculateBlockedEOptimality(combinedDesign, vInv);
    }
  }
  //return the model matrix and a list of the candidate list indices used to construct the run matrix
  return(List::create(_["indices"] = candidateRow, _["modelmatrix"] = combinedDesign, _["criterion"] = newOptimum));
}

