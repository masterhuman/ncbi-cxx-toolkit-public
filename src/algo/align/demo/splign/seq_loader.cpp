/* $Id$
* ===========================================================================
*
*                            public DOMAIN NOTICE                          
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
* Author:  Yuri Kapustin
*
* File Description:
*   CSeqLoader class
*
*/

#include "seq_loader.hpp"
#include "splign_app_exception.hpp"


BEGIN_NCBI_SCOPE

void CSeqLoader::Open(const string& filename_index)
{
  CNcbiIfstream idxstream (filename_index.c_str());
  if(idxstream) {

    // first read file names
    char buf [1024];
    bool found_start = false;
    while(idxstream.good()) {
      buf[0] = 0;
      idxstream.getline(buf, sizeof buf, '\n');
      if(buf[0] == '#') continue;
      if(idxstream.fail()) {
	goto throw_file_corrupt;
      }
      if(strcmp(buf,"$$$FI") == 0) {
	found_start = true;
	break;
      }
    }
    if(!found_start) {
	goto throw_file_corrupt;
    }

    vector<string> filenames;
    vector<size_t> indices;
    found_start = false;
    while(idxstream.good()) {
      buf[0] = 0;
      idxstream.getline(buf, sizeof buf, '\n');

      if(strcmp("$$$SI", buf) == 0) {
	found_start = true;
	break;
      }
      if(buf[0] == '#' || buf[0] == 0) {
	continue;
      }
      CNcbiIstrstream iss (buf);
      size_t idx = kMax_UInt;
      string name;
      iss >> name >> idx;
      if(idx == kMax_UInt) {
	goto throw_file_corrupt;
      }
      filenames.push_back(name);
      indices.push_back(idx);
    }
    if(!found_start) goto throw_file_corrupt;

    const size_t fndim = filenames.size();
    m_filenames.resize(fndim);
    for(size_t i = 0; i < fndim; ++i) {
      m_filenames[indices[i]] = filenames[i];
    }

    m_idx.clear();
    while(idxstream.good()) {

      buf[0] = 0;
      idxstream.getline(buf, sizeof buf, '\n');

      if(buf[0] == '#') continue; // skip comments
      if(idxstream.eof()) break;

      if(idxstream.fail()) {
	goto throw_file_corrupt;
      }

      CNcbiIstrstream iss (buf);
      string id;
      SIdxTarget s;
      iss >> id >> s.m_filename_idx >> s.m_offset;
      if(s.m_offset == kMax_UInt) {
	goto throw_file_corrupt;
      }
      m_idx[id] = s;
    }
    
    return;
  }
  else {      
    NCBI_THROW(
	       CSplignAppException,
	       eCannotOpenFile,
	       filename_index.c_str());
  }

 throw_file_corrupt: {

    NCBI_THROW(
	       CSplignAppException,
	       eErrorReadingIndexFile,
	       "File is corrupt");
  }
}


void CSeqLoader::Load(const string& id, vector<char>* seq,
		      size_t from, size_t to)
{
  auto_ptr<istream> input (0);

  map<string, SIdxTarget>::const_iterator im = m_idx.find(id);
  if(im == m_idx.end()) {
    string msg ("Unable to locate ");
    msg += id;
    NCBI_THROW(
	       CSplignAppException,
	       eInternal,
	       msg.c_str());
  }
  else {
    const string& filename = m_filenames[im->second.m_filename_idx];
    input.reset(new ifstream (filename.c_str()));
    input->seekg(im->second.m_offset);
  }
  
  char buf [1024];
  input->getline(buf, sizeof buf, '\n'); // info line
  if(input->fail()) {
    NCBI_THROW(
	       CSplignAppException,
	       eCannotReadFile,
	       "Unable to read sequence data");
  }

  seq->clear();

  if(from == 0 && to == kMax_UInt) {
    // read entire sequence until the next one or eof
    while(*input) {
      size_t i0 = input->tellg();
      input->getline(buf, sizeof buf, '\n');
      if(!*input) {
	break;
      }
      size_t i1 = input->tellg();
      if(i1 - i0 > 1) {
	size_t line_size = i1 - i0 - 1;
	if(buf[0] == '>') break;
	size_t size_old = seq->size();
	seq->resize(size_old + line_size);
	copy(buf, buf + line_size, seq->begin() + size_old);
      }
      else if (i0 == i1) {
	break; // GCC hack
      }
    }
  }
  else {
    // read only a portion of a sequence
    const size_t dst_seq_len = to - from + 1;
    seq->resize(dst_seq_len + sizeof buf);
    size_t i0 = input->tellg(), i1;
    size_t dst_read = 0, src_read = 0;
    while(*input) {
      input->getline(buf, sizeof buf, '\n');
      if(buf[0] == '>' || !*input) {
	seq->resize(dst_read);
	return;
      }
      i1 = input->tellg();

      if(i1 - i0 > 1) {
	src_read += i1 - i0 - 1;
      }
      else if(i1 - i0 == 1) {
	continue;
      }
      else { 
	break; // GCC hack
      }

      if(src_read > from) {
	size_t line_size = i1 - i0 - 1;
	size_t start  = dst_read? 0: (line_size - (src_read - from));
	size_t finish = (src_read > to)?
	                (line_size - (src_read - to) + 1):
	                line_size;
	copy(buf + start, buf + finish, seq->begin() + dst_read);
	dst_read += finish - start;
	if(dst_read >= dst_seq_len) {
	  seq->resize(dst_seq_len);
	  return;
	}
      }
      i0 = i1;
    }
    seq->resize(dst_read);
  }

}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2003/11/20 17:58:20  kapustin
 * Make the code msvc6.0-friendly
 *
 * Revision 1.6  2003/11/14 13:13:29  ucko
 * Fix #include directives: +<memory> for auto_ptr; -<fstream>
 * (redundant, and seq_loader.hpp should be the first anyway)
 *
 * Revision 1.5  2003/11/06 02:07:41  kapustin
 * Fix NCB_THROW usage
 *
 * Revision 1.4  2003/11/05 20:32:10  kapustin
 * Include source information into the index
 *
 * Revision 1.2  2003/10/31 19:43:15  kapustin
 * Format and compatibility update
 *
 * ===========================================================================
 */
