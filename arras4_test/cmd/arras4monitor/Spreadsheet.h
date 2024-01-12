// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef _SPREADSHEET_H_INCLUDED
#define _SPREADSHEET_H_INCLUDED

#include <string>
#include <vector>

//
// A Row is either the standard size array of cells or a single unformatted
// that takes over the entire row
//
class Row {
  public:
    Row(int columns) : mHighlight(false) {
        mCells.resize(columns);
    }

    const std::string& operator [] (int index) const {
        return mCells[index];
    }
    std::string& operator [] (int index) {
        return mCells[index];
    }

    // allow a single string which takes over a row
    const std::string& operator = (const std::string& unformatted) {
        mUnformatted = unformatted;
        return mUnformatted;
    }
    bool isUnformatted() const {
        return !mUnformatted.empty();
    }
    const std::string& getUnformatted() {
        return mUnformatted;
    }

    void highlight(bool value) {
        mHighlight = value;
    }
    bool highlighted() const {
        return mHighlight;
    }

  private:
    std::string mUnformatted;
    std::vector<std::string> mCells;
    bool mHighlight;
};

class Spreadsheet {
  public:
    Spreadsheet(int rows, int columns) : mColumns(columns) {
        for (auto i = 0; i < rows; i++) {
            mRows.push_back(Row(columns));
        }
        mAlignment.resize(columns);
    }

    const Row& operator [] (int index) const {
        return mRows[index];
    }
    Row& operator [] (int index) {
        return mRows[index];
    }

    // increment the number of rows and allocate the cells
    void addRow() {
        mRows.push_back(Row(mColumns));
    }

    // find the maximum size of each column
    void findColumnWidths() {
        mWidths.clear();
        for (auto c = 0; c < mColumns; c++) {
            int width = 0;
            for (auto r = 0u; r < mRows.size(); r++ ) {
                // ignore unformatted rows for cell width
                if (mRows[r].isUnformatted()) continue;

                if (static_cast<int>(mRows[r][c].length()) > width) {
                    width = static_cast<int>( mRows[r][c].length());
            }
            mWidths.push_back(width);
        }
    }

    // print the spreadsheet to the terminal or file
    void print(bool wrapUnformatted=false);

    enum Alignment {
       ALIGN_LEFT,
       ALIGN_RIGHT};
    void setAlignment(int index, Alignment alignment) {
        mAlignment[index] = alignment;
    }

    int rows() { return static_cast<int>(mRows.size()); }
    int columns() { return mColumns; };

  private:
    std::vector<Row> mRows;
    std::vector<int> mWidths;
    std::vector<Alignment> mAlignment;
    const int mColumns;
};

#endif // _SPREADSHEET_H_INCLUDED_

