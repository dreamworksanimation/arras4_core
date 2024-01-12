// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <unistd.h>
#include <sys/ioctl.h>

#include "Spreadsheet.h"

namespace {

bool gInteractive = true;

void
termGetSize(int& width, int& height)
{
    struct winsize size;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) < 0) {
        printf("Couldn't get terminal dimensions\n");
        gInteractive = false;
    }
    width = size.ws_col;
    height = size.ws_row;
}

void
termClear()
{
    if (gInteractive) {
        printf("\033[2J");
    }
}

void
termGoto(int x, int y)
{
    if (gInteractive) {
        printf("\033\133%d;%df",x+1,y+1);
    }
}

void
termClearEos()
{
    if (gInteractive) {
        printf("\033[J");
    }
}

void
termInvert(bool invert)
{
    if (gInteractive) {
        if (invert) {
            printf("\033[7m");
        } else {
            printf("\033[27m");
        }
    }
}

void termBold(bool bold)
{
    if (gInteractive) {
        if (bold) {
            printf("\033[1m");
        } else {
            printf("\033[21m");
        }
    }
}

} // end anonymous namespace

void
Spreadsheet::print(bool wrapUnformatted) {
    gInteractive = isatty(STDOUT_FILENO);
    if (mRows.size() == 0) return;
    findColumnWidths();
    int width{0}, height{0};
    if (gInteractive) {
        termGetSize(width, height);
        termClear();
        termGoto(0,0);
    }

    // print out the first row special since we want the headers to be all left justified
    int chars = 0;
    for (auto c = 0; c < mColumns; c++) {
        chars += mWidths[c]+1;
        if (gInteractive) {
            if (chars >= (width-1)) {
                printf(">");
                break;
            }
        }
        printf("%-*s", mWidths[c], mRows[0][c].c_str());
        printf(" ");
    }
    printf("\n");


    bool highlighted=false;
    int line=1;
    for (auto r = 1u; r < mRows.size(); r++) {
        if (highlighted != mRows[r].highlighted()) {
            termBold(mRows[r].highlighted());
            highlighted = mRows[r].highlighted();
        }
        if (static_cast<int>(r) == height) break;
        chars = 0;
        if (mRows[r].isUnformatted()) {
            const std::string& unformatted = mRows[r].getUnformatted();
            if (gInteractive) {
                size_t size = unformatted.length();
                size_t index = 0;
                size_t chunk = width -1;
                size_t indent = 0;
                while (1) {
                    std::string fragment = unformatted.substr(index, chunk);
                    printf("%*s%s",static_cast<int>(indent),"",fragment.c_str());

                    size -= fragment.length();
                    index += fragment.length();

                    // exit if not wrapping or done
                    if ((size == 0) || !wrapUnformatted) break;

                    // change the indenting (and shrink the line) for subsequent lines
                    chunk = width - 9;
                    indent = 8;

                    // need to stop before causing the terminal to scroll
                    if (gInteractive) {
                        if (line == (height-1)) {
                            break;
                        }
                    }
                    printf("\n");

                    // need to count the extra lines
                    line++;

                }
            } else {
                // just print the entire line when writing to a file
                printf("%s", unformatted.c_str());
            }
        } else {
            for (auto c = 0; c < mColumns; c++) {
                chars += mWidths[c]+1;
                if (gInteractive) {
                    if (chars >= (width-1)) {
                        printf(">");
                        break;
                    }
                }
                if (mAlignment[c] == ALIGN_LEFT) {
                    printf("%-*s", mWidths[c], mRows[r][c].c_str());
                } else {
                    printf("%*s", mWidths[c], mRows[r][c].c_str());
                }
                printf(" ");
            }
        }

        // need to stop before causing the terminal to scroll
        if (gInteractive) {
            if (line == (height-1)) {
                fflush(stdout);
                break;
            }
        }
        printf("\n");
        line++;
    }
    if (highlighted) termBold(false);
    termClearEos();
}

