/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Authors:  Paul Thiessen
*
* File Description:
*      Classes to hold alignment display
*
* ===========================================================================
*/

#include <corelib/ncbistl.hpp>
#include <corelib/ncbiobj.hpp>

#include <map>
#include <deque>
#include <memory>
#include <iomanip>
#include <math.h>

#include "objects/cddalignview/cav_alndisplay.hpp"
#include "objects/cddalignview/cav_seqset.hpp"
#include "objects/cddalignview/cav_alignset.hpp"
#include "objects/cddalignview/cddalignview.h"


BEGIN_NCBI_SCOPE

const double AlignmentDisplay::SHOW_IDENTITY = 100000.0;

// HTML colors
static const int nBlockColors = 2;
static const string
    bgColor("#FFFFFF"), blockBGColors[nBlockColors] = { "#FFFFE0", "#FFE8FF" },
    rulerColor("#700777"), numColor("#229922"), featColor("#888811"),
    plainColor("#888888"), blockColor("#2233CC"), conservedColor("#FF4466");

#define LEFT_JUSTIFY resetiosflags(IOS_BASE::right) << setiosflags(IOS_BASE::left)
#define RIGHT_JUSTIFY resetiosflags(IOS_BASE::left) << setiosflags(IOS_BASE::right)

static string StrToLower(const string& str)
{
    string newStr(str);
    for (int i=0; i<newStr.size(); i++) newStr[i] = tolower(newStr[i]);
    return newStr;
}

class UnalignedInterval
{
public:
    int alnLocBefore, alnLocAfter, seqLocFrom, seqLocTo;
};

typedef list < UnalignedInterval > IntervalList;

AlignmentDisplay::AlignmentDisplay(const SequenceSet *sequenceSet, const AlignmentSet *alignmentSet) :
    status(CAV_ERROR_DISPLAY)
{
    // start with master row at top, all in lowercase - will be capitalized as things are
    // aligned to it. Also, add an IndexAlnLocToSeqLocRow for the master (only).
    if (!sequenceSet->master) {
        ERR_POST(Error << "Need to know master sequence of SequenceSet for AlignmentDisplay construction");
        return;
    }
    textRows.push_back(new TextRow(StrToLower(sequenceSet->master->sequenceString)));

    // initialize the master index row
    indexAlnLocToSeqLocRows.push_back(
        new IndexAlnLocToSeqLocRow(sequenceSet->master, sequenceSet->master->sequenceString.size()));
    int l;
    for (l=0; l<sequenceSet->master->sequenceString.size(); l++)
        indexAlnLocToSeqLocRows[0]->SetSeqLocAt(l, l);

    // loop through alignments - one new row for each alignment
    AlignmentSet::AlignmentList::const_iterator a, ae = alignmentSet->alignments.end();
    for (a=alignmentSet->alignments.begin(); a!=ae; a++) {

        // add index row for each slave that contains only Sequence* for now
        indexAlnLocToSeqLocRows.push_back(new IndexAlnLocToSeqLocRow((*a)->slave));

        // first add blank row that will be filled in with slave
        textRows.push_back(new TextRow(GetWidth()));

        // during this pass through the master, do several things:
        //  - change aligned master residues to uppercase
        //  - fill in aligned slave residues with uppercase
        //  - make a list of all unaligned slave regions, and the current display
        //      coordinates of the aligned slave residues on either side
        int prevAlignedSlaveSeqLoc = -1, prevAlignedSlaveAlnLoc = -1;
        IntervalList intervalList;
        for (int alnLoc=0; alnLoc<=GetWidth(); alnLoc++) {

            int masterSeqLoc = -1, alignedSlaveSeqLoc = -1;

            if (alnLoc < GetWidth()) {
                masterSeqLoc = indexAlnLocToSeqLocRows.front()->GetSeqLocAt(alnLoc);
                if (masterSeqLoc >= 0) {
                    alignedSlaveSeqLoc = (*a)->masterToSlave[masterSeqLoc];
                    if (alignedSlaveSeqLoc >= 0) {
                        textRows.front()->SetCharAt(alnLoc,
                            toupper(textRows.front()->GetCharAt(alnLoc)));
                        textRows.back()->SetCharAt(alnLoc,
                            toupper((*a)->slave->sequenceString[alignedSlaveSeqLoc]));
                    }
                }

            } else {
                masterSeqLoc = (*a)->master->sequenceString.size();
                alignedSlaveSeqLoc = (*a)->slave->sequenceString.size();
            }

            if (alignedSlaveSeqLoc >= 0) {
                if (alignedSlaveSeqLoc - prevAlignedSlaveSeqLoc > 1) {
                    intervalList.resize(intervalList.size() + 1);
                    intervalList.back().alnLocBefore = prevAlignedSlaveAlnLoc;
                    intervalList.back().alnLocAfter = alnLoc;
                    intervalList.back().seqLocFrom = prevAlignedSlaveSeqLoc + 1;
                    intervalList.back().seqLocTo = alignedSlaveSeqLoc - 1;
                }
                prevAlignedSlaveSeqLoc = alignedSlaveSeqLoc;
                prevAlignedSlaveAlnLoc = alnLoc;
            }
        }

        // now, put in the unaligned regions of the slave. If there is more space
        // than residues, then pad with spaces as necessary. If
        // there isn't enough space, then add gaps to all prior alignments.
        IntervalList::iterator i, ie = intervalList.end();
        int alnLocOffset = 0;   // to track # inserted columns
        for (i=intervalList.begin(); i!=ie; i++) {

            // compensate for inserted columns in display
            i->alnLocBefore += alnLocOffset;
            i->alnLocAfter += alnLocOffset;

            int
                displaySpace = i->alnLocAfter - i->alnLocBefore - 1,
                unalignedLength = i->seqLocTo - i->seqLocFrom + 1,
                extraSpace = displaySpace - unalignedLength;

            // add gaps to make space
            if (extraSpace < 0) {

                // where to insert gaps if display space is too small
                int insertPos;
                if (i->seqLocFrom == 0) {   // left tail
                    insertPos = 0;
                } else if (i->seqLocTo == (*a)->slave->sequenceString.size() - 1) { // right tail
                    insertPos = GetWidth();
                } else {    // between aligned blocks
                    insertPos = i->alnLocAfter - displaySpace / 2;
                }

                InsertGaps(-extraSpace, insertPos);
                alnLocOffset -= extraSpace;
                extraSpace = 0;
            }

            // fill in unaligned regions with lowercase, right-justifying only for left tail
            for (l=0; l<unalignedLength; l++) {
                textRows.back()->SetCharAt(
                    i->alnLocBefore + 1 + ((i->seqLocFrom == 0) ? l + extraSpace : l),
                    tolower((*a)->slave->sequenceString[i->seqLocFrom + l]));
            }
        }
    }

    ERR_POST(Info << "initial alignment display size: " << GetWidth() << " x " << GetNRows());

    // the above algorithm introduces more gaps than are strictly necessary. This
    // will "squeeze" the alignment, deleting gaps wherever possible
    Squeeze();

    // finally, redistribute unaligned residues so that they are split equally between
    // flanking aligned residues
    SplitUnaligned();

    // The Squeeze and SplitUnaligned leave the master index row out of sync, so reindex it
    indexAlnLocToSeqLocRows[0]->ReIndex(*(textRows[0]));

    // find first and last aligned master residues (in alignment coords)
    firstAlnLoc = GetWidth();
    lastAlnLoc = -1;
    for (l=0; l<GetWidth(); l++)
        if (IsAligned(textRows[0]->GetCharAt(l))) {
            firstAlnLoc = l;
            break;
        }
    for (l=GetWidth()-1; l>=0; l--)
        if (IsAligned(textRows[0]->GetCharAt(l))) {
            lastAlnLoc = l;
            break;
        }

    ERR_POST(Info << "final alignment display size: " << GetWidth() << " x " << GetNRows());
    ERR_POST(Info << "aligned region: " << firstAlnLoc << " through " << lastAlnLoc);
    status = CAV_SUCCESS;
}

