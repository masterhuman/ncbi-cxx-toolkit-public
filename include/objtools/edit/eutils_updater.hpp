#ifndef _EUTILS_UPDATER_HPP_
#define _EUTILS_UPDATER_HPP_

#include <objtools/eutils/api/eutils.hpp>
#include <objtools/edit/pubmed_updater.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

class CEUtilsUpdater : IPubmedUpdater
{
public:
    CEUtilsUpdater();
    TEntrezId CitMatch(const CPub&) override;
    CRef<CPub> GetPub(TEntrezId pmid, EPubmedError* = nullptr) override;
    CRef<CTitle_msg_list> GetTitle(const CTitle_msg&) override;
private:
    CRef<CEUtils_ConnContext> m_Ctx;
};

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // _EUTILS_UPDATER_HPP_
