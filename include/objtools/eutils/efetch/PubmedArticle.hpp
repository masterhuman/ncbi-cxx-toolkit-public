/* $Id$
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
 */

/// @file PubmedArticle.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'efetch.dtd'.
///
/// New methods or data members can be added to it if needed.
/// See also: PubmedArticle_.hpp


#ifndef eutils__OBJTOOLS_EUTILS_EFETCH_PUBMEDARTICLE_HPP
#define eutils__OBJTOOLS_EUTILS_EFETCH_PUBMEDARTICLE_HPP

#include <locale>
#include <serial/iterator.hpp>

// generated includes
#include <objtools/eutils/efetch/PubmedArticle_.hpp>

// generated classes

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);
class CPubmed_entry;
class CDate;
class CTitle;
class CArticleIdSet;
END_NAMESPACE(objects);
END_NAMESPACE(ncbi);



BEGIN_eutils_SCOPE // namespace eutils::

/////////////////////////////////////////////////////////////////////////////
class CPubmedArticle : public CPubmedArticle_Base
{
    typedef CPubmedArticle_Base Tparent;
public:
    // constructor
    CPubmedArticle(void);
    // destructor
    ~CPubmedArticle(void);

    ncbi::CRef<ncbi::objects::CPubmed_entry> ToPubmed_entry(void) const;

private:
    // Prohibit copy constructor and assignment operator
    CPubmedArticle(const CPubmedArticle& value);
    CPubmedArticle& operator=(const CPubmedArticle& value);

};

/////////////////// CPubmedArticle inline methods

// constructor
inline
CPubmedArticle::CPubmedArticle(void)
{
}


/////////////////// end of CPubmedArticle inline methods


/////////////////// Helper classes and functions also used by CPubmedBookArticle

class CPubDate;
class CPubMedPubDate;
class CArticleTitle;
class CVernacularTitle;
class CAuthor;
class CPagination;
class CArticleIdList;
class CArticle;
class CGrantList;
class CText;

std::string s_CleanupText(std::string str);
ncbi::CRef<ncbi::objects::CDate> s_GetDateFromPubDate(const CPubDate& pub_date);
ncbi::CRef<ncbi::objects::CDate> s_GetDateFromPubMedPubDate(const CPubMedPubDate& pdate);
std::string s_GetArticleTitleStr(const CArticleTitle& article_title);
std::string s_GetVernacularTitleStr(const CVernacularTitle& vernacular_title);
ncbi::CRef<ncbi::objects::CTitle> s_MakeTitle(
    const std::string& title_str,
    const std::string& vernacular_title_str);
std::string s_GetAuthorMedlineName(const CAuthor& author);
std::string s_GetPagination(const CPagination& pagination);
ncbi::CRef<ncbi::objects::CArticleIdSet> s_GetArticleIdSet(
    const CArticleIdList& article_id_list,
    const CArticle* article);
void s_FillGrants(std::list<std::string>& id_nums, const CGrantList& grant_list);
std::string s_TextToString(const CText& text);


template <class CharT>
inline auto get_ctype_facet() -> decltype(std::use_facet<std::ctype<CharT>>(std::locale()))
{
    static const std::locale s_Locale("C");
    static const auto& s_CType = std::use_facet<std::ctype<CharT>>(s_Locale);
    return s_CType;
}


template <class CharT>
inline auto get_ctype_facet(const std::locale& loc) -> decltype(std::use_facet<std::ctype<CharT>>(std::locale()))
{
    static const auto& s_CType = std::use_facet<std::ctype<CharT>>(loc);
    return s_CType;
}


template<class TE>
typename std::enable_if<std::is_member_function_pointer<decltype(&TE::IsText)>::value, std::string>::type
s_TextOrOtherToString(const TE& te) {
    if (te.IsText()) {
        return s_TextToString(te.GetText());
    }
    else {
        std::string ret;
        for (ncbi::CStdTypeConstIterator<std::string> j(Begin(te)); j; ++j) {
            ret.append(*j);
        }
        return ret;
    }
}


template<class TE>
typename std::enable_if<std::is_member_function_pointer<decltype(&TE::IsSetText)>::value, std::string>::type
s_TextOrOtherToString(const TE& te) {
    if (te.IsSetText()) {
        return s_TextListToString(te.GetText());
    }
    else {
        std::string ret;
        for (ncbi::CStdTypeConstIterator<std::string> j(Begin(te)); j; ++j) {
            ret.append(*j);
        }
        return ret;
    }
}


template<class TE>
std::string s_TextListToString(const std::list<ncbi::CRef<TE>>& text_list)
{
    std::string ret;
    for (auto it : text_list) {
        ret.append(s_TextToString(*it));
    }
    return ret;
}


template<class TE>
std::string s_TextOrOtherListToString(const std::list<ncbi::CRef<TE>>& text_list)
{
    std::string ret;
    for (auto it : text_list) {
        ret.append(s_TextOrOtherToString(*it));
    }
    return ret;
}


template<class D> ncbi::CRef<ncbi::objects::CTitle> s_GetTitle(const D& doc)
{
    std::string title_str;
    if (doc.IsSetArticleTitle()) title_str = s_GetArticleTitleStr(doc.GetArticleTitle());
    std::string vernacular_title_str;
    if (doc.IsSetVernacularTitle()) {
        vernacular_title_str = s_GetVernacularTitleStr(doc.GetVernacularTitle());
    }
    return s_MakeTitle(title_str, vernacular_title_str);
}


END_eutils_SCOPE // namespace eutils::


#endif // eutils__OBJTOOLS_EUTILS_EFETCH_PUBMEDARTICLE_HPP
/* Original file checksum: lines: 82, chars: 2477, CRC32: 26408000 */