AlignmentDisplay::~AlignmentDisplay()
{
    int i;
    for (i=0; i<indexAlnLocToSeqLocRows.size(); i++) delete indexAlnLocToSeqLocRows[i];
    for (i=0; i<textRows.size(); i++) delete textRows[i];
}

// shift unaligned residues as far as possible to the left
void AlignmentDisplay::ShiftUnalignedLeft(void)
{
    int gapLoc, resLoc;

    for (int row=0; row<GetNRows(); row++) {

        gapLoc = 0;
        while (gapLoc < GetWidth()) {

            // find a gap
            while (gapLoc < GetWidth() && !IsGap(textRows[row]->GetCharAt(gapLoc))) gapLoc++;
            if (gapLoc == GetWidth()) break;

            // find the next unaligned residue
            resLoc = gapLoc + 1;
            while (resLoc < GetWidth() && IsGap(textRows[row]->GetCharAt(resLoc))) resLoc++;
            if (resLoc == GetWidth()) break;
            if (!IsUnaligned(textRows[row]->GetCharAt(resLoc))) {
                gapLoc = resLoc + 1;
                continue;
            }

            // shift unaligned residues over
            while (resLoc < GetWidth() && IsUnaligned(textRows[row]->GetCharAt(resLoc))) {
                textRows[row]->SetCharAt(gapLoc++, textRows[row]->GetCharAt(resLoc));
                textRows[row]->SetCharAt(resLoc++, '-');
            }
        }
    }
}

void AlignmentDisplay::Squeeze(void)
{
    // move all unaligned residues as far left as possible - makes it much simpler
    // to identify squeezable regions
    ShiftUnalignedLeft();

    typedef vector < int > SqueezeLocs;
    SqueezeLocs squeezeLocs(GetNRows());

    // find last aligned residue; stop search one after that
    int alnLoc, lastAlignedLoc;
    for (lastAlignedLoc=GetWidth()-2;
         lastAlignedLoc>=0 && !IsAligned(textRows[0]->GetCharAt(lastAlignedLoc));
         lastAlignedLoc--) ;
    ERR_POST(Info << "checking for squeeze up to " << (lastAlignedLoc+1));

    for (alnLoc=0; alnLoc<=lastAlignedLoc+1; alnLoc++) {

        // check to see whether each row is squeezable at this location
        int row, nGaps, minNGaps = GetWidth();
        for (row=0; row<GetNRows(); row++) {
            if (!textRows[row]->IsSqueezable(alnLoc, &nGaps, &(squeezeLocs[row]), minNGaps))
                break;
            if (nGaps < minNGaps) minNGaps = nGaps;
        }

        // if all rows are squeezable, then do the squeeze
        if (row == GetNRows()) {
            ERR_POST(Info << "squeezing " << minNGaps << " gaps at loc " << alnLoc);
            for (row=0; row<GetNRows(); row++)
                textRows[row]->DeleteGaps(minNGaps, squeezeLocs[row]);
            lastAlignedLoc -= minNGaps; // account for shift of lastAlignedLoc
        }

        // after checking very first column, skip to first aligned loc to save time
        if (alnLoc == 0)
            while (alnLoc<=lastAlignedLoc && !IsAligned(textRows[0]->GetCharAt(alnLoc))) alnLoc++;
    }
}

// redistribute unaligned residues so they're split left/right between aligned residues.
// This assumes ShiftUnalignedLeft() has already been called, so that all unaligned
// residues are on the left of the gaps.
void AlignmentDisplay::SplitUnaligned(void)
{
    // alnLocs of various key residues
    int firstAligned, prevAligned, nextAligned, firstUnaligned, lastUnaligned;
    int nGaps, nShift, shiftRes, shiftGap, i;

    for (int row=0; row<GetNRows(); row++) {

        // find first aligned loc; count gaps up to there
        nGaps = 0;
        for (i=0; i<GetWidth()-2 && !IsAligned(textRows[row]->GetCharAt(i)); i++)
             if (IsGap(textRows[row]->GetCharAt(i))) nGaps++;
        firstAligned = i;

        // right-shift left tails
        if (nGaps > 0) {
            for (i=0; i<firstAligned-nGaps; i++) {
                textRows[row]->SetCharAt(firstAligned-1-i, textRows[row]->GetCharAt(firstAligned-1-nGaps-i));
                textRows[row]->SetCharAt(firstAligned-1-nGaps-i, '-');
            }
        }

        prevAligned = firstAligned;
        while (prevAligned < GetWidth()-2) {

            // find aligned res immediately followed by unaligned
            if (!(IsAligned(textRows[row]->GetCharAt(prevAligned)) &&
                  IsUnaligned(textRows[row]->GetCharAt(prevAligned + 1)))) {
                prevAligned++;
                continue;
            }
            firstUnaligned = prevAligned + 1;

            // find last unaligned residue
            for (lastUnaligned = firstUnaligned;
                 lastUnaligned < GetWidth()-1 &&
                    IsUnaligned(textRows[row]->GetCharAt(lastUnaligned + 1));
                 lastUnaligned++) ;

            // find next aligned after this
            for (nextAligned = lastUnaligned + 1;
                 nextAligned < GetWidth() &&
                    !IsAligned(textRows[row]->GetCharAt(nextAligned));
                 nextAligned++) ;
            if (nextAligned == GetWidth()) break;

            // shift over the right half of the unaligned stretch
            nGaps = nextAligned - lastUnaligned - 1;
            nShift = (lastUnaligned - firstUnaligned + 1) / 2;
            if (nGaps > 0 && nShift > 0) {
                shiftRes = lastUnaligned;
                shiftGap = nextAligned - 1;
                for (i=0; i<nShift; i++) {
                    textRows[row]->SetCharAt(shiftGap - i, textRows[row]->GetCharAt(shiftRes - i));
                    textRows[row]->SetCharAt(shiftRes - i, '-');
                }
            }
            prevAligned = nextAligned;
        }
    }
}

