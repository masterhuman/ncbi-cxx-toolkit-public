#ifndef ENTRY_H
#define ENTRY_H

#include <objects/seq/Bioseq.hpp>

#include "ftablock.h"
#include "entry.h"
BEGIN_NCBI_SCOPE

//  ============================================================================
struct Section
//  ============================================================================
{
    Section(
        int                   type_,
        const vector<string>& textLines) :
        mType(type_),
        mTextLines(textLines.begin(), textLines.end()),
        mpQscore(nullptr),
        mDrop(false) {}

    ~Section()
    {
        delete[] mpQscore;
        for (auto subStr : mSubSections) {
            delete subStr;
        }
    }

    void DumpText(ostream& ostr) const
    {
        ostr << NStr::Join(mTextLines, "\n");
        ostr << endl
             << endl;
        for (auto subPtr : mSubSections) {
            for (auto line : subPtr->mTextLines) {
                ostr << ">>  " << line << endl;
            }
            ostr << endl
                 << endl;
        }
    }

    void xBuildSubBlock(int subtype, const char* subKw);
    void xBuildFeatureBlocks();

    int              mType; // which keyword block or node type
    vector<string>   mTextLines;
    vector<Section*> mSubSections;
    char*            mpQscore; // points to quality score buffer
    bool             mDrop;
};
using SectionPtr = Section*;


//  ============================================================================
struct Entry {
    //  ============================================================================

    Entry(
        ParserPtr   pp,
        const char* baseData) :
        mPp(pp),
        mBaseData(baseData)
    {
    }

    ~Entry()
    {
        for (auto sectionPtr : mSections) {
            delete sectionPtr;
        }
    };

    SectionPtr GetFirstSectionOfType(int type)
    {
        for (auto secPtr : mSections) {
            if (secPtr->mType == type) {
                return secPtr;
            }
        }
        return nullptr;
    }

    void AppendSection(SectionPtr secPtr)
    {
        if (secPtr) {
            mSections.push_back(secPtr);
        }
    }

    bool IsAA() const;

    bool xInitNidSeqId(objects::CBioseq&, int type, int dataOffset, Parser::ESource);
    bool xInitSeqInst(const unsigned char* pConvert);

    ParserPtr                 mPp;
    string                    mBaseData;
    list<SectionPtr>          mSections;
    CRef<objects::CSeq_entry> mSeqEntry;
};
using EntryPtr = Entry*;


DataBlkPtr LoadEntry(ParserPtr pp, size_t offset, size_t len);
EntryPtr   LoadEntryGenbank(ParserPtr pp, size_t offset, size_t len);

END_NCBI_SCOPE

#endif // ENTRY_H
