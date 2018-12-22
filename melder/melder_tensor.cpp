/* melder_tensor.cpp
 *
 * Copyright (C) 1992-2012,2018 Paul Boersma
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This code is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this work. If not, see <http://www.gnu.org/licenses/>.
 */

#include "melder.h"

static integer theTotalNumberOfArrays;

integer NUM_getTotalNumberOfArrays () { return theTotalNumberOfArrays; }

#pragma mark - Generic memory functions for vectors

byte * NUMvector_generic (integer elementSize, integer lo, integer hi, bool initializeToZero) {
	try {
		const int64 numberOfCells = hi - lo + 1;
		if (numberOfCells <= 0)
			return nullptr;   // not an error
		byte *result;
		for (;;) {   // not very infinite: 99.999 % of the time once, 0.001 % twice
			result = initializeToZero ?
				reinterpret_cast <byte *> (_Melder_calloc (numberOfCells, elementSize)) :
				reinterpret_cast <byte *> (_Melder_malloc (numberOfCells * elementSize));
			const int64 offset = (int64) lo * elementSize;
			result -= offset;
			if (result != nullptr)   // it would be quite a coincidence if this failed
				break;   // this will normally succeed at the first try
			(void) Melder_realloc_f (result + offset, 1);   // make "sure" that the second try will succeed (not *very* sure, because realloc might move memory even if it shrinks)
		}
		theTotalNumberOfArrays += 1;
		return result;
	} catch (MelderError) {
		Melder_throw (U"Vector of elements not created.");
	}
}

void NUMvector_free_generic (integer elementSize, byte *vector, integer lo) noexcept {
	if (! vector)
		return;   // not an error
	byte *cells = & vector [lo * elementSize];
	Melder_free (cells);
	theTotalNumberOfArrays -= 1;
}

byte * NUMvector_copy_generic (integer elementSize, const byte *vector, integer lo, integer hi) {
	try {
		if (! vector)
			return nullptr;
		byte *result = NUMvector_generic (elementSize, lo, hi, false);
		const byte *p_cells = & vector [lo * elementSize];
		byte *p_resultCells = & result [lo * elementSize];
		integer numberOfBytesToCopy = (hi - lo + 1) * elementSize;
		memcpy (p_resultCells, p_cells, (size_t) numberOfBytesToCopy);
		return result;
	} catch (MelderError) {
		Melder_throw (U"Vector of elements not copied.");
	}
}

void NUMvector_copyElements_generic (integer elementSize, const byte *fromVector, byte *toVector, integer lo, integer hi) {
	Melder_assert (fromVector && toVector);
	const byte *p_fromCells = & fromVector [lo * elementSize];
	byte *p_toCells   = & toVector   [lo * elementSize];
	integer numberOfBytesToCopy = (hi - lo + 1) * elementSize;
	if (hi >= lo) memcpy (p_toCells, p_fromCells, (size_t) numberOfBytesToCopy);   // BUG this assumes contiguity
}

bool NUMvector_equal_generic (integer elementSize, const byte *vector1, const byte *vector2, integer lo, integer hi) {
	Melder_assert (vector1 && vector2);
	const byte *p_cells1 = & vector1 [lo * elementSize];
	const byte *p_cells2 = & vector2 [lo * elementSize];
	integer numberOfBytesToCompare = (hi - lo + 1) * elementSize;
	return memcmp (p_cells1, p_cells2, (size_t) numberOfBytesToCompare) == 0;
}

void NUMvector_append_generic (integer elementSize, byte **v, integer lo, integer *hi) {
	try {
		byte *result;
		if (! *v) {
			result = NUMvector_generic (elementSize, lo, lo, true);
			*hi = lo;
		} else {
			integer offset = lo * elementSize;
			for (;;) {   // not very infinite: 99.999 % of the time once, 0.001 % twice
				result = reinterpret_cast <byte *> (Melder_realloc ((char *) *v + offset, (*hi - lo + 2) * elementSize));
				if ((result -= offset) != nullptr)   // it would be quite a coincidence if this failed
					break;   // this will normally succeed at the first try
				(void) Melder_realloc_f (result + offset, 1);   // make "sure" that the second try will succeed
			}
			(*hi) ++;
			memset (result + *hi * elementSize, 0, elementSize);   // initialize the new element to zeroes
		}
		*v = result;
	} catch (MelderError) {
		Melder_throw (U"Vector: element not appended.");
	}
}

