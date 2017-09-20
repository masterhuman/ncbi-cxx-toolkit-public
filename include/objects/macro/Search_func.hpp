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

/// @file Search_func.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'macro.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: Search_func_.hpp


#ifndef OBJECTS_MACRO_SEARCH_FUNC_HPP
#define OBJECTS_MACRO_SEARCH_FUNC_HPP


// generated includes
#include <objects/macro/Search_func_.hpp>
#include <objects/macro/String_constraint.hpp>
#include <objects/macro/Word_substitution.hpp>
#include <objects/macro/Word_substitution_set.hpp>


// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

/////////////////////////////////////////////////////////////////////////////
class CSearch_func : public CSearch_func_Base
{
    typedef CSearch_func_Base Tparent;
public:
    CSearch_func() {}
    ~CSearch_func() {}

    bool Empty() const;
    bool Match(const string& str) const;

private:
    // Prohibit copy constructor and assignment operator
    CSearch_func(const CSearch_func& value);
    CSearch_func& operator=(const CSearch_func& value);

    // e_Contains_plural
    bool x_StringMayContainPlural(const string& str) const;
    bool x_DoesStrContainPlural(const string& word,
                                 char last_letter,
                                 char second_to_last_letter,
                                 char next_letter) const;

    // e_N_or_more_brackets_or_parentheses
    bool x_ContainsNorMoreSetsOfBracketsOrParentheses(const string& str,
                                                       const int& n) const;
    char x_GetClose(char bp) const;
    bool x_SkipBracketOrParen(size_t idx, string& start) const;
    
    // e_Three_numbers
    bool x_ContainsThreeOrMoreNumbersTogether(const string& str) const;
    bool x_FollowedByFamily(string& after_str) const;
    bool x_PrecededByOkPrefix (const string& start_str) const;
    bool x_InWordBeforeCytochromeOrCoenzyme(const string& start_str) const;

    // e_Underscore
    bool x_StringContainsUnderscore(const string& str) const;

    // e_Prefix_and_numbers
    bool x_IsPrefixPlusNumbers(const string& str, const string& prefix) const;

    // e_Unbalanced_paren
    bool x_StringContainsUnbalancedParentheses(const string& str) const;
    bool x_IsPropClose(const string& str, char open_p) const;

    // e_Has_term
    bool x_ProductContainsTerm(const string& str, const string& pattern) const;
};

END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE
#endif // OBJECTS_MACRO_SEARCH_FUNC_HPP
