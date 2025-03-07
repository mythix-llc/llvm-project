//===- Simplex.cpp - MLIR Simplex Class -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "mlir/Analysis/Presburger/Simplex.h"
#include "mlir/Analysis/Presburger/Matrix.h"
#include "mlir/Support/MathExtras.h"
#include "llvm/ADT/Optional.h"

namespace mlir {

using namespace presburger_utils;
using Direction = Simplex::Direction;

const int nullIndex = std::numeric_limits<int>::max();

SimplexBase::SimplexBase(unsigned nVar, bool mustUseBigM)
    : usingBigM(mustUseBigM), nRow(0), nCol(getNumFixedCols() + nVar),
      nRedundant(0), tableau(0, nCol), empty(false) {
  colUnknown.insert(colUnknown.begin(), getNumFixedCols(), nullIndex);
  for (unsigned i = 0; i < nVar; ++i) {
    var.emplace_back(Orientation::Column, /*restricted=*/false,
                     /*pos=*/getNumFixedCols() + i);
    colUnknown.push_back(i);
  }
}

const Simplex::Unknown &SimplexBase::unknownFromIndex(int index) const {
  assert(index != nullIndex && "nullIndex passed to unknownFromIndex");
  return index >= 0 ? var[index] : con[~index];
}

const Simplex::Unknown &SimplexBase::unknownFromColumn(unsigned col) const {
  assert(col < nCol && "Invalid column");
  return unknownFromIndex(colUnknown[col]);
}

const Simplex::Unknown &SimplexBase::unknownFromRow(unsigned row) const {
  assert(row < nRow && "Invalid row");
  return unknownFromIndex(rowUnknown[row]);
}

Simplex::Unknown &SimplexBase::unknownFromIndex(int index) {
  assert(index != nullIndex && "nullIndex passed to unknownFromIndex");
  return index >= 0 ? var[index] : con[~index];
}

Simplex::Unknown &SimplexBase::unknownFromColumn(unsigned col) {
  assert(col < nCol && "Invalid column");
  return unknownFromIndex(colUnknown[col]);
}

Simplex::Unknown &SimplexBase::unknownFromRow(unsigned row) {
  assert(row < nRow && "Invalid row");
  return unknownFromIndex(rowUnknown[row]);
}

/// Add a new row to the tableau corresponding to the given constant term and
/// list of coefficients. The coefficients are specified as a vector of
/// (variable index, coefficient) pairs.
unsigned SimplexBase::addRow(ArrayRef<int64_t> coeffs, bool makeRestricted) {
  assert(coeffs.size() == var.size() + 1 &&
         "Incorrect number of coefficients!");

  ++nRow;
  // If the tableau is not big enough to accomodate the extra row, we extend it.
  if (nRow >= tableau.getNumRows())
    tableau.resizeVertically(nRow);
  rowUnknown.push_back(~con.size());
  con.emplace_back(Orientation::Row, makeRestricted, nRow - 1);

  // Zero out the new row.
  tableau.fillRow(nRow - 1, 0);

  tableau(nRow - 1, 0) = 1;
  tableau(nRow - 1, 1) = coeffs.back();
  if (usingBigM) {
    // When the lexicographic pivot rule is used, instead of the variables
    //
    // x, y, z ...
    //
    // we internally use the variables
    //
    // M, M + x, M + y, M + z, ...
    //
    // where M is the big M parameter. As such, when the user tries to add
    // a row ax + by + cz + d, we express it in terms of our internal variables
    // as -(a + b + c)M + a(M + x) + b(M + y) + c(M + z) + d.
    int64_t bigMCoeff = 0;
    for (unsigned i = 0; i < coeffs.size() - 1; ++i)
      bigMCoeff -= coeffs[i];
    // The coefficient to the big M parameter is stored in column 2.
    tableau(nRow - 1, 2) = bigMCoeff;
  }

  // Process each given variable coefficient.
  for (unsigned i = 0; i < var.size(); ++i) {
    unsigned pos = var[i].pos;
    if (coeffs[i] == 0)
      continue;

    if (var[i].orientation == Orientation::Column) {
      // If a variable is in column position at column col, then we just add the
      // coefficient for that variable (scaled by the common row denominator) to
      // the corresponding entry in the new row.
      tableau(nRow - 1, pos) += coeffs[i] * tableau(nRow - 1, 0);
      continue;
    }

    // If the variable is in row position, we need to add that row to the new
    // row, scaled by the coefficient for the variable, accounting for the two
    // rows potentially having different denominators. The new denominator is
    // the lcm of the two.
    int64_t lcm = mlir::lcm(tableau(nRow - 1, 0), tableau(pos, 0));
    int64_t nRowCoeff = lcm / tableau(nRow - 1, 0);
    int64_t idxRowCoeff = coeffs[i] * (lcm / tableau(pos, 0));
    tableau(nRow - 1, 0) = lcm;
    for (unsigned col = 1; col < nCol; ++col)
      tableau(nRow - 1, col) =
          nRowCoeff * tableau(nRow - 1, col) + idxRowCoeff * tableau(pos, col);
  }

  normalizeRow(nRow - 1);
  // Push to undo log along with the index of the new constraint.
  undoLog.push_back(UndoLogEntry::RemoveLastConstraint);
  return con.size() - 1;
}

/// Normalize the row by removing factors that are common between the
/// denominator and all the numerator coefficients.
void SimplexBase::normalizeRow(unsigned row) {
  int64_t gcd = 0;
  for (unsigned col = 0; col < nCol; ++col) {
    gcd = llvm::greatestCommonDivisor(gcd, std::abs(tableau(row, col)));
    // If the gcd becomes 1 then the row is already normalized.
    if (gcd == 1)
      return;
  }

  // Note that the gcd can never become zero since the first element of the row,
  // the denominator, is non-zero.
  assert(gcd != 0);
  for (unsigned col = 0; col < nCol; ++col)
    tableau(row, col) /= gcd;
}

namespace {
bool signMatchesDirection(int64_t elem, Direction direction) {
  assert(elem != 0 && "elem should not be 0");
  return direction == Direction::Up ? elem > 0 : elem < 0;
}

Direction flippedDirection(Direction direction) {
  return direction == Direction::Up ? Direction::Down : Simplex::Direction::Up;
}
} // namespace

MaybeOptimum<SmallVector<Fraction, 8>> LexSimplex::getRationalLexMin() {
  restoreRationalConsistency();
  return getRationalSample();
}

bool LexSimplex::rowIsViolated(unsigned row) const {
  if (tableau(row, 2) < 0)
    return true;
  if (tableau(row, 2) == 0 && tableau(row, 1) < 0)
    return true;
  return false;
}

Optional<unsigned> LexSimplex::maybeGetViolatedRow() const {
  for (unsigned row = 0; row < nRow; ++row)
    if (rowIsViolated(row))
      return row;
  return {};
}

// We simply look for violated rows and keep trying to move them to column
// orientation, which always succeeds unless the constraints have no solution
// in which case we just give up and return.
void LexSimplex::restoreRationalConsistency() {
  while (Optional<unsigned> maybeViolatedRow = maybeGetViolatedRow()) {
    LogicalResult status = moveRowUnknownToColumn(*maybeViolatedRow);
    if (failed(status))
      return;
  }
}

// Move the row unknown to column orientation while preserving lexicopositivity
// of the basis transform.
//
// We only consider pivots where the pivot element is positive. Suppose no such
// pivot exists, i.e., some violated row has no positive coefficient for any
// basis unknown. The row can be represented as (s + c_1*u_1 + ... + c_n*u_n)/d,
// where d is the denominator, s is the sample value and the c_i are the basis
// coefficients. Since any feasible assignment of the basis satisfies u_i >= 0
// for all i, and we have s < 0 as well as c_i < 0 for all i, any feasible
// assignment would violate this row and therefore the constraints have no
// solution.
//
// We can preserve lexicopositivity by picking the pivot column with positive
// pivot element that makes the lexicographically smallest change to the sample
// point.
//
// Proof. Let
// x = (x_1, ... x_n) be the variables,
// z = (z_1, ... z_m) be the constraints,
// y = (y_1, ... y_n) be the current basis, and
// define w = (x_1, ... x_n, z_1, ... z_m) = B*y + s.
// B is basically the simplex tableau of our implementation except that instead
// of only describing the transform to get back the non-basis unknowns, it
// defines the values of all the unknowns in terms of the basis unknowns.
// Similarly, s is the column for the sample value.
//
// Our goal is to show that each column in B, restricted to the first n
// rows, is lexicopositive after the pivot if it is so before. This is
// equivalent to saying the columns in the whole matrix are lexicopositive;
// there must be some non-zero element in every column in the first n rows since
// the n variables cannot be spanned without using all the n basis unknowns.
//
// Consider a pivot where z_i replaces y_j in the basis. Recall the pivot
// transform for the tableau derived for SimplexBase::pivot:
//
//            pivot col    other col                   pivot col    other col
// pivot row     a             b       ->   pivot row     1/a         -b/a
// other row     c             d            other row     c/a        d - bc/a
//
// Similarly, a pivot results in B changing to B' and c to c'; the difference
// between the tableau and these matrices B and B' is that there is no special
// case for the pivot row, since it continues to represent the same unknown. The
// same formula applies for all rows:
//
// B'.col(j) = B.col(j) / B(i,j)
// B'.col(k) = B.col(k) - B(i,k) * B.col(j) / B(i,j) for k != j
// and similarly, s' = s - s_i * B.col(j) / B(i,j).
//
// Since the row is violated, we have s_i < 0, so the change in sample value
// when pivoting with column a is lexicographically smaller than that when
// pivoting with column b iff B.col(a) / B(i, a) is lexicographically smaller
// than B.col(b) / B(i, b).
//
// Since B(i, j) > 0, column j remains lexicopositive.
//
// For the other columns, suppose C.col(k) is not lexicopositive.
// This means that for some p, for all t < p,
// C(t,k) = 0 => B(t,k) = B(t,j) * B(i,k) / B(i,j) and
// C(t,k) < 0 => B(p,k) < B(t,j) * B(i,k) / B(i,j),
// which is in contradiction to the fact that B.col(j) / B(i,j) must be
// lexicographically smaller than B.col(k) / B(i,k), since it lexicographically
// minimizes the change in sample value.
LogicalResult LexSimplex::moveRowUnknownToColumn(unsigned row) {
  Optional<unsigned> maybeColumn;
  for (unsigned col = 3; col < nCol; ++col) {
    if (tableau(row, col) <= 0)
      continue;
    maybeColumn =
        !maybeColumn ? col : getLexMinPivotColumn(row, *maybeColumn, col);
  }

  if (!maybeColumn) {
    markEmpty();
    return failure();
  }

  pivot(row, *maybeColumn);
  return success();
}

unsigned LexSimplex::getLexMinPivotColumn(unsigned row, unsigned colA,
                                          unsigned colB) const {
  // A pivot causes the following change. (in the diagram the matrix elements
  // are shown as rationals and there is no common denominator used)
  //
  //            pivot col    big M col      const col
  // pivot row     a            p               b
  // other row     c            q               d
  //                        |
  //                        v
  //
  //            pivot col    big M col      const col
  // pivot row     1/a         -p/a           -b/a
  // other row     c/a        q - pc/a       d - bc/a
  //
  // Let the sample value of the pivot row be s = pM + b before the pivot. Since
  // the pivot row represents a violated constraint we know that s < 0.
  //
  // If the variable is a non-pivot column, its sample value is zero before and
  // after the pivot.
  //
  // If the variable is the pivot column, then its sample value goes from 0 to
  // (-p/a)M + (-b/a), i.e. 0 to -(pM + b)/a. Thus the change in the sample
  // value is -s/a.
  //
  // If the variable is the pivot row, it sampel value goes from s to 0, for a
  // change of -s.
  //
  // If the variable is a non-pivot row, its sample value changes from
  // qM + d to qM + d + (-pc/a)M + (-bc/a). Thus the change in sample value
  // is -(pM + b)(c/a) = -sc/a.
  //
  // Thus the change in sample value is either 0, -s/a, -s, or -sc/a. Here -s is
  // fixed for all calls to this function since the row and tableau are fixed.
  // The callee just wants to compare the return values with the return value of
  // other invocations of the same function. So the -s is common for all
  // comparisons involved and can be ignored, since -s is strictly positive.
  //
  // Thus we take away this common factor and just return 0, 1/a, 1, or c/a as
  // appropriate. This allows us to run the entire algorithm without ever having
  // to fix a value of M.
  auto getSampleChangeCoeffForVar = [this, row](unsigned col,
                                                const Unknown &u) -> Fraction {
    int64_t a = tableau(row, col);
    if (u.orientation == Orientation::Column) {
      // Pivot column case.
      if (u.pos == col)
        return {1, a};

      // Non-pivot column case.
      return {0, 1};
    }

    // Pivot row case.
    if (u.pos == row)
      return {1, 1};

    // Non-pivot row case.
    int64_t c = tableau(u.pos, col);
    return {c, a};
  };

  for (const Unknown &u : var) {
    Fraction changeA = getSampleChangeCoeffForVar(colA, u);
    Fraction changeB = getSampleChangeCoeffForVar(colB, u);
    if (changeA < changeB)
      return colA;
    if (changeA > changeB)
      return colB;
  }

  // If we reached here, both result in exactly the same changes, so it
  // doesn't matter which we return.
  return colA;
}

/// Find a pivot to change the sample value of the row in the specified
/// direction. The returned pivot row will involve `row` if and only if the
/// unknown is unbounded in the specified direction.
///
/// To increase (resp. decrease) the value of a row, we need to find a live
/// column with a non-zero coefficient. If the coefficient is positive, we need
/// to increase (decrease) the value of the column, and if the coefficient is
/// negative, we need to decrease (increase) the value of the column. Also,
/// we cannot decrease the sample value of restricted columns.
///
/// If multiple columns are valid, we break ties by considering a lexicographic
/// ordering where we prefer unknowns with lower index.
Optional<SimplexBase::Pivot> Simplex::findPivot(int row,
                                                Direction direction) const {
  Optional<unsigned> col;
  for (unsigned j = 2; j < nCol; ++j) {
    int64_t elem = tableau(row, j);
    if (elem == 0)
      continue;

    if (unknownFromColumn(j).restricted &&
        !signMatchesDirection(elem, direction))
      continue;
    if (!col || colUnknown[j] < colUnknown[*col])
      col = j;
  }

  if (!col)
    return {};

  Direction newDirection =
      tableau(row, *col) < 0 ? flippedDirection(direction) : direction;
  Optional<unsigned> maybePivotRow = findPivotRow(row, newDirection, *col);
  return Pivot{maybePivotRow.getValueOr(row), *col};
}

/// Swap the associated unknowns for the row and the column.
///
/// First we swap the index associated with the row and column. Then we update
/// the unknowns to reflect their new position and orientation.
void SimplexBase::swapRowWithCol(unsigned row, unsigned col) {
  std::swap(rowUnknown[row], colUnknown[col]);
  Unknown &uCol = unknownFromColumn(col);
  Unknown &uRow = unknownFromRow(row);
  uCol.orientation = Orientation::Column;
  uRow.orientation = Orientation::Row;
  uCol.pos = col;
  uRow.pos = row;
}

void SimplexBase::pivot(Pivot pair) { pivot(pair.row, pair.column); }

/// Pivot pivotRow and pivotCol.
///
/// Let R be the pivot row unknown and let C be the pivot col unknown.
/// Since initially R = a*C + sum b_i * X_i
/// (where the sum is over the other column's unknowns, x_i)
/// C = (R - (sum b_i * X_i))/a
///
/// Let u be some other row unknown.
/// u = c*C + sum d_i * X_i
/// So u = c*(R - sum b_i * X_i)/a + sum d_i * X_i
///
/// This results in the following transform:
///            pivot col    other col                   pivot col    other col
/// pivot row     a             b       ->   pivot row     1/a         -b/a
/// other row     c             d            other row     c/a        d - bc/a
///
/// Taking into account the common denominators p and q:
///
///            pivot col    other col                    pivot col   other col
/// pivot row     a/p          b/p     ->   pivot row      p/a         -b/a
/// other row     c/q          d/q          other row     cp/aq    (da - bc)/aq
///
/// The pivot row transform is accomplished be swapping a with the pivot row's
/// common denominator and negating the pivot row except for the pivot column
/// element.
void SimplexBase::pivot(unsigned pivotRow, unsigned pivotCol) {
  assert(pivotCol >= getNumFixedCols() && "Refusing to pivot invalid column");

  swapRowWithCol(pivotRow, pivotCol);
  std::swap(tableau(pivotRow, 0), tableau(pivotRow, pivotCol));
  // We need to negate the whole pivot row except for the pivot column.
  if (tableau(pivotRow, 0) < 0) {
    // If the denominator is negative, we negate the row by simply negating the
    // denominator.
    tableau(pivotRow, 0) = -tableau(pivotRow, 0);
    tableau(pivotRow, pivotCol) = -tableau(pivotRow, pivotCol);
  } else {
    for (unsigned col = 1; col < nCol; ++col) {
      if (col == pivotCol)
        continue;
      tableau(pivotRow, col) = -tableau(pivotRow, col);
    }
  }
  normalizeRow(pivotRow);

  for (unsigned row = 0; row < nRow; ++row) {
    if (row == pivotRow)
      continue;
    if (tableau(row, pivotCol) == 0) // Nothing to do.
      continue;
    tableau(row, 0) *= tableau(pivotRow, 0);
    for (unsigned j = 1; j < nCol; ++j) {
      if (j == pivotCol)
        continue;
      // Add rather than subtract because the pivot row has been negated.
      tableau(row, j) = tableau(row, j) * tableau(pivotRow, 0) +
                        tableau(row, pivotCol) * tableau(pivotRow, j);
    }
    tableau(row, pivotCol) *= tableau(pivotRow, pivotCol);
    normalizeRow(row);
  }
}

/// Perform pivots until the unknown has a non-negative sample value or until
/// no more upward pivots can be performed. Return success if we were able to
/// bring the row to a non-negative sample value, and failure otherwise.
LogicalResult Simplex::restoreRow(Unknown &u) {
  assert(u.orientation == Orientation::Row &&
         "unknown should be in row position");

  while (tableau(u.pos, 1) < 0) {
    Optional<Pivot> maybePivot = findPivot(u.pos, Direction::Up);
    if (!maybePivot)
      break;

    pivot(*maybePivot);
    if (u.orientation == Orientation::Column)
      return success(); // the unknown is unbounded above.
  }
  return success(tableau(u.pos, 1) >= 0);
}

/// Find a row that can be used to pivot the column in the specified direction.
/// This returns an empty optional if and only if the column is unbounded in the
/// specified direction (ignoring skipRow, if skipRow is set).
///
/// If skipRow is set, this row is not considered, and (if it is restricted) its
/// restriction may be violated by the returned pivot. Usually, skipRow is set
/// because we don't want to move it to column position unless it is unbounded,
/// and we are either trying to increase the value of skipRow or explicitly
/// trying to make skipRow negative, so we are not concerned about this.
///
/// If the direction is up (resp. down) and a restricted row has a negative
/// (positive) coefficient for the column, then this row imposes a bound on how
/// much the sample value of the column can change. Such a row with constant
/// term c and coefficient f for the column imposes a bound of c/|f| on the
/// change in sample value (in the specified direction). (note that c is
/// non-negative here since the row is restricted and the tableau is consistent)
///
/// We iterate through the rows and pick the row which imposes the most
/// stringent bound, since pivoting with a row changes the row's sample value to
/// 0 and hence saturates the bound it imposes. We break ties between rows that
/// impose the same bound by considering a lexicographic ordering where we
/// prefer unknowns with lower index value.
Optional<unsigned> Simplex::findPivotRow(Optional<unsigned> skipRow,
                                         Direction direction,
                                         unsigned col) const {
  Optional<unsigned> retRow;
  // Initialize these to zero in order to silence a warning about retElem and
  // retConst being used uninitialized in the initialization of `diff` below. In
  // reality, these are always initialized when that line is reached since these
  // are set whenever retRow is set.
  int64_t retElem = 0, retConst = 0;
  for (unsigned row = nRedundant; row < nRow; ++row) {
    if (skipRow && row == *skipRow)
      continue;
    int64_t elem = tableau(row, col);
    if (elem == 0)
      continue;
    if (!unknownFromRow(row).restricted)
      continue;
    if (signMatchesDirection(elem, direction))
      continue;
    int64_t constTerm = tableau(row, 1);

    if (!retRow) {
      retRow = row;
      retElem = elem;
      retConst = constTerm;
      continue;
    }

    int64_t diff = retConst * elem - constTerm * retElem;
    if ((diff == 0 && rowUnknown[row] < rowUnknown[*retRow]) ||
        (diff != 0 && !signMatchesDirection(diff, direction))) {
      retRow = row;
      retElem = elem;
      retConst = constTerm;
    }
  }
  return retRow;
}

bool SimplexBase::isEmpty() const { return empty; }

void SimplexBase::swapRows(unsigned i, unsigned j) {
  if (i == j)
    return;
  tableau.swapRows(i, j);
  std::swap(rowUnknown[i], rowUnknown[j]);
  unknownFromRow(i).pos = i;
  unknownFromRow(j).pos = j;
}

void SimplexBase::swapColumns(unsigned i, unsigned j) {
  assert(i < nCol && j < nCol && "Invalid columns provided!");
  if (i == j)
    return;
  tableau.swapColumns(i, j);
  std::swap(colUnknown[i], colUnknown[j]);
  unknownFromColumn(i).pos = i;
  unknownFromColumn(j).pos = j;
}

/// Mark this tableau empty and push an entry to the undo stack.
void SimplexBase::markEmpty() {
  // If the set is already empty, then we shouldn't add another UnmarkEmpty log
  // entry, since in that case the Simplex will be erroneously marked as
  // non-empty when rolling back past this point.
  if (empty)
    return;
  undoLog.push_back(UndoLogEntry::UnmarkEmpty);
  empty = true;
}

/// Add an inequality to the tableau. If coeffs is c_0, c_1, ... c_n, where n
/// is the current number of variables, then the corresponding inequality is
/// c_n + c_0*x_0 + c_1*x_1 + ... + c_{n-1}*x_{n-1} >= 0.
///
/// We add the inequality and mark it as restricted. We then try to make its
/// sample value non-negative. If this is not possible, the tableau has become
/// empty and we mark it as such.
void Simplex::addInequality(ArrayRef<int64_t> coeffs) {
  unsigned conIndex = addRow(coeffs, /*makeRestricted=*/true);
  LogicalResult result = restoreRow(con[conIndex]);
  if (failed(result))
    markEmpty();
}

/// Add an equality to the tableau. If coeffs is c_0, c_1, ... c_n, where n
/// is the current number of variables, then the corresponding equality is
/// c_n + c_0*x_0 + c_1*x_1 + ... + c_{n-1}*x_{n-1} == 0.
///
/// We simply add two opposing inequalities, which force the expression to
/// be zero.
void SimplexBase::addEquality(ArrayRef<int64_t> coeffs) {
  addInequality(coeffs);
  SmallVector<int64_t, 8> negatedCoeffs;
  for (int64_t coeff : coeffs)
    negatedCoeffs.emplace_back(-coeff);
  addInequality(negatedCoeffs);
}

unsigned SimplexBase::getNumVariables() const { return var.size(); }
unsigned SimplexBase::getNumConstraints() const { return con.size(); }

/// Return a snapshot of the current state. This is just the current size of the
/// undo log.
unsigned SimplexBase::getSnapshot() const { return undoLog.size(); }

unsigned SimplexBase::getSnapshotBasis() {
  SmallVector<int, 8> basis;
  for (int index : colUnknown) {
    if (index != nullIndex)
      basis.push_back(index);
  }
  savedBases.push_back(std::move(basis));

  undoLog.emplace_back(UndoLogEntry::RestoreBasis);
  return undoLog.size() - 1;
}

void SimplexBase::removeLastConstraintRowOrientation() {
  assert(con.back().orientation == Orientation::Row);

  // Move this unknown to the last row and remove the last row from the
  // tableau.
  swapRows(con.back().pos, nRow - 1);
  // It is not strictly necessary to shrink the tableau, but for now we
  // maintain the invariant that the tableau has exactly nRow rows.
  tableau.resizeVertically(nRow - 1);
  nRow--;
  rowUnknown.pop_back();
  con.pop_back();
}

// This doesn't find a pivot row only if the column has zero
// coefficients for every row.
//
// If the unknown is a constraint, this can't happen, since it was added
// initially as a row. Such a row could never have been pivoted to a column. So
// a pivot row will always be found if we have a constraint.
//
// If we have a variable, then the column has zero coefficients for every row
// iff no constraints have been added with a non-zero coefficient for this row.
Optional<unsigned> SimplexBase::findAnyPivotRow(unsigned col) {
  for (unsigned row = nRedundant; row < nRow; ++row)
    if (tableau(row, col) != 0)
      return row;
  return {};
}

// It's not valid to remove the constraint by deleting the column since this
// would result in an invalid basis.
void Simplex::undoLastConstraint() {
  if (con.back().orientation == Orientation::Column) {
    // We try to find any pivot row for this column that preserves tableau
    // consistency (except possibly the column itself, which is going to be
    // deallocated anyway).
    //
    // If no pivot row is found in either direction, then the unknown is
    // unbounded in both directions and we are free to perform any pivot at
    // all. To do this, we just need to find any row with a non-zero
    // coefficient for the column. findAnyPivotRow will always be able to
    // find such a row for a constraint.
    unsigned column = con.back().pos;
    if (Optional<unsigned> maybeRow = findPivotRow({}, Direction::Up, column)) {
      pivot(*maybeRow, column);
    } else if (Optional<unsigned> maybeRow =
                   findPivotRow({}, Direction::Down, column)) {
      pivot(*maybeRow, column);
    } else {
      Optional<unsigned> row = findAnyPivotRow(column);
      assert(row.hasValue() && "Pivot should always exist for a constraint!");
      pivot(*row, column);
    }
  }
  removeLastConstraintRowOrientation();
}

// It's not valid to remove the constraint by deleting the column since this
// would result in an invalid basis.
void LexSimplex::undoLastConstraint() {
  if (con.back().orientation == Orientation::Column) {
    // When removing the last constraint during a rollback, we just need to find
    // any pivot at all, i.e., any row with non-zero coefficient for the
    // column, because when rolling back a lexicographic simplex, we always
    // end by restoring the exact basis that was present at the time of the
    // snapshot, so what pivots we perform while undoing doesn't matter as
    // long as we get the unknown to row orientation and remove it.
    unsigned column = con.back().pos;
    Optional<unsigned> row = findAnyPivotRow(column);
    assert(row.hasValue() && "Pivot should always exist for a constraint!");
    pivot(*row, column);
  }
  removeLastConstraintRowOrientation();
}

void SimplexBase::undo(UndoLogEntry entry) {
  if (entry == UndoLogEntry::RemoveLastConstraint) {
    // Simplex and LexSimplex handle this differently, so we call out to a
    // virtual function to handle this.
    undoLastConstraint();
  } else if (entry == UndoLogEntry::RemoveLastVariable) {
    // Whenever we are rolling back the addition of a variable, it is guaranteed
    // that the variable will be in column position.
    //
    // We can see this as follows: any constraint that depends on this variable
    // was added after this variable was added, so the addition of such
    // constraints should already have been rolled back by the time we get to
    // rolling back the addition of the variable. Therefore, no constraint
    // currently has a component along the variable, so the variable itself must
    // be part of the basis.
    assert(var.back().orientation == Orientation::Column &&
           "Variable to be removed must be in column orientation!");

    // Move this variable to the last column and remove the column from the
    // tableau.
    swapColumns(var.back().pos, nCol - 1);
    tableau.resizeHorizontally(nCol - 1);
    var.pop_back();
    colUnknown.pop_back();
    nCol--;
  } else if (entry == UndoLogEntry::UnmarkEmpty) {
    empty = false;
  } else if (entry == UndoLogEntry::UnmarkLastRedundant) {
    nRedundant--;
  } else if (entry == UndoLogEntry::RestoreBasis) {
    assert(!savedBases.empty() && "No bases saved!");

    SmallVector<int, 8> basis = std::move(savedBases.back());
    savedBases.pop_back();

    for (int index : basis) {
      Unknown &u = unknownFromIndex(index);
      if (u.orientation == Orientation::Column)
        continue;
      for (unsigned col = getNumFixedCols(); col < nCol; col++) {
        assert(colUnknown[col] != nullIndex &&
               "Column should not be a fixed column!");
        if (std::find(basis.begin(), basis.end(), colUnknown[col]) !=
            basis.end())
          continue;
        if (tableau(u.pos, col) == 0)
          continue;
        pivot(u.pos, col);
        break;
      }

      assert(u.orientation == Orientation::Column && "No pivot found!");
    }
  }
}

/// Rollback to the specified snapshot.
///
/// We undo all the log entries until the log size when the snapshot was taken
/// is reached.
void SimplexBase::rollback(unsigned snapshot) {
  while (undoLog.size() > snapshot) {
    undo(undoLog.back());
    undoLog.pop_back();
  }
}

void SimplexBase::appendVariable(unsigned count) {
  if (count == 0)
    return;
  var.reserve(var.size() + count);
  colUnknown.reserve(colUnknown.size() + count);
  for (unsigned i = 0; i < count; ++i) {
    nCol++;
    var.emplace_back(Orientation::Column, /*restricted=*/false,
                     /*pos=*/nCol - 1);
    colUnknown.push_back(var.size() - 1);
  }
  tableau.resizeHorizontally(nCol);
  undoLog.insert(undoLog.end(), count, UndoLogEntry::RemoveLastVariable);
}

/// Add all the constraints from the given IntegerPolyhedron.
void SimplexBase::intersectIntegerPolyhedron(const IntegerPolyhedron &poly) {
  assert(poly.getNumIds() == getNumVariables() &&
         "IntegerPolyhedron must have same dimensionality as simplex");
  for (unsigned i = 0, e = poly.getNumInequalities(); i < e; ++i)
    addInequality(poly.getInequality(i));
  for (unsigned i = 0, e = poly.getNumEqualities(); i < e; ++i)
    addEquality(poly.getEquality(i));
}

MaybeOptimum<Fraction> Simplex::computeRowOptimum(Direction direction,
                                                  unsigned row) {
  // Keep trying to find a pivot for the row in the specified direction.
  while (Optional<Pivot> maybePivot = findPivot(row, direction)) {
    // If findPivot returns a pivot involving the row itself, then the optimum
    // is unbounded, so we return None.
    if (maybePivot->row == row)
      return OptimumKind::Unbounded;
    pivot(*maybePivot);
  }

  // The row has reached its optimal sample value, which we return.
  // The sample value is the entry in the constant column divided by the common
  // denominator for this row.
  return Fraction(tableau(row, 1), tableau(row, 0));
}

/// Compute the optimum of the specified expression in the specified direction,
/// or None if it is unbounded.
MaybeOptimum<Fraction> Simplex::computeOptimum(Direction direction,
                                               ArrayRef<int64_t> coeffs) {
  if (empty)
    return OptimumKind::Empty;
  unsigned snapshot = getSnapshot();
  unsigned conIndex = addRow(coeffs);
  unsigned row = con[conIndex].pos;
  MaybeOptimum<Fraction> optimum = computeRowOptimum(direction, row);
  rollback(snapshot);
  return optimum;
}

MaybeOptimum<Fraction> Simplex::computeOptimum(Direction direction,
                                               Unknown &u) {
  if (empty)
    return OptimumKind::Empty;
  if (u.orientation == Orientation::Column) {
    unsigned column = u.pos;
    Optional<unsigned> pivotRow = findPivotRow({}, direction, column);
    // If no pivot is returned, the constraint is unbounded in the specified
    // direction.
    if (!pivotRow)
      return OptimumKind::Unbounded;
    pivot(*pivotRow, column);
  }

  unsigned row = u.pos;
  MaybeOptimum<Fraction> optimum = computeRowOptimum(direction, row);
  if (u.restricted && direction == Direction::Down &&
      (optimum.isUnbounded() || *optimum < Fraction(0, 1))) {
    if (failed(restoreRow(u)))
      llvm_unreachable("Could not restore row!");
  }
  return optimum;
}

bool Simplex::isBoundedAlongConstraint(unsigned constraintIndex) {
  assert(!empty && "It is not meaningful to ask whether a direction is bounded "
                   "in an empty set.");
  // The constraint's perpendicular is already bounded below, since it is a
  // constraint. If it is also bounded above, we can return true.
  return computeOptimum(Direction::Up, con[constraintIndex]).isBounded();
}

/// Redundant constraints are those that are in row orientation and lie in
/// rows 0 to nRedundant - 1.
bool Simplex::isMarkedRedundant(unsigned constraintIndex) const {
  const Unknown &u = con[constraintIndex];
  return u.orientation == Orientation::Row && u.pos < nRedundant;
}

/// Mark the specified row redundant.
///
/// This is done by moving the unknown to the end of the block of redundant
/// rows (namely, to row nRedundant) and incrementing nRedundant to
/// accomodate the new redundant row.
void Simplex::markRowRedundant(Unknown &u) {
  assert(u.orientation == Orientation::Row &&
         "Unknown should be in row position!");
  assert(u.pos >= nRedundant && "Unknown is already marked redundant!");
  swapRows(u.pos, nRedundant);
  ++nRedundant;
  undoLog.emplace_back(UndoLogEntry::UnmarkLastRedundant);
}

/// Find a subset of constraints that is redundant and mark them redundant.
void Simplex::detectRedundant() {
  // It is not meaningful to talk about redundancy for empty sets.
  if (empty)
    return;

  // Iterate through the constraints and check for each one if it can attain
  // negative sample values. If it can, it's not redundant. Otherwise, it is.
  // We mark redundant constraints redundant.
  //
  // Constraints that get marked redundant in one iteration are not respected
  // when checking constraints in later iterations. This prevents, for example,
  // two identical constraints both being marked redundant since each is
  // redundant given the other one. In this example, only the first of the
  // constraints that is processed will get marked redundant, as it should be.
  for (Unknown &u : con) {
    if (u.orientation == Orientation::Column) {
      unsigned column = u.pos;
      Optional<unsigned> pivotRow = findPivotRow({}, Direction::Down, column);
      // If no downward pivot is returned, the constraint is unbounded below
      // and hence not redundant.
      if (!pivotRow)
        continue;
      pivot(*pivotRow, column);
    }

    unsigned row = u.pos;
    MaybeOptimum<Fraction> minimum = computeRowOptimum(Direction::Down, row);
    if (minimum.isUnbounded() || *minimum < Fraction(0, 1)) {
      // Constraint is unbounded below or can attain negative sample values and
      // hence is not redundant.
      if (failed(restoreRow(u)))
        llvm_unreachable("Could not restore non-redundant row!");
      continue;
    }

    markRowRedundant(u);
  }
}

bool Simplex::isUnbounded() {
  if (empty)
    return false;

  SmallVector<int64_t, 8> dir(var.size() + 1);
  for (unsigned i = 0; i < var.size(); ++i) {
    dir[i] = 1;

    if (computeOptimum(Direction::Up, dir).isUnbounded())
      return true;

    if (computeOptimum(Direction::Down, dir).isUnbounded())
      return true;

    dir[i] = 0;
  }
  return false;
}

/// Make a tableau to represent a pair of points in the original tableau.
///
/// The product constraints and variables are stored as: first A's, then B's.
///
/// The product tableau has row layout:
///   A's redundant rows, B's redundant rows, A's other rows, B's other rows.
///
/// It has column layout:
///   denominator, constant, A's columns, B's columns.
Simplex Simplex::makeProduct(const Simplex &a, const Simplex &b) {
  unsigned numVar = a.getNumVariables() + b.getNumVariables();
  unsigned numCon = a.getNumConstraints() + b.getNumConstraints();
  Simplex result(numVar);

  result.tableau.resizeVertically(numCon);
  result.empty = a.empty || b.empty;

  auto concat = [](ArrayRef<Unknown> v, ArrayRef<Unknown> w) {
    SmallVector<Unknown, 8> result;
    result.reserve(v.size() + w.size());
    result.insert(result.end(), v.begin(), v.end());
    result.insert(result.end(), w.begin(), w.end());
    return result;
  };
  result.con = concat(a.con, b.con);
  result.var = concat(a.var, b.var);

  auto indexFromBIndex = [&](int index) {
    return index >= 0 ? a.getNumVariables() + index
                      : ~(a.getNumConstraints() + ~index);
  };

  result.colUnknown.assign(2, nullIndex);
  for (unsigned i = 2; i < a.nCol; ++i) {
    result.colUnknown.push_back(a.colUnknown[i]);
    result.unknownFromIndex(result.colUnknown.back()).pos =
        result.colUnknown.size() - 1;
  }
  for (unsigned i = 2; i < b.nCol; ++i) {
    result.colUnknown.push_back(indexFromBIndex(b.colUnknown[i]));
    result.unknownFromIndex(result.colUnknown.back()).pos =
        result.colUnknown.size() - 1;
  }

  auto appendRowFromA = [&](unsigned row) {
    for (unsigned col = 0; col < a.nCol; ++col)
      result.tableau(result.nRow, col) = a.tableau(row, col);
    result.rowUnknown.push_back(a.rowUnknown[row]);
    result.unknownFromIndex(result.rowUnknown.back()).pos =
        result.rowUnknown.size() - 1;
    result.nRow++;
  };

  // Also fixes the corresponding entry in rowUnknown and var/con (as the case
  // may be).
  auto appendRowFromB = [&](unsigned row) {
    result.tableau(result.nRow, 0) = b.tableau(row, 0);
    result.tableau(result.nRow, 1) = b.tableau(row, 1);

    unsigned offset = a.nCol - 2;
    for (unsigned col = 2; col < b.nCol; ++col)
      result.tableau(result.nRow, offset + col) = b.tableau(row, col);
    result.rowUnknown.push_back(indexFromBIndex(b.rowUnknown[row]));
    result.unknownFromIndex(result.rowUnknown.back()).pos =
        result.rowUnknown.size() - 1;
    result.nRow++;
  };

  result.nRedundant = a.nRedundant + b.nRedundant;
  for (unsigned row = 0; row < a.nRedundant; ++row)
    appendRowFromA(row);
  for (unsigned row = 0; row < b.nRedundant; ++row)
    appendRowFromB(row);
  for (unsigned row = a.nRedundant; row < a.nRow; ++row)
    appendRowFromA(row);
  for (unsigned row = b.nRedundant; row < b.nRow; ++row)
    appendRowFromB(row);

  return result;
}

Optional<SmallVector<Fraction, 8>> Simplex::getRationalSample() const {
  if (empty)
    return {};

  SmallVector<Fraction, 8> sample;
  sample.reserve(var.size());
  // Push the sample value for each variable into the vector.
  for (const Unknown &u : var) {
    if (u.orientation == Orientation::Column) {
      // If the variable is in column position, its sample value is zero.
      sample.emplace_back(0, 1);
    } else {
      // If the variable is in row position, its sample value is the
      // entry in the constant column divided by the denominator.
      int64_t denom = tableau(u.pos, 0);
      sample.emplace_back(tableau(u.pos, 1), denom);
    }
  }
  return sample;
}

MaybeOptimum<SmallVector<Fraction, 8>> LexSimplex::getRationalSample() const {
  if (empty)
    return OptimumKind::Empty;

  SmallVector<Fraction, 8> sample;
  sample.reserve(var.size());
  // Push the sample value for each variable into the vector.
  for (const Unknown &u : var) {
    // When the big M parameter is being used, each variable x is represented
    // as M + x, so its sample value is finite if and only if it is of the
    // form 1*M + c. If the coefficient of M is not one then the sample value
    // is infinite, and we return an empty optional.

    if (u.orientation == Orientation::Column) {
      // If the variable is in column position, the sample value of M + x is
      // zero, so x = -M which is unbounded.
      return OptimumKind::Unbounded;
    }

    // If the variable is in row position, its sample value is the
    // entry in the constant column divided by the denominator.
    int64_t denom = tableau(u.pos, 0);
    if (usingBigM)
      if (tableau(u.pos, 2) != denom)
        return OptimumKind::Unbounded;
    sample.emplace_back(tableau(u.pos, 1), denom);
  }
  return sample;
}

Optional<SmallVector<int64_t, 8>> Simplex::getSamplePointIfIntegral() const {
  // If the tableau is empty, no sample point exists.
  if (empty)
    return {};

  // The value will always exist since the Simplex is non-empty.
  SmallVector<Fraction, 8> rationalSample = *getRationalSample();
  SmallVector<int64_t, 8> integerSample;
  integerSample.reserve(var.size());
  for (const Fraction &coord : rationalSample) {
    // If the sample is non-integral, return None.
    if (coord.num % coord.den != 0)
      return {};
    integerSample.push_back(coord.num / coord.den);
  }
  return integerSample;
}

/// Given a simplex for a polytope, construct a new simplex whose variables are
/// identified with a pair of points (x, y) in the original polytope. Supports
/// some operations needed for generalized basis reduction. In what follows,
/// dotProduct(x, y) = x_1 * y_1 + x_2 * y_2 + ... x_n * y_n where n is the
/// dimension of the original polytope.
///
/// This supports adding equality constraints dotProduct(dir, x - y) == 0. It
/// also supports rolling back this addition, by maintaining a snapshot stack
/// that contains a snapshot of the Simplex's state for each equality, just
/// before that equality was added.
class GBRSimplex {
  using Orientation = Simplex::Orientation;

public:
  GBRSimplex(const Simplex &originalSimplex)
      : simplex(Simplex::makeProduct(originalSimplex, originalSimplex)),
        simplexConstraintOffset(simplex.getNumConstraints()) {}