void AlignmentDisplay::InsertGaps(int nGaps, int beforePos)
{
    int i;
    for (i=0; i<indexAlnLocToSeqLocRows.size(); i++)
        indexAlnLocToSeqLocRows[i]->InsertGaps(nGaps, beforePos);
    for (i=0; i<textRows.size(); i++)
        textRows[i]->InsertGaps(nGaps, beforePos);
}

char AlignmentDisplay::GetCharAt(int alnLoc, int row) const
{
    if (alnLoc < 0 || alnLoc >= GetWidth() || row < 0 || row >= GetNRows()) {
        ERR_POST(Error << "AlignmentDisplay::GetCharAt() - coordinate out of range");
        return '?';
    }

    return textRows[row]->GetCharAt(alnLoc);
}

class CondensedColumn : public CObject
{
protected:
    const string color;
    vector < int > info;

    CondensedColumn(int nRows, string columnColor) : color(columnColor) { info.resize(nRows, 0); }

public:
    virtual ~CondensedColumn(void) { }

    string GetColor(void) const { return color; }
    virtual int GetDisplayWidth(void) const = 0;
    virtual int GetNResidues(int row) const = 0;
    virtual void DumpRow(CNcbiOstream& os, int row) const = 0;
    virtual void AddRowChar(int row, char ch) = 0;
};

class CondensedColumnAligned : public CondensedColumn
{
public:
    CondensedColumnAligned(int nRows, string color) : CondensedColumn(nRows, color) { }
    int GetDisplayWidth(void) const { return 1; }
    int GetNResidues(int row) const { return 1; }
    void DumpRow(CNcbiOstream& os, int row) const;
    void AddRowChar(int row, char ch);
};

void CondensedColumnAligned::DumpRow(CNcbiOstream& os, int row) const
{
    os << ((char) info[row]);
}

void CondensedColumnAligned::AddRowChar(int row, char ch)
{
    info[row] = (int) ch;
}

class CondensedColumnUnaligned : public CondensedColumn
{
private:
    static const string prefix, postfix;
    int nDigits;

public:
    CondensedColumnUnaligned(int nRows, string color) : CondensedColumn(nRows, color) { nDigits = 1; }
    int GetDisplayWidth(void) const { return prefix.size() + nDigits + postfix.size(); }
    int GetNResidues(int row) const { return info[row]; }
    void DumpRow(CNcbiOstream& os, int row) const;
    void AddRowChar(int row, char ch);
};

const string CondensedColumnUnaligned::prefix = ".[";
const string CondensedColumnUnaligned::postfix = "].";

void CondensedColumnUnaligned::DumpRow(CNcbiOstream& os, int row) const
{
    if (info[row] > 0)
        os << prefix << RIGHT_JUSTIFY << setw(nDigits) << info[row] << postfix;
    else
        os << setw(GetDisplayWidth()) << ' ';
}

void CondensedColumnUnaligned::AddRowChar(int row, char ch)
{
    if (!IsGap(ch)) {
        (info[row])++;
        int digits = ((int) log10((double) info[row])) + 1;
        if (digits > nDigits)
            nDigits = digits;
    }
}