void NUMvector_insert_generic (integer elementSize, byte **v, integer lo, integer *hi, integer position) {
	try {
		byte *result;
		if (! *v) {
			result = NUMvector_generic (elementSize, lo, lo, true);
			*hi = lo;
			Melder_assert (position == lo);
		} else {
			result = NUMvector_generic (elementSize, lo, *hi + 1, false);
			Melder_assert (position >= lo && position <= *hi + 1);
			NUMvector_copyElements_generic (elementSize, *v, result, lo, position - 1);
			memset (result + position * elementSize, 0, elementSize);
			NUMvector_copyElements_generic (elementSize, *v, result + elementSize, position, *hi);
			NUMvector_free_generic (elementSize, *v, lo);
			(*hi) ++;
		}
		*v = result;
	} catch (MelderError) {
		Melder_throw (U"Vector: element not inserted.");
	}
}

/*** Generic memory functions for matrices. ***/

void * NUMmatrix_generic (integer elementSize, integer row1, integer row2, integer col1, integer col2, bool initializeToZero) {
	try {
		const int64 numberOfRows = row2 - row1 + 1;
		const int64 numberOfColumns = col2 - col1 + 1;
		const int64 numberOfCells = numberOfRows * numberOfColumns;

		byte **result, **roomForRows;
		for (;;) {
			const int64 pointerSize = (int64) sizeof (byte *);
			const int64 sizeOfRoomForRows = numberOfRows * pointerSize;
			roomForRows = reinterpret_cast <byte **> (_Melder_malloc (sizeOfRoomForRows));
			result = roomForRows - row1;
			if (result != nullptr)   // it would be quite a coincidence if this failed
				break;   // this will normally succeed at the first try
			(void) Melder_realloc_f (roomForRows, 1);   // make "sure" that the second try will succeed (if this is an in-place realloc)
		}
		try {
			byte * const roomForCells = initializeToZero ?
				reinterpret_cast <byte *> (_Melder_calloc (numberOfCells, elementSize)) :
				reinterpret_cast <byte *> (_Melder_malloc (numberOfCells * elementSize));
			byte *p_cell = roomForCells - col1 * elementSize;
			const int64 rowSize = numberOfColumns * elementSize;
			for (integer irow = row1; irow <= row2; irow ++) {
				result [irow] = p_cell;
				p_cell += rowSize;
			}
		} catch (MelderError) {
			Melder_free (roomForRows);
			throw;
		}
		theTotalNumberOfArrays += 1;
		return result;
	} catch (MelderError) {
		Melder_throw (U"Matrix of elements not created.");
	}
}