  /// Add an equality dotProduct(dir, x - y) == 0.
  /// First pushes a snapshot for the current simplex state to the stack so
  /// that this can be rolled back later.
  void addEqualityForDirection(ArrayRef<int64_t> dir) {
    assert(
        std::any_of(dir.begin(), dir.end(), [](int64_t x) { return x != 0; }) &&
        "Direction passed is the zero vector!");
    snapshotStack.push_back(simplex.getSnapshot());
    simplex.addEquality(getCoeffsForDirection(dir));
  }
  /// Compute max(dotProduct(dir, x - y)).
  Fraction computeWidth(ArrayRef<int64_t> dir) {
    MaybeOptimum<Fraction> maybeWidth =
        simplex.computeOptimum(Direction::Up, getCoeffsForDirection(dir));
    assert(maybeWidth.isBounded() && "Width should be bounded!");
    return *maybeWidth;
  }

  /// Compute max(dotProduct(dir, x - y)) and save the dual variables for only
  /// the direction equalities to `dual`.
  Fraction computeWidthAndDuals(ArrayRef<int64_t> dir,
                                SmallVectorImpl<int64_t> &dual,
                                int64_t &dualDenom) {
    // We can't just call into computeWidth or computeOptimum since we need to
    // access the state of the tableau after computing the optimum, and these
    // functions rollback the insertion of the objective function into the
    // tableau before returning. We instead add a row for the objective function
    // ourselves, call into computeOptimum, compute the duals from the tableau
    // state, and finally rollback the addition of the row before returning.
    unsigned snap = simplex.getSnapshot();
    unsigned conIndex = simplex.addRow(getCoeffsForDirection(dir));
    unsigned row = simplex.con[conIndex].pos;
    MaybeOptimum<Fraction> maybeWidth =
        simplex.computeRowOptimum(Simplex::Direction::Up, row);
    assert(maybeWidth.isBounded() && "Width should be bounded!");
    dualDenom = simplex.tableau(row, 0);
    dual.clear();

    // The increment is i += 2 because equalities are added as two inequalities,
    // one positive and one negative. Each iteration processes one equality.
    for (unsigned i = simplexConstraintOffset; i < conIndex; i += 2) {
      // The dual variable for an inequality in column orientation is the
      // negative of its coefficient at the objective row. If the inequality is
      // in row orientation, the corresponding dual variable is zero.
      //
      // We want the dual for the original equality, which corresponds to two
      // inequalities: a positive inequality, which has the same coefficients as
      // the equality, and a negative equality, which has negated coefficients.
      //
      // Note that at most one of these inequalities can be in column
      // orientation because the column unknowns should form a basis and hence
      // must be linearly independent. If the positive inequality is in column
      // position, its dual is the dual corresponding to the equality. If the
      // negative inequality is in column position, the negation of its dual is
      // the dual corresponding to the equality. If neither is in column
      // position, then that means that this equality is redundant, and its dual
      // is zero.
      //
      // Note that it is NOT valid to perform pivots during the computation of
      // the duals. This entire dual computation must be performed on the same
      // tableau configuration.
      assert(!(simplex.con[i].orientation == Orientation::Column &&
               simplex.con[i + 1].orientation == Orientation::Column) &&
             "Both inequalities for the equality cannot be in column "
             "orientation!");
      if (simplex.con[i].orientation == Orientation::Column)
        dual.push_back(-simplex.tableau(row, simplex.con[i].pos));
      else if (simplex.con[i + 1].orientation == Orientation::Column)
        dual.push_back(simplex.tableau(row, simplex.con[i + 1].pos));
      else
        dual.push_back(0);
    }
    simplex.rollback(snap);
    return *maybeWidth;
  }