int AlignmentDisplay::DumpCondensed(CNcbiOstream& os, unsigned int options,
    int firstCol, int lastCol, int nColumns, double conservationThreshhold,
    const char *titleHTML, int nFeatures, const AlignmentFeature *alnFeatures) const
{
    bool doHTML = ((options & CAV_HTML) > 0), doHTMLHeader = ((options & CAV_HTML_HEADER) > 0);

    if (firstCol < 0 || lastCol >= GetWidth() || firstCol > lastCol || nColumns < 1) {
        ERR_POST(Error << "AlignmentDisplay::DumpText() - nonsensical display region parameters");
        return CAV_ERROR_BAD_PARAMS;
    }

    // how many rows in the display, including annotations?
    int nDisplayRows = GetNRows();
    if (!alnFeatures) nFeatures = 0;
    if (nFeatures > 0) nDisplayRows += nFeatures;

    // do make look-up location index for each feature
    vector < vector < bool > > annotLocsByIndex;
    int i, featIndex, masterIndex;
    if (nFeatures > 0) {
        annotLocsByIndex.resize(nFeatures);
        for (featIndex=0; featIndex<nFeatures; featIndex++) {
            annotLocsByIndex[featIndex].resize(indexAlnLocToSeqLocRows[0]->sequence->Length(), false);
            for (i=0; i<alnFeatures[featIndex].nLocations; i++) {
                masterIndex = alnFeatures[featIndex].locations[i];
                if (masterIndex >= 0 && masterIndex < indexAlnLocToSeqLocRows[0]->sequence->Length())
                    annotLocsByIndex[featIndex][masterIndex] = true;
            }
        }
    }

    // condense the display into CondensedColumn list
    deque < CRef < CondensedColumn > > condensedColumns;
    int alnLoc, alnRow, row;
    bool isAlnRow;
    for (alnLoc=firstCol; alnLoc<=lastCol; alnLoc++) {
        for (alnRow=0; alnRow<GetNRows(); alnRow++)
            if (!IsAligned(textRows[alnRow]->GetCharAt(alnLoc))) break;
        bool alignedColumn = (alnRow == GetNRows());
        if (alignedColumn) {
            condensedColumns.resize(condensedColumns.size() + 1);
            condensedColumns.back().Reset(
                new CondensedColumnAligned(nDisplayRows, GetColumnColor(alnLoc, conservationThreshhold)));
        } else {
            if (condensedColumns.size() == 0 ||
                !(dynamic_cast<CondensedColumnUnaligned*>(condensedColumns.back().GetPointer()))) {
                condensedColumns.resize(condensedColumns.size() + 1);
                condensedColumns.back().Reset(
                    new CondensedColumnUnaligned(nDisplayRows, GetColumnColor(alnLoc, conservationThreshhold)));
            }
        }
        for (row=0; row<nDisplayRows; row++) {
            if (options & CAV_ANNOT_BOTTOM) {
                alnRow = row;
                featIndex = row - GetNRows();
            } else {
                featIndex = row;
                alnRow = row - nFeatures;
            }
            isAlnRow = (alnRow >= 0 && alnRow < GetNRows());
            if (isAlnRow) {
                condensedColumns.back()->AddRowChar(row, textRows[alnRow]->GetCharAt(alnLoc));
            } else if (alignedColumn) {
                // display feature characters only in aligned columns
                masterIndex = indexAlnLocToSeqLocRows[0]->GetSeqLocAt(alnLoc);
                if (masterIndex >= 0 && annotLocsByIndex[featIndex][masterIndex])
                    condensedColumns.back()->AddRowChar(row, alnFeatures[featIndex].featChar);
                else
                    condensedColumns.back()->AddRowChar(row, ' ');
            }
        }
    }

    // set up the titles and uids, figure out how much space any seqLoc string will take
    vector < string > titles(nDisplayRows), uids(doHTML ? GetNRows() : 0);
    int maxTitleLength = 0, maxSeqLocStrLength = 0, leftMargin, decimalLength;
    for (row=0; row<nDisplayRows; row++) {
        if (options & CAV_ANNOT_BOTTOM) {
            alnRow = row;
            featIndex = row - GetNRows();
        } else {
            featIndex = row;
            alnRow = row - nFeatures;
        }
        isAlnRow = (alnRow >= 0 && alnRow < GetNRows());

        // title
        titles[row] = isAlnRow ?
            indexAlnLocToSeqLocRows[alnRow]->sequence->GetTitle() :
            string(alnFeatures[featIndex].shortName);
        if (titles[row].size() > maxTitleLength) maxTitleLength = titles[row].size();
        if (isAlnRow) {
            decimalLength = ((int) log10((double)
                indexAlnLocToSeqLocRows[alnRow]->sequence->sequenceString.size())) + 1;
            if (decimalLength > maxSeqLocStrLength) maxSeqLocStrLength = decimalLength;
        }

        // uid for link to entrez
        if (doHTML && isAlnRow) {
            if (indexAlnLocToSeqLocRows[alnRow]->sequence->pdbID.size() > 0) {
                if (indexAlnLocToSeqLocRows[alnRow]->sequence->pdbID != "query" &&
                    indexAlnLocToSeqLocRows[alnRow]->sequence->pdbID != "consensus") {
                    uids[alnRow] = indexAlnLocToSeqLocRows[alnRow]->sequence->pdbID;
                    if (indexAlnLocToSeqLocRows[alnRow]->sequence->pdbChain != ' ')
                        uids[alnRow] += (char) indexAlnLocToSeqLocRows[alnRow]->sequence->pdbChain;
                    uids[alnRow] += "%5BACCN%5D";
                }
            } else if (indexAlnLocToSeqLocRows[alnRow]->sequence->gi != Sequence::NOT_SET) {
                CNcbiOstrstream uidoss;
                uidoss << indexAlnLocToSeqLocRows[alnRow]->sequence->gi << "%5BUID%5D" << '\0';
                uids[alnRow] = uidoss.str();
                delete uidoss.str();
            } else if (indexAlnLocToSeqLocRows[alnRow]->sequence->accession.size() > 0) {
                uids[alnRow] = indexAlnLocToSeqLocRows[alnRow]->sequence->accession + "%5BACCN%5D";
            }
        }
    }
    leftMargin = maxTitleLength + maxSeqLocStrLength + 2;

    // need to keep track of first, last seqLocs for each row in each paragraph;
    // find seqLoc of first residue >= firstCol
    vector < int > lastShownSeqLocs(GetNRows());
    for (alnRow=0; alnRow<GetNRows(); alnRow++) {
        lastShownSeqLocs[alnRow] = -1;
        for (alnLoc=0; alnLoc<firstCol; alnLoc++)
            if (!IsGap(textRows[alnRow]->GetCharAt(alnLoc))) lastShownSeqLocs[alnRow]++;
    }

    // header
    if (doHTML && doHTMLHeader)
        os << "<HTML><TITLE>" << (titleHTML ? titleHTML : "CDDAlignView HTML Display") <<
            "</TITLE><BODY BGCOLOR=" << bgColor << ">\n";;

    // split alignment up into "paragraphs", each with <= nColumns
    if (doHTML) os << "<TABLE>\n";
    int paragraphStart, nParags = 0, nCondensedColumns;
    ERR_POST(Info << "paragraph width: " << nColumns);
    for (paragraphStart=0;
         paragraphStart<condensedColumns.size();
         paragraphStart+=nCondensedColumns, nParags++) {

        // figure out how many condensed columns will fit in this paragraph
        int displayWidth = condensedColumns[paragraphStart]->GetDisplayWidth();
        nCondensedColumns = 1;
        while (paragraphStart+nCondensedColumns < condensedColumns.size()) {
            int columnWidth = condensedColumns[paragraphStart+nCondensedColumns]->GetDisplayWidth();
            if (displayWidth + columnWidth <= nColumns) {
                displayWidth += columnWidth;
                nCondensedColumns++;
            } else
                break;
        }

        // start table row
        if (doHTML)
            os << "<tr><td bgcolor=" << blockBGColors[nParags % nBlockColors] << "><pre>\n\n";
        else
            if (paragraphStart > 0) os << '\n';

        // output each alignment row
        for (row=0; row<nDisplayRows; row++) {
            if (options & CAV_ANNOT_BOTTOM) {
                alnRow = row;
                featIndex = row - GetNRows();
            } else {
                featIndex = row;
                alnRow = row - nFeatures;
            }
            isAlnRow = (alnRow >= 0 && alnRow < GetNRows());

            // title
            if (isAlnRow && doHTML && uids[alnRow].size() > 0) {
                os << "<a href=\"http://www.ncbi.nlm.nih.gov/entrez/query.fcgi"
                    << "?cmd=Search&doptcmdl=GenPept&db=Protein&term="
                    << uids[alnRow] << "\" onMouseOut=\"window.status=''\"\n"
                    << "onMouseOver=\"window.status='"
                    << ((indexAlnLocToSeqLocRows[alnRow]->sequence->description.size() > 0) ?
                            indexAlnLocToSeqLocRows[alnRow]->sequence->description : titles[row])
                    << "';return true\">"
                    << setw(0) << titles[row] << "</a>";
            } else {
                if (doHTML && !isAlnRow) os << "<font color=" << featColor << '>';
                os << setw(0) << titles[row];
            }
            os << setw(maxTitleLength+1-titles[row].size()) << ' ';

            if (isAlnRow) {
                // count displayed residues
                int nDisplayedResidues = 0;
                for (i=0; i<nCondensedColumns; i++)
                    nDisplayedResidues += condensedColumns[paragraphStart+i]->GetNResidues(row);

                // left start pos (output 1-numbered for humans...)
                if (doHTML) os << "<font color=" << numColor << '>';
                if (nDisplayedResidues > 0)
                    os << RIGHT_JUSTIFY << setw(maxSeqLocStrLength) << (lastShownSeqLocs[alnRow]+2) << ' ';
                else
                    os << RIGHT_JUSTIFY << setw(maxSeqLocStrLength) << ' ' << ' ';

                // dump sequence, applying color changes only when necessary
                if (doHTML) {
                    string prevColor;
                    for (i=0; i<nCondensedColumns; i++) {
                        string color = condensedColumns[paragraphStart+i]->GetColor();
                        if (color != prevColor) {
                            os << "</font><font color=" << color << '>';
                            prevColor = color;
                        }
                        condensedColumns[paragraphStart+i]->DumpRow(os, row);
                    }
                    os << "</font>";
                } else {
                    for (i=0; i<nCondensedColumns; i++) {
                        condensedColumns[paragraphStart+i]->DumpRow(os, row);
                    }
                }

                // right end pos
                if (nDisplayedResidues > 0) {
                    os << ' ';
                    if (doHTML) os << "<font color=" << numColor << '>';
                    os << LEFT_JUSTIFY << setw(0) << (lastShownSeqLocs[alnRow]+nDisplayedResidues+1);
                    if (doHTML) os << "</font>";
                }
                os << '\n';

                // setup to begin next parag
                lastShownSeqLocs[alnRow] += nDisplayedResidues;
            }

            // print alignment annotation characters
            else {
                // skip number
                os << RIGHT_JUSTIFY << setw(maxSeqLocStrLength) << ' ' << ' ';

                // do characters
                for (i=0; i<nCondensedColumns; i++)
                    condensedColumns[paragraphStart+i]->DumpRow(os, row);

                os << (doHTML ? "</font>\n" : "\n");
            }
        }

        // end table row
        if (doHTML) os << "</pre></td></tr>\n";
    }

    if (doHTML) os << "</TABLE>\n";

    // add feature legend
    if (nFeatures > 0) {
        os << (doHTML ? "<BR>\n" : "\n");
        for (featIndex=0; featIndex<nFeatures; featIndex++)
            if (alnFeatures[featIndex].description)
                os << alnFeatures[featIndex].shortName << ": " << alnFeatures[featIndex].description
                   << (doHTML ? "<BR>\n" : "\n");
    }

    if (doHTML && doHTMLHeader) os << "</BODY></HTML>\n";

    // additional sanity check on seqloc markers
    if (firstCol == 0 && lastCol == GetWidth()-1) {
        for (alnRow=0; alnRow<GetNRows(); alnRow++) {
            if (lastShownSeqLocs[alnRow] !=
                    indexAlnLocToSeqLocRows[alnRow]->sequence->sequenceString.size()-1) {
                ERR_POST(Error << "full display - seqloc markers don't add up");
                break;
            }
        }
        if (alnRow == GetNRows())
            ERR_POST(Info << "full display - seqloc markers add up correctly");
    }

    return CAV_SUCCESS;
}