byte *** NUMtensor3_generic (integer elementSize, integer pla1, integer pla2, integer row1, integer row2, integer col1, integer col2, bool initializeToZero) {
	try {
		Melder_require (pla2 >= pla1,
			U"The requested highest plane number (", pla2, U") should be at least as great as the requested lowest plane number (", pla1, U").");
		int64 numberOfPlanes = pla2 - pla1 + 1;
		Melder_require (row2 >= row1,
			U"The requested highest row number (", row2, U") should be at least as great as the requested lowest row number (", row1, U").");
		int64 numberOfRows = row2 - row1 + 1;
		Melder_require (col2 >= col1,
			U"The requested highest column number (", col2, U") should be at least as great as the requested lowest column number (", col1, U").");
		int64 numberOfColumns = col2 - col1 + 1;
		int64 numberOfCells = numberOfPlanes * numberOfRows * numberOfColumns;

		/*
			Allocate room for the plane pointers.
		*/
		byte ***result, ***planes;
		for (;;) {
			planes = reinterpret_cast <byte ***> (_Melder_malloc (numberOfPlanes * (int64) sizeof (byte **)));   // assume that all pointers have the same size
			result = planes - pla1;
			if (result != nullptr)   // it would be quite a coincidence if this failed
				break;
			(void) Melder_realloc_f (planes, 1);   // make "sure" that the second try will succeed
		}
		/*
			Allocate room for the row pointers.
		*/
		try {
			byte **rows =
				reinterpret_cast <byte **> (_Melder_malloc (numberOfPlanes * numberOfRows * (int64) sizeof (byte *)));   // assume that all pointers have the same size
			byte **p_row = rows - row1;
			for (integer ipla = pla1; ipla <= pla2; ipla ++) {
				result [ipla] = p_row;
				p_row += numberOfRows;
			}
			/*
				Allocate room for the cells.
			*/
			try {
				byte *cells = initializeToZero ?
					reinterpret_cast <byte *> (_Melder_calloc (numberOfCells, elementSize)) :
					reinterpret_cast <byte *> (_Melder_malloc (numberOfCells * elementSize));
				byte *p_cell = cells - col1 * elementSize;
				int64 rowSize = numberOfColumns * elementSize;
				for (integer ipla = pla1; ipla <= pla2; ipla ++) {
					for (integer irow = row1; irow <= row2; irow ++) {
						result [ipla] [irow] = p_cell;
						p_cell += rowSize;
					}
				}
			} catch (MelderError) {
				Melder_free (rows);
				throw;
			}
		} catch (MelderError) {
			Melder_free (planes);
			throw;
		}
		theTotalNumberOfArrays += 1;
		return result;
	} catch (MelderError) {
		Melder_throw (U"Three-rank tensor of elements not created.");
	}
}

void NUMmatrix_free_generic (integer elementSize, byte **m, integer row1, integer col1) noexcept {
	if (! m) return;
	byte *cells = & m [row1] [col1 * elementSize];
	Melder_free (cells);
	byte **rowPointers = & m [row1];
	Melder_free (rowPointers);
	theTotalNumberOfArrays -= 1;
}

void NUMtensor3_free_generic (integer elementSize, byte ***t, integer pla1, integer row1, integer col1) noexcept {
	if (! t) return;
	byte *cells = & t [pla1] [row1] [col1 * elementSize];
	Melder_free (cells);
	byte **rowPointers = & t [pla1] [row1];
	Melder_free (rowPointers);
	byte ***planePointers = & t [pla1];
	Melder_free (planePointers);
	theTotalNumberOfArrays -= 1;
}

void * NUMmatrix_copy_generic (integer elementSize, void *m, integer row1, integer row2, integer col1, integer col2) {
	try {
		if (! m)
			return nullptr;
		char **result = reinterpret_cast <char **>
				(NUMmatrix_generic (elementSize, row1, row2, col1, col2, false));
		if (! result)
			return nullptr;
		integer columnOffset = col1 * elementSize;
		integer dataSize = (row2 - row1 + 1) * (col2 - col1 + 1) * elementSize;
		memcpy (result [row1] + columnOffset, ((char **) m) [row1] + columnOffset, (size_t) dataSize);
		return result;
	} catch (MelderError) {
		Melder_throw (U"Matrix of elements not copied.");
	}
}

bool NUMmatrix_equal_generic (integer elementSize, void *m1, void *m2, integer row1, integer row2, integer col1, integer col2) {
	Melder_assert (m1 && m2);
	integer columnOffset = col1 * elementSize;
	integer dataSize = (row2 - row1 + 1) * (col2 - col1 + 1) * elementSize;
	return memcmp (((char **) m1) [row1] + columnOffset, ((char **) m2) [row1] + columnOffset,
			(size_t) dataSize) == 0;
}

/* End of file melder_tensor.cpp */