  /// Remove the last equality that was added through addEqualityForDirection.
  ///
  /// We do this by rolling back to the snapshot at the top of the stack, which
  /// should be a snapshot taken just before the last equality was added.
  void removeLastEquality() {
    assert(!snapshotStack.empty() && "Snapshot stack is empty!");
    simplex.rollback(snapshotStack.back());
    snapshotStack.pop_back();
  }

private:
  /// Returns coefficients of the expression 'dot_product(dir, x - y)',
  /// i.e.,   dir_1 * x_1 + dir_2 * x_2 + ... + dir_n * x_n
  ///       - dir_1 * y_1 - dir_2 * y_2 - ... - dir_n * y_n,
  /// where n is the dimension of the original polytope.
  SmallVector<int64_t, 8> getCoeffsForDirection(ArrayRef<int64_t> dir) {
    assert(2 * dir.size() == simplex.getNumVariables() &&
           "Direction vector has wrong dimensionality");
    SmallVector<int64_t, 8> coeffs(dir.begin(), dir.end());
    coeffs.reserve(2 * dir.size());
    for (int64_t coeff : dir)
      coeffs.push_back(-coeff);
    coeffs.push_back(0); // constant term
    return coeffs;
  }

  Simplex simplex;
  /// The first index of the equality constraints, the index immediately after
  /// the last constraint in the initial product simplex.
  unsigned simplexConstraintOffset;
  /// A stack of snapshots, used for rolling back.
  SmallVector<unsigned, 8> snapshotStack;
};

// Return a + scale*b;
static SmallVector<int64_t, 8> scaleAndAdd(ArrayRef<int64_t> a, int64_t scale,
                                           ArrayRef<int64_t> b) {
  assert(a.size() == b.size());
  SmallVector<int64_t, 8> res;
  res.reserve(a.size());
  for (unsigned i = 0, e = a.size(); i < e; ++i)
    res.push_back(a[i] + scale * b[i]);
  return res;
}

/// Reduce the basis to try and find a direction in which the polytope is
/// "thin". This only works for bounded polytopes.
///
/// This is an implementation of the algorithm described in the paper
/// "An Implementation of Generalized Basis Reduction for Integer Programming"
/// by W. Cook, T. Rutherford, H. E. Scarf, D. Shallcross.
///
/// Let b_{level}, b_{level + 1}, ... b_n be the current basis.
/// Let width_i(v) = max <v, x - y> where x and y are points in the original
/// polytope such that <b_j, x - y> = 0 is satisfied for all level <= j < i.
///
/// In every iteration, we first replace b_{i+1} with b_{i+1} + u*b_i, where u
/// is the integer such that width_i(b_{i+1} + u*b_i) is minimized. Let dual_i
/// be the dual variable associated with the constraint <b_i, x - y> = 0 when
/// computing width_{i+1}(b_{i+1}). It can be shown that dual_i is the
/// minimizing value of u, if it were allowed to be fractional. Due to
/// convexity, the minimizing integer value is either floor(dual_i) or
/// ceil(dual_i), so we just need to check which of these gives a lower
/// width_{i+1} value. If dual_i turned out to be an integer, then u = dual_i.
///
/// Now if width_i(b_{i+1}) < 0.75 * width_i(b_i), we swap b_i and (the new)
/// b_{i + 1} and decrement i (unless i = level, in which case we stay at the
/// same i). Otherwise, we increment i.
///
/// We keep f values and duals cached and invalidate them when necessary.
/// Whenever possible, we use them instead of recomputing them. We implement the
/// algorithm as follows.
///
/// In an iteration at i we need to compute:
///   a) width_i(b_{i + 1})
///   b) width_i(b_i)
///   c) the integer u that minimizes width_i(b_{i + 1} + u*b_i)
///
/// If width_i(b_i) is not already cached, we compute it.
///
/// If the duals are not already cached, we compute width_{i+1}(b_{i+1}) and
/// store the duals from this computation.
///
/// We call updateBasisWithUAndGetFCandidate, which finds the minimizing value
/// of u as explained before, caches the duals from this computation, sets
/// b_{i+1} to b_{i+1} + u*b_i, and returns the new value of width_i(b_{i+1}).
///
/// Now if width_i(b_{i+1}) < 0.75 * width_i(b_i), we swap b_i and b_{i+1} and
/// decrement i, resulting in the basis
/// ... b_{i - 1}, b_{i + 1} + u*b_i, b_i, b_{i+2}, ...
/// with corresponding f values
/// ... width_{i-1}(b_{i-1}), width_i(b_{i+1} + u*b_i), width_{i+1}(b_i), ...
/// The values up to i - 1 remain unchanged. We have just gotten the middle
/// value from updateBasisWithUAndGetFCandidate, so we can update that in the
/// cache. The value at width_{i+1}(b_i) is unknown, so we evict this value from
/// the cache. The iteration after decrementing needs exactly the duals from the
/// computation of width_i(b_{i + 1} + u*b_i), so we keep these in the cache.
///
/// When incrementing i, no cached f values get invalidated. However, the cached
/// duals do get invalidated as the duals for the higher levels are different.
void Simplex::reduceBasis(Matrix &basis, unsigned level) {
  const Fraction epsilon(3, 4);

  if (level == basis.getNumRows() - 1)
    return;

  GBRSimplex gbrSimplex(*this);
  SmallVector<Fraction, 8> width;
  SmallVector<int64_t, 8> dual;
  int64_t dualDenom;

  // Finds the value of u that minimizes width_i(b_{i+1} + u*b_i), caches the
  // duals from this computation, sets b_{i+1} to b_{i+1} + u*b_i, and returns
  // the new value of width_i(b_{i+1}).
  //
  // If dual_i is not an integer, the minimizing value must be either
  // floor(dual_i) or ceil(dual_i). We compute the expression for both and
  // choose the minimizing value.
  //
  // If dual_i is an integer, we don't need to perform these computations. We
  // know that in this case,
  //   a) u = dual_i.
  //   b) one can show that dual_j for j < i are the same duals we would have
  //      gotten from computing width_i(b_{i + 1} + u*b_i), so the correct duals
  //      are the ones already in the cache.
  //   c) width_i(b_{i+1} + u*b_i) = min_{alpha} width_i(b_{i+1} + alpha * b_i),
  //   which
  //      one can show is equal to width_{i+1}(b_{i+1}). The latter value must
  //      be in the cache, so we get it from there and return it.
  auto updateBasisWithUAndGetFCandidate = [&](unsigned i) -> Fraction {
    assert(i < level + dual.size() && "dual_i is not known!");

    int64_t u = floorDiv(dual[i - level], dualDenom);
    basis.addToRow(i, i + 1, u);
    if (dual[i - level] % dualDenom != 0) {
      SmallVector<int64_t, 8> candidateDual[2];
      int64_t candidateDualDenom[2];
      Fraction widthI[2];

      // Initially u is floor(dual) and basis reflects this.
      widthI[0] = gbrSimplex.computeWidthAndDuals(
          basis.getRow(i + 1), candidateDual[0], candidateDualDenom[0]);

      // Now try ceil(dual), i.e. floor(dual) + 1.
      ++u;
      basis.addToRow(i, i + 1, 1);
      widthI[1] = gbrSimplex.computeWidthAndDuals(
          basis.getRow(i + 1), candidateDual[1], candidateDualDenom[1]);

      unsigned j = widthI[0] < widthI[1] ? 0 : 1;
      if (j == 0)
        // Subtract 1 to go from u = ceil(dual) back to floor(dual).
        basis.addToRow(i, i + 1, -1);

      // width_i(b{i+1} + u*b_i) should be minimized at our value of u.
      // We assert that this holds by checking that the values of width_i at
      // u - 1 and u + 1 are greater than or equal to the value at u. If the
      // width is lesser at either of the adjacent values, then our computed
      // value of u is clearly not the minimizer. Otherwise by convexity the
      // computed value of u is really the minimizer.

      // Check the value at u - 1.
      assert(gbrSimplex.computeWidth(scaleAndAdd(
                 basis.getRow(i + 1), -1, basis.getRow(i))) >= widthI[j] &&
             "Computed u value does not minimize the width!");
      // Check the value at u + 1.
      assert(gbrSimplex.computeWidth(scaleAndAdd(
                 basis.getRow(i + 1), +1, basis.getRow(i))) >= widthI[j] &&
             "Computed u value does not minimize the width!");

      dual = std::move(candidateDual[j]);
      dualDenom = candidateDualDenom[j];
      return widthI[j];
    }

    assert(i + 1 - level < width.size() && "width_{i+1} wasn't saved");
    // f_i(b_{i+1} + dual*b_i) == width_{i+1}(b_{i+1}) when `dual` minimizes the
    // LHS. (note: the basis has already been updated, so b_{i+1} + dual*b_i in
    // the above expression is equal to basis.getRow(i+1) below.)
    assert(gbrSimplex.computeWidth(basis.getRow(i + 1)) ==
           width[i + 1 - level]);
    return width[i + 1 - level];
  };

  // In the ith iteration of the loop, gbrSimplex has constraints for directions
  // from `level` to i - 1.
  unsigned i = level;
  while (i < basis.getNumRows() - 1) {
    if (i >= level + width.size()) {
      // We don't even know the value of f_i(b_i), so let's find that first.
      // We have to do this first since later we assume that width already
      // contains values up to and including i.

      assert((i == 0 || i - 1 < level + width.size()) &&
             "We are at level i but we don't know the value of width_{i-1}");

      // We don't actually use these duals at all, but it doesn't matter
      // because this case should only occur when i is level, and there are no
      // duals in that case anyway.
      assert(i == level && "This case should only occur when i == level");
      width.push_back(
          gbrSimplex.computeWidthAndDuals(basis.getRow(i), dual, dualDenom));
    }

    if (i >= level + dual.size()) {
      assert(i + 1 >= level + width.size() &&
             "We don't know dual_i but we know width_{i+1}");
      // We don't know dual for our level, so let's find it.
      gbrSimplex.addEqualityForDirection(basis.getRow(i));
      width.push_back(gbrSimplex.computeWidthAndDuals(basis.getRow(i + 1), dual,
                                                      dualDenom));
      gbrSimplex.removeLastEquality();
    }

    // This variable stores width_i(b_{i+1} + u*b_i).
    Fraction widthICandidate = updateBasisWithUAndGetFCandidate(i);
    if (widthICandidate < epsilon * width[i - level]) {
      basis.swapRows(i, i + 1);
      width[i - level] = widthICandidate;
      // The values of width_{i+1}(b_{i+1}) and higher may change after the
      // swap, so we remove the cached values here.
      width.resize(i - level + 1);
      if (i == level) {
        dual.clear();
        continue;
      }

      gbrSimplex.removeLastEquality();
      i--;
      continue;
    }

    // Invalidate duals since the higher level needs to recompute its own duals.
    dual.clear();
    gbrSimplex.addEqualityForDirection(basis.getRow(i));
    i++;
  }
}

/// Search for an integer sample point using a branch and bound algorithm.
///
/// Each row in the basis matrix is a vector, and the set of basis vectors
/// should span the space. Initially this is the identity matrix,
/// i.e., the basis vectors are just the variables.
///
/// In every level, a value is assigned to the level-th basis vector, as
/// follows. Compute the minimum and maximum rational values of this direction.
/// If only one integer point lies in this range, constrain the variable to
/// have this value and recurse to the next variable.
///
/// If the range has multiple values, perform generalized basis reduction via
/// reduceBasis and then compute the bounds again. Now we try constraining
/// this direction in the first value in this range and "recurse" to the next
/// level. If we fail to find a sample, we try assigning the direction the next
/// value in this range, and so on.
///
/// If no integer sample is found from any of the assignments, or if the range
/// contains no integer value, then of course the polytope is empty for the
/// current assignment of the values in previous levels, so we return to
/// the previous level.
///
/// If we reach the last level where all the variables have been assigned values
/// already, then we simply return the current sample point if it is integral,
/// and go back to the previous level otherwise.
///
/// To avoid potentially arbitrarily large recursion depths leading to stack
/// overflows, this algorithm is implemented iteratively.
Optional<SmallVector<int64_t, 8>> Simplex::findIntegerSample() {
  if (empty)
    return {};

  unsigned nDims = var.size();
  Matrix basis = Matrix::identity(nDims);

  unsigned level = 0;
  // The snapshot just before constraining a direction to a value at each level.
  SmallVector<unsigned, 8> snapshotStack;
  // The maximum value in the range of the direction for each level.
  SmallVector<int64_t, 8> upperBoundStack;
  // The next value to try constraining the basis vector to at each level.
  SmallVector<int64_t, 8> nextValueStack;

  snapshotStack.reserve(basis.getNumRows());
  upperBoundStack.reserve(basis.getNumRows());
  nextValueStack.reserve(basis.getNumRows());
  while (level != -1u) {
    if (level == basis.getNumRows()) {
      // We've assigned values to all variables. Return if we have a sample,
      // or go back up to the previous level otherwise.
      if (auto maybeSample = getSamplePointIfIntegral())
        return maybeSample;
      level--;
      continue;
    }

    if (level >= upperBoundStack.size()) {
      // We haven't populated the stack values for this level yet, so we have
      // just come down a level ("recursed"). Find the lower and upper bounds.
      // If there is more than one integer point in the range, perform
      // generalized basis reduction.
      SmallVector<int64_t, 8> basisCoeffs =
          llvm::to_vector<8>(basis.getRow(level));
      basisCoeffs.push_back(0);

      MaybeOptimum<int64_t> minRoundedUp, maxRoundedDown;
      std::tie(minRoundedUp, maxRoundedDown) =
          computeIntegerBounds(basisCoeffs);

      // We don't have any integer values in the range.
      // Pop the stack and return up a level.
      if (minRoundedUp.isEmpty() || maxRoundedDown.isEmpty()) {
        assert((minRoundedUp.isEmpty() && maxRoundedDown.isEmpty()) &&
               "If one bound is empty, both should be.");
        snapshotStack.pop_back();
        nextValueStack.pop_back();
        upperBoundStack.pop_back();
        level--;
        continue;
      }

      // We already checked the empty case above.
      assert((minRoundedUp.isBounded() && maxRoundedDown.isBounded()) &&
             "Polyhedron should be bounded!");

      // Heuristic: if the sample point is integral at this point, just return
      // it.
      if (auto maybeSample = getSamplePointIfIntegral())
        return *maybeSample;

      if (*minRoundedUp < *maxRoundedDown) {
        reduceBasis(basis, level);
        basisCoeffs = llvm::to_vector<8>(basis.getRow(level));
        basisCoeffs.push_back(0);
        std::tie(minRoundedUp, maxRoundedDown) =
            computeIntegerBounds(basisCoeffs);
      }

      snapshotStack.push_back(getSnapshot());
      // The smallest value in the range is the next value to try.
      // The values in the optionals are guaranteed to exist since we know the
      // polytope is bounded.
      nextValueStack.push_back(*minRoundedUp);
      upperBoundStack.push_back(*maxRoundedDown);
    }

    assert((snapshotStack.size() - 1 == level &&
            nextValueStack.size() - 1 == level &&
            upperBoundStack.size() - 1 == level) &&
           "Mismatched variable stack sizes!");

    // Whether we "recursed" or "returned" from a lower level, we rollback
    // to the snapshot of the starting state at this level. (in the "recursed"
    // case this has no effect)
    rollback(snapshotStack.back());
    int64_t nextValue = nextValueStack.back();
    nextValueStack.back()++;
    if (nextValue > upperBoundStack.back()) {
      // We have exhausted the range and found no solution. Pop the stack and
      // return up a level.
      snapshotStack.pop_back();
      nextValueStack.pop_back();
      upperBoundStack.pop_back();
      level--;
      continue;
    }

    // Try the next value in the range and "recurse" into the next level.
    SmallVector<int64_t, 8> basisCoeffs(basis.getRow(level).begin(),
                                        basis.getRow(level).end());
    basisCoeffs.push_back(-nextValue);
    addEquality(basisCoeffs);
    level++;
  }

  return {};
}

/// Compute the minimum and maximum integer values the expression can take. We
/// compute each separately.
std::pair<MaybeOptimum<int64_t>, MaybeOptimum<int64_t>>
Simplex::computeIntegerBounds(ArrayRef<int64_t> coeffs) {
  MaybeOptimum<int64_t> minRoundedUp(
      computeOptimum(Simplex::Direction::Down, coeffs).map(ceil));
  MaybeOptimum<int64_t> maxRoundedDown(
      computeOptimum(Simplex::Direction::Up, coeffs).map(floor));
  return {minRoundedUp, maxRoundedDown};
}

void SimplexBase::print(raw_ostream &os) const {
  os << "rows = " << nRow << ", columns = " << nCol << "\n";
  if (empty)
    os << "Simplex marked empty!\n";
  os << "var: ";
  for (unsigned i = 0; i < var.size(); ++i) {
    if (i > 0)
      os << ", ";
    var[i].print(os);
  }
  os << "\ncon: ";
  for (unsigned i = 0; i < con.size(); ++i) {
    if (i > 0)
      os << ", ";
    con[i].print(os);
  }
  os << '\n';
  for (unsigned row = 0; row < nRow; ++row) {
    if (row > 0)
      os << ", ";
    os << "r" << row << ": " << rowUnknown[row];
  }
  os << '\n';
  os << "c0: denom, c1: const";
  for (unsigned col = 2; col < nCol; ++col)
    os << ", c" << col << ": " << colUnknown[col];
  os << '\n';
  for (unsigned row = 0; row < nRow; ++row) {
    for (unsigned col = 0; col < nCol; ++col)
      os << tableau(row, col) << '\t';
    os << '\n';
  }
  os << '\n';
}

void SimplexBase::dump() const { print(llvm::errs()); }

bool Simplex::isRationalSubsetOf(const IntegerPolyhedron &poly) {
  if (isEmpty())
    return true;

  for (unsigned i = 0, e = poly.getNumInequalities(); i < e; ++i)
    if (findIneqType(poly.getInequality(i)) != IneqType::Redundant)
      return false;

  for (unsigned i = 0, e = poly.getNumEqualities(); i < e; ++i)
    if (!isRedundantEquality(poly.getEquality(i)))
      return false;

  return true;
}

/// Returns the type of the inequality with coefficients `coeffs`.
/// Possible types are:
/// Redundant   The inequality is satisfied by all points in the polytope
/// Cut         The inequality is satisfied by some points, but not by others
/// Separate    The inequality is not satisfied by any point
///
/// Internally, this computes the minimum and the maximum the inequality with
/// coefficients `coeffs` can take. If the minimum is >= 0, the inequality holds
/// for all points in the polytope, so it is redundant.  If the minimum is <= 0
/// and the maximum is >= 0, the points in between the minimum and the
/// inequality do not satisfy it, the points in between the inequality and the
/// maximum satisfy it. Hence, it is a cut inequality. If both are < 0, no
/// points of the polytope satisfy the inequality, which means it is a separate
/// inequality.
Simplex::IneqType Simplex::findIneqType(ArrayRef<int64_t> coeffs) {
  MaybeOptimum<Fraction> minimum = computeOptimum(Direction::Down, coeffs);
  if (minimum.isBounded() && *minimum >= Fraction(0, 1)) {
    return IneqType::Redundant;
  }
  MaybeOptimum<Fraction> maximum = computeOptimum(Direction::Up, coeffs);
  if ((!minimum.isBounded() || *minimum <= Fraction(0, 1)) &&
      (!maximum.isBounded() || *maximum >= Fraction(0, 1))) {
    return IneqType::Cut;
  }
  return IneqType::Separate;
}

/// Checks whether the type of the inequality with coefficients `coeffs`
/// is Redundant.
bool Simplex::isRedundantInequality(ArrayRef<int64_t> coeffs) {
  assert(!empty &&
         "It is not meaningful to ask about redundancy in an empty set!");
  return findIneqType(coeffs) == IneqType::Redundant;
}

/// Check whether the equality given by `coeffs == 0` is redundant given
/// the existing constraints. This is redundant when `coeffs` is already
/// always zero under the existing constraints. `coeffs` is always zero
/// when the minimum and maximum value that `coeffs` can take are both zero.
bool Simplex::isRedundantEquality(ArrayRef<int64_t> coeffs) {
  assert(!empty &&
         "It is not meaningful to ask about redundancy in an empty set!");
  MaybeOptimum<Fraction> minimum = computeOptimum(Direction::Down, coeffs);
  MaybeOptimum<Fraction> maximum = computeOptimum(Direction::Up, coeffs);
  assert((!minimum.isEmpty() && !maximum.isEmpty()) &&
         "Optima should be non-empty for a non-empty set");
  return minimum.isBounded() && maximum.isBounded() &&
         *maximum == Fraction(0, 1) && *minimum == Fraction(0, 1);
}

} // namespace mlir