int AlignmentDisplay::DumpText(CNcbiOstream& os, unsigned int options,
    int firstCol, int lastCol, int nColumns, double conservationThreshhold,
    const char *titleHTML, int nFeatures, const AlignmentFeature *alnFeatures) const
{
    bool doHTML = ((options & CAV_HTML) > 0), doHTMLHeader = ((options & CAV_HTML_HEADER) > 0);

    if (firstCol < 0 || lastCol >= GetWidth() || firstCol > lastCol || nColumns < 1) {
        ERR_POST(Error << "AlignmentDisplay::DumpText() - nonsensical display region parameters");
        return CAV_ERROR_BAD_PARAMS;
    }

    // how many rows in the display, including annotations?
    int nDisplayRows = GetNRows();
    if (!alnFeatures) nFeatures = 0;
    if (nFeatures > 0) nDisplayRows += nFeatures;

    // set up the titles and uids, figure out how much space any seqLoc string will take
    vector < string > titles(nDisplayRows), uids(doHTML ? GetNRows() : 0);
    int row, featIndex, alnRow, maxTitleLength = 0, maxSeqLocStrLength = 0, leftMargin, decimalLength;
    bool isAlnRow;
    for (row=0; row<nDisplayRows; row++) {
        if (options & CAV_ANNOT_BOTTOM) {
            alnRow = row;
            featIndex = row - GetNRows();
        } else {
            featIndex = row;
            alnRow = row - nFeatures;
        }
        isAlnRow = (alnRow >= 0 && alnRow < GetNRows());

        // title
        titles[row] = isAlnRow ?
            indexAlnLocToSeqLocRows[alnRow]->sequence->GetTitle() :
            string(alnFeatures[featIndex].shortName);
        if (titles[row].size() > maxTitleLength) maxTitleLength = titles[row].size();
        if (isAlnRow) {
            decimalLength = ((int) log10((double)
                indexAlnLocToSeqLocRows[alnRow]->sequence->sequenceString.size())) + 1;
            if (decimalLength > maxSeqLocStrLength) maxSeqLocStrLength = decimalLength;
        }

        // uid for link to entrez
        if (doHTML && isAlnRow) {
            if (indexAlnLocToSeqLocRows[alnRow]->sequence->pdbID.size() > 0) {
                if (indexAlnLocToSeqLocRows[alnRow]->sequence->pdbID != "query" &&
                    indexAlnLocToSeqLocRows[alnRow]->sequence->pdbID != "consensus") {
                    uids[alnRow] = indexAlnLocToSeqLocRows[alnRow]->sequence->pdbID;
                    if (indexAlnLocToSeqLocRows[alnRow]->sequence->pdbChain != ' ')
                        uids[alnRow] += (char) indexAlnLocToSeqLocRows[alnRow]->sequence->pdbChain;
                    uids[alnRow] += "%5BACCN%5D";
                }
            } else if (indexAlnLocToSeqLocRows[alnRow]->sequence->gi != Sequence::NOT_SET) {
                CNcbiOstrstream uidoss;
                uidoss << indexAlnLocToSeqLocRows[alnRow]->sequence->gi << "%5BUID%5D" << '\0';
                uids[alnRow] = uidoss.str();
                delete uidoss.str();
            } else if (indexAlnLocToSeqLocRows[alnRow]->sequence->accession.size() > 0) {
                uids[alnRow] = indexAlnLocToSeqLocRows[alnRow]->sequence->accession + "%5BACCN%5D";
            }
        }
    }
    leftMargin = maxTitleLength + maxSeqLocStrLength + 2;

    // need to keep track of first, last seqLocs for each row in each paragraph;
    // find seqLoc of first residue >= firstCol
    vector < int > lastShownSeqLocs(GetNRows());
    int alnLoc;
    for (alnRow=0; alnRow<GetNRows(); alnRow++) {
        lastShownSeqLocs[alnRow] = -1;
        for (alnLoc=0; alnLoc<firstCol; alnLoc++)
            if (!IsGap(textRows[alnRow]->GetCharAt(alnLoc))) lastShownSeqLocs[alnRow]++;
    }

    // header
    if (doHTML && doHTMLHeader)
        os << "<HTML><TITLE>" << (titleHTML ? titleHTML : "CDDAlignView HTML Display") <<
            "</TITLE><BODY BGCOLOR=" << bgColor << ">\n";;

    // do make look-up location index for each feature
    vector < vector < bool > > annotLocsByIndex;
    int i, masterIndex;
    if (nFeatures > 0) {
        annotLocsByIndex.resize(nFeatures);
        for (featIndex=0; featIndex<nFeatures; featIndex++) {
            annotLocsByIndex[featIndex].resize(indexAlnLocToSeqLocRows[0]->sequence->Length(), false);
            for (i=0; i<alnFeatures[featIndex].nLocations; i++) {
                masterIndex = alnFeatures[featIndex].locations[i];
                if (masterIndex >= 0 && masterIndex < indexAlnLocToSeqLocRows[0]->sequence->Length())
                    annotLocsByIndex[featIndex][masterIndex] = true;
            }
        }
    }

    // split alignment up into "paragraphs", each with nColumns
    if (doHTML) os << "<TABLE>\n";
    int paragraphStart, nParags = 0;
    for (paragraphStart=0; (firstCol+paragraphStart)<=lastCol; paragraphStart+=nColumns, nParags++) {

        // start table row
        if (doHTML)
            os << "<tr><td bgcolor=" << blockBGColors[nParags % nBlockColors] << "><pre>\n";
        else
            if (paragraphStart > 0) os << '\n';

        // do ruler
        int nMarkers = 0, width;
        if (doHTML) os << "<font color=" << rulerColor << '>';
        for (i=0; i<nColumns && (firstCol+paragraphStart+i)<=lastCol; i++) {
            if ((paragraphStart+i+1)%10 == 0) {
                if (nMarkers == 0)
                    width = leftMargin + i + 1;
                else
                    width = 10;
                os << RIGHT_JUSTIFY << setw(width) << (paragraphStart+i+1);
                nMarkers++;
            }
        }
        if (doHTML) os << "</font>";
        os << '\n';
        if (doHTML) os << "<font color=" << rulerColor << '>';
        for (i=0; i<leftMargin; i++) os << ' ';
        for (i=0; i<nColumns && (firstCol+paragraphStart+i)<=lastCol; i++) {
            if ((paragraphStart+i+1)%10 == 0)
                os << '|';
            else if ((paragraphStart+i+1)%5 == 0)
                os << '*';
            else
                os << '.';
        }
        if (doHTML) os << "</font>";
        os << '\n';

        // get column colors
        vector < string > columnColors;
        if (doHTML)
            for (i=0; i<nColumns && (firstCol+paragraphStart+i)<=lastCol; i++)
                columnColors.resize(columnColors.size()+1,
                    GetColumnColor(firstCol+paragraphStart+i, conservationThreshhold));

        // output each alignment row
        int nDisplayedResidues;
        for (row=0; row<nDisplayRows; row++) {
            if (options & CAV_ANNOT_BOTTOM) {
                alnRow = row;
                featIndex = row - GetNRows();
            } else {
                featIndex = row;
                alnRow = row - nFeatures;
            }
            isAlnRow = (alnRow >= 0 && alnRow < GetNRows());

            // actual sequence characters; count how many non-gaps in each row
            nDisplayedResidues = 0;
            string rowChars;
            if (isAlnRow) {
                for (i=0; i<nColumns && (firstCol+paragraphStart+i)<=lastCol; i++) {
                    char ch = textRows[alnRow]->GetCharAt(firstCol+paragraphStart+i);
                    rowChars += ch;
                    if (!IsGap(ch)) nDisplayedResidues++;
                }
            }

            // title
            if (isAlnRow && doHTML && uids[alnRow].size() > 0) {
                os << "<a href=\"http://www.ncbi.nlm.nih.gov/entrez/query.fcgi"
                    << "?cmd=Search&doptcmdl=GenPept&db=Protein&term="
                    << uids[alnRow] << "\" onMouseOut=\"window.status=''\"\n"
                    << "onMouseOver=\"window.status='"
                    << ((indexAlnLocToSeqLocRows[alnRow]->sequence->description.size() > 0) ?
                            indexAlnLocToSeqLocRows[alnRow]->sequence->description : titles[row])
                    << "';return true\">"
                    << setw(0) << titles[row] << "</a>";
            } else {
                if (doHTML && !isAlnRow) os << "<font color=" << featColor << '>';
                os << setw(0) << titles[row];
            }
            os << setw(maxTitleLength+1-titles[row].size()) << ' ';

            if (isAlnRow) {
                // left start pos (output 1-numbered for humans...)
                if (doHTML) os << "<font color=" << numColor << '>';
                if (nDisplayedResidues > 0)
                    os << RIGHT_JUSTIFY << setw(maxSeqLocStrLength) << (lastShownSeqLocs[alnRow]+2) << ' ';
                else
                    os << RIGHT_JUSTIFY << setw(maxSeqLocStrLength) << ' ' << ' ';

                // dump sequence, applying color changes only when necessary
                if (doHTML) {
                    string prevColor;
                    for (i=0; i<rowChars.size(); i++) {
                        if (columnColors[i] != prevColor) {
                            os << "</font><font color=" << columnColors[i] << '>';
                            prevColor = columnColors[i];
                        }
                        os << rowChars[i];
                    }
                    os << "</font>";
                } else
                    os << rowChars;

                // right end pos
                if (nDisplayedResidues > 0) {
                    os << ' ';
                    if (doHTML) os << "<font color=" << numColor << '>';
                    os << LEFT_JUSTIFY << setw(0) << (lastShownSeqLocs[alnRow]+nDisplayedResidues+1);
                    if (doHTML) os << "</font>";
                }
                os << '\n';

                // setup to begin next parag
                lastShownSeqLocs[alnRow] += nDisplayedResidues;
            }

            // print alignment annotation characters
            else {
                // skip number
                os << RIGHT_JUSTIFY << setw(maxSeqLocStrLength) << ' ' << ' ';

                // do characters, but only allow annot where master residue is aligned to something
                for (i=0; i<nColumns && (firstCol+paragraphStart+i)<=lastCol; i++) {
                    if (IsAligned(textRows[0]->GetCharAt(firstCol+paragraphStart+i))) {
                        masterIndex = indexAlnLocToSeqLocRows[0]->GetSeqLocAt(firstCol+paragraphStart+i);
                        os << ((masterIndex >= 0 && annotLocsByIndex[featIndex][masterIndex])
                            ? alnFeatures[featIndex].featChar : ' ');
                    } else
                        os << ' ';
                }

                os << (doHTML ? "</font>\n" : "\n");
            }
        }

        // end table row
        if (doHTML) os << "</pre></td></tr>\n";
    }

    if (doHTML) os << "</TABLE>\n";

    // add feature legend
    if (nFeatures > 0) {
        os << (doHTML ? "<BR>\n" : "\n");
        for (featIndex=0; featIndex<nFeatures; featIndex++)
            if (alnFeatures[featIndex].description)
                os << alnFeatures[featIndex].shortName << ": " << alnFeatures[featIndex].description
                   << (doHTML ? "<BR>\n" : "\n");
    }

    if (doHTML && doHTMLHeader) os << "</BODY></HTML>\n";

    // additional sanity check on seqloc markers
    if (firstCol == 0 && lastCol == GetWidth()-1) {
        for (alnRow=0; alnRow<GetNRows(); alnRow++) {
            if (lastShownSeqLocs[alnRow] !=
                    indexAlnLocToSeqLocRows[alnRow]->sequence->sequenceString.size()-1) {
                ERR_POST(Error << "full display - seqloc markers don't add up");
                break;
            }
        }
        if (alnRow == GetNRows())
            ERR_POST(Info << "full display - seqloc markers add up correctly");
    }

    return CAV_SUCCESS;
}

int AlignmentDisplay::DumpFASTA(int firstCol, int lastCol, int nColumns,
    bool doLowercase, CNcbiOstream& os) const
{
    if (firstCol < 0 || lastCol >= GetWidth() || firstCol > lastCol || nColumns < 1) {
        ERR_POST(Error << "AlignmentDisplay::DumpFASTA() - nonsensical display region parameters");
        return CAV_ERROR_BAD_PARAMS;
    }

    // output each alignment row
    for (int row=0; row<GetNRows(); row++) {

        // create title line
        os << '>';
        const Sequence *seq = indexAlnLocToSeqLocRows[row]->sequence;
        bool prevID = false;
        if (seq->gi != Sequence::NOT_SET) {
            os << "gi|" << seq->gi;
            prevID = true;
        }
        if (seq->pdbID.size() > 0) {
            if (prevID) os << '|';
            if (seq->pdbID == "query" || seq->pdbID == "consensus") {
                os << "lcl|" << seq->pdbID;
            } else {
                os << "pdb|" << seq->pdbID;
                if (seq->pdbChain != ' ')
                    os << '|' << (char) seq->pdbChain << " Chain "
                       << (char) seq->pdbChain << ',';
            }
            prevID = true;
        }
        if (seq->accession.size() > 0) {
            if (prevID) os << '|';
            os << seq->accession;
            prevID = true;
        }
        if (seq->description.size() > 0)
            os << ' ' << seq->description;
        os << '\n';

        // split alignment up into "paragraphs", each with nColumns
        int paragraphStart, nParags = 0, i;
        for (paragraphStart=0; (firstCol+paragraphStart)<=lastCol; paragraphStart+=nColumns, nParags++) {
            for (i=0; i<nColumns && (firstCol+paragraphStart+i)<=lastCol; i++) {
                char ch = textRows[row]->GetCharAt(firstCol+paragraphStart+i);
                if (!doLowercase) ch = toupper(ch);
                os << ch;
            }
            os << '\n';
        }
    }

    return CAV_SUCCESS;
}

const string& AlignmentDisplay::GetColumnColor(int alnLoc, double conservationThreshhold) const
{
    // standard probabilities (calculated by BLAST using BLOSUM62 - see conservation_colorer.cpp in Cn3D++)
    typedef map < char , double > Char2Double;
    static Char2Double StandardProbabilities;
    if (StandardProbabilities.size() == 0) {
        StandardProbabilities['A'] = 0.07805;
        StandardProbabilities['C'] = 0.01925;
        StandardProbabilities['D'] = 0.05364;
        StandardProbabilities['E'] = 0.06295;
        StandardProbabilities['F'] = 0.03856;
        StandardProbabilities['G'] = 0.07377;
        StandardProbabilities['H'] = 0.02199;
        StandardProbabilities['I'] = 0.05142;
        StandardProbabilities['K'] = 0.05744;
        StandardProbabilities['L'] = 0.09019;
        StandardProbabilities['M'] = 0.02243;
        StandardProbabilities['N'] = 0.04487;
        StandardProbabilities['P'] = 0.05203;
        StandardProbabilities['Q'] = 0.04264;
        StandardProbabilities['R'] = 0.05129;
        StandardProbabilities['S'] = 0.07120;
        StandardProbabilities['T'] = 0.05841;
        StandardProbabilities['V'] = 0.06441;
        StandardProbabilities['W'] = 0.01330;
        StandardProbabilities['X'] = 0;
        StandardProbabilities['Y'] = 0.03216;
    }

    // if this column isn't completely aligned, use plain color
    int row;
    for (row=0; row<GetNRows(); row++)
        if (!IsAligned(textRows[row]->GetCharAt(alnLoc))) return plainColor;

    // create profile (residue frequencies) for this column
    Char2Double profile;
    Char2Double::iterator p, pe;
    for (row=0; row<GetNRows(); row++) {
        char ch = toupper(textRows[row]->GetCharAt(alnLoc));
        switch (ch) {
            case 'A': case 'R': case 'N': case 'D': case 'C':
            case 'Q': case 'E': case 'G': case 'H': case 'I':
            case 'L': case 'K': case 'M': case 'F': case 'P':
            case 'S': case 'T': case 'W': case 'Y': case 'V':
                break;
            default:
                ch = 'X'; // make all but natural aa's just 'X'
        }
        if ((p=profile.find(ch)) != profile.end())
            p->second += 1.0/GetNRows();
        else
            profile[ch] = 1.0/GetNRows();
    }

    // check for identity...
    if (conservationThreshhold == SHOW_IDENTITY) {
        if (profile.size() == 1 && profile.begin()->first != 'X')
            return conservedColor;
        else
            return blockColor;
    }

    // ... or calculate information content for this column (calculated in bits -> logs of base 2)
    double information = 0.0;
    pe = profile.end();
    for (p=profile.begin(); p!=pe; p++) {
        static const double ln2 = log(2.0), threshhold = 0.0001;
        double expFreq = StandardProbabilities[p->first];
        if (expFreq > threshhold) {
            float freqRatio = p->second / expFreq;
            if (freqRatio > threshhold)
                information += p->second * log(freqRatio) / ln2;
        }
    }

    // if conserved, use conservation color
    if (information > conservationThreshhold) return conservedColor;

    // if is unconserved, use block color to show post-IBM block locations
    return blockColor;
}


///// Row class methods /////

void TextRow::InsertGaps(int nGaps, int beforePos)
{
    if (beforePos < 0 || beforePos > Length()) {
        ERR_POST(Error << "TextRow::InsertGaps() - beforePos out of range");
        return;
    }

    chars.insert(beforePos, nGaps, '-');
}

void TextRow::DeleteGaps(int nGaps, int startPos)
{
    if (startPos < 0 || startPos+nGaps-1 > Length()) {
        ERR_POST(Error << "TextRow::DeleteGaps() - startPos out of range");
        return;
    }

    // check to make sure they're all gaps
    for (int i=0; i<nGaps; i++) {
        if (!IsGap(chars[startPos + i])) {
            ERR_POST(Error << "TextRow::DeleteGaps() - trying to delete non-gap");
            return;
        }
    }

    chars.erase(startPos, nGaps);
}

// find out how many gaps (up to maxGaps) are present from alnLoc to the next aligned
// residue to the right; set startPos to the first gap, nGaps to # gaps starting at startPos
bool TextRow::IsSqueezable(int alnLoc, int *nGaps, int *startPos, int maxGaps) const
{
    if (alnLoc < 0 || alnLoc >= chars.size()) {
        ERR_POST(Error << "TextRow::IsSqueezable() - alnLoc out of range");
        return false;
    }

    // skip unaligned residues
    while (alnLoc < chars.size() && IsUnaligned(chars[alnLoc])) alnLoc++;
    if (alnLoc == chars.size() || IsAligned(chars[alnLoc])) return false;

    // count gaps
    *startPos = alnLoc;
    for (*nGaps=1, alnLoc++; alnLoc < chars.size() && IsGap(chars[alnLoc]); (*nGaps)++, alnLoc++)
        if (*nGaps == maxGaps) break;
    return true;
}


IndexAlnLocToSeqLocRow::IndexAlnLocToSeqLocRow(const Sequence *seq, int length) :
    sequence(seq)
{
    if (length > 0) seqLocs.resize(length, -1);
}

void IndexAlnLocToSeqLocRow::InsertGaps(int nGaps, int beforePos)
{
    if (nGaps <= 0 || seqLocs.size() == 0) return;

    if (beforePos < 0 || beforePos > Length()) {
        ERR_POST(Error << "IndexAlnLocToSeqLocRow::InsertGaps() - beforePos out of range");
        return;
    }

    IntVec::iterator s = seqLocs.begin();
    for (int i=0; i<beforePos; i++) s++;
    seqLocs.insert(s, nGaps, -1);
}

void IndexAlnLocToSeqLocRow::ReIndex(const TextRow& textRow)
{
    seqLocs.resize(textRow.Length());
    int seqLoc = 0;
    for (int i=0; i<textRow.Length(); i++) {
        if (IsGap(textRow.GetCharAt(i)))
            seqLocs[i] = -1;
        else
            seqLocs[i] = seqLoc++;
    }
    if (seqLoc != sequence->sequenceString.size())
        ERR_POST(Error << "IndexAlnLocToSeqLocRow::ReIndex() - wrong sequence length");
}

END_NCBI_SCOPE


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/03/19 19:04:12  thiessen
* move again
*
* Revision 1.2  2003/03/19 13:25:09  thiessen
* fix log ambiguity
*
* Revision 1.1  2003/03/19 05:33:43  thiessen
* move to src/app/cddalignview
*
* Revision 1.23  2003/02/03 17:52:03  thiessen
* move CVS Log to end of file
*
* Revision 1.22  2003/01/21 18:01:07  thiessen
* add condensed alignment display
*
* Revision 1.21  2002/12/09 13:31:04  thiessen
* use new query.fcgi for genpept links
*
* Revision 1.20  2002/11/08 19:38:11  thiessen
* add option for lowercase unaligned in FASTA
*
* Revision 1.19  2002/04/25 13:14:55  thiessen
* fix range test
*
* Revision 1.18  2002/02/12 13:08:20  thiessen
* annot description optional
*
* Revision 1.17  2002/02/12 12:54:10  thiessen
* feature legend at bottom; annot only where aligned
*
* Revision 1.16  2002/02/08 19:53:17  thiessen
* add annotation to text/HTML displays
*
* Revision 1.15  2001/03/02 01:19:24  thiessen
* add FASTA output
*
* Revision 1.14  2001/02/16 19:18:47  thiessen
* change color scheme again
*
* Revision 1.13  2001/02/15 19:23:44  thiessen
* add identity coloring
*
* Revision 1.12  2001/02/15 18:08:15  thiessen
* change color scheme
*
* Revision 1.11  2001/02/14 16:06:09  thiessen
* add block and conservation coloring to HTML display
*
* Revision 1.10  2001/02/14 03:16:27  thiessen
* fix seqloc markers and right-justification of left tails
*
* Revision 1.9  2001/02/14 00:06:49  thiessen
* filter out consensus
*
* Revision 1.8  2001/01/29 23:55:09  thiessen
* add AlignmentDisplay verification
*
* Revision 1.7  2001/01/29 18:13:33  thiessen
* split into C-callable library + main
*
* Revision 1.6  2001/01/25 00:51:20  thiessen
* add command-line args; can read asn data from stdin
*
* Revision 1.5  2001/01/23 23:17:48  thiessen
* fix uid link problems
*
* Revision 1.4  2001/01/23 20:42:00  thiessen
* add description
*
* Revision 1.3  2001/01/23 17:34:12  thiessen
* add HTML output
*
* Revision 1.2  2001/01/22 15:55:11  thiessen
* correctly set up ncbi namespacing
*
* Revision 1.1  2001/01/22 13:15:23  thiessen
* initial checkin
*
*/
