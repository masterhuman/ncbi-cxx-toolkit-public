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
* Author:  Pavel Ivanov, Aleksey Grichenko, NCBI
*
* File Description:
*   Unit test for CFormatGuess class
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <util/format_guess.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>


USING_NCBI_SCOPE;


NCBITEST_AUTO_INIT()
{
}

NCBITEST_AUTO_FINI()
{
}


BOOST_AUTO_TEST_CASE(TestSupportedFormats)
{
    BOOST_CHECK(CFormatGuess::IsSupportedFormat(CFormatGuess::ePsl));
    BOOST_CHECK(!CFormatGuess::IsSupportedFormat(CFormatGuess::eAltGraphX));
}


BOOST_AUTO_TEST_CASE(TestEmptyFile)
{
    CNcbiIstrstream str(kEmptyStr);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eUnknown);
}


BOOST_AUTO_TEST_CASE(TestBinaryAsn)
{
    // Real data does not matter, just need some non-printable chars.
    const string kData_AsnBin = "\1\2\3\4\5\6\7\0"s;

    CNcbiIstrstream str(kData_AsnBin);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eBinaryASN);
}


BOOST_AUTO_TEST_CASE(TestBinaryAsnSeqFeat)
{
    const string kData_AsnBin_SeqFeat = 
        "\x30\x80\xa0\x80\xa2\x80\xa0\x80\x02\x01\x09\x00\x00\x00\x00\x00"
        "\x00\xa1\x80\xa0\x80\x30\x80\xa0\x80\x1a\x07\x56\x4e\x31\x52\x35" 
        "\x34\x50\x00\x00\x00\x00\x00\x00\x00\x00\xa6\x80\xa3\x80\x30\x80"
        "\xa0\x80\x02\x03\x0c\x3a\x4c\x00\x00\xa1\x80\x02\x03\x0c\x3d\xc2"
        "\x00\x00\xa2\x80\x0d\x0a\x01\x01\x00\x00\xa3\x80\xab\x80\x02\x04"
        "\x0d\x61\xd3\x43\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xa9\x80"
        "\x30\x80\xa1\x80\xa1\x80\x1a\x1a\x43\x6f\x6d\x62\x69\x6e\x65\x64"
        "\x46\x65\x61\x74\x75\x72\x65\x55\x73\x65\x72\x4f\x62\x6a\x65\x63"
        "\x74\x73\x00\x00\x00\x00\xa2\x80\x30\x80\x30\x80\xa0\x80\xa1\x80"
        "\x1a\x0d\x0a\x54\x72\x61\x63\x6b\x69\x6e\x67\x49\x64\x00\x00\x00"
        "\x00\xa2\x80\xa1\x80\x02\x03\x01\x82\x08\x00\x00\x00\x00\x00\x00"
        "\x30\x80\xa0\x80\xa1\x80\x1a\x0d\x4d\x6f\x64\x65\x6c\x45\x76\x69"
        "\x64\x65\x6e\x63\x65\x00\x00\x00\x00\xa2\x80\xa5\x80\x30\x80\xa1"
        "\x80\xa1\x80\x1a\x0d\x4d\x6f\x64\x65\x6c\x45\x76\x69\x64\x65\x6e"
        "\x63\x65\x00\x00\x00\x00\xa2\x80\x30\x80\x30\x80\xa0\x80\xa1\x80"
        "\x1a\x06\x4d\x65\x74\x68\x6f\x64\x00\x00\x00\x00\xa2\x80\xa0\x80"
        "\x1a\x0f\x43\x75\x72\x61\x74\x65\x64\x20\x47\x65\x6e\x6f\x6d\x69"
        "\x63\x00\x00\x00\x00\x00\x00\x30\x80\xa0\x80\xa1\x80\x1a\x06\x53"
        "\x6f\x75\x72\x63\x65\x00\x00\x00\x00\xa2\x80\xa0\x80\x1a\x0b\x4e"
        "\x47\x5f\x30\x31\x35\x36\x36\x35\x2e\x31\x00\x00\x00\x00\x00\x00"
        "\x30\x80\xa0\x80\xa1\x80\x1a\x0b\x43\x6f\x6e\x74\x69\x67\x20\x4e"
        "\x61\x6d\x65\x00\x00\x00\x00\xa2\x80\xa0\x80\x1a\x09\x4e\x54\x5f"
        "\x30\x33\x33\x39\x38\x35\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\xad\x80\x31\x80\x30\x80\xa0\x80\x1a\x06\x47\x65\x6e\x65\x49\x44"
        "\x00\x00\xa1\x80\xa0\x80\x02\x04\x05\xfa\xa6\xe7\x00\x00\x00\x00"
        "\x00\x00\x30\x80\xa0\x80\x1a\x04\x48\x47\x4e\x43\x00\x00\xa1\x80"
        "\xa0\x80\x02\x03\x00\x91\xfe\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\xae\x80\x01\x01\x01\x00\x00\x00\x00"s;

    CNcbiIstrstream str(kData_AsnBin_SeqFeat);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eBinaryASN);
}


BOOST_AUTO_TEST_CASE(TestRepeatMasker)
{
    const string kData_Rmo =
        " 1320 15.6  6.2  0.0  HSU08988  6563 6781 (22462) C  MER7A   "
        "DNA/MER2_type    (0)  337  104  20\n"
        "12279 10.5  2.1  1.7  HSU08988  6782 7718 (21525) C  Tigger1 "
        "DNA/MER2_type    (0) 2418 1486  19\n"
        " 1769 12.9  6.6  1.9  HSU08988  7719 8022 (21221) C  AluSx   "
        "SINE/Alu         (0)  317    1  17\n"
        "12279 10.5  2.1  1.7  HSU08988  8023 8694 (20549) C  Tigger1 "
        "DNA/MER2_type  (932) 1486  818  19\n";
    const string kHeader_Rmo =
        "SW perc query position matching\n"
        "score div. del. ins. sequence\n";
    {{
        // Without header+
        CNcbiIstrstream str(kData_Rmo);
        CFormatGuess guess(str);
        BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eRmo);
    }}
    {{
        // With header
        string with_header = string(kHeader_Rmo) + string(kData_Rmo);
        CNcbiIstrstream str(with_header);
        CFormatGuess guess(str);
        BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eRmo);
    }}
}


BOOST_AUTO_TEST_CASE(TestGtf)
{
    const string kData_Gtf =
        "381 Twinscan  CDS          380   401   .   +   0  "
        "gene_id \"001\"; transcript_id \"001.1\";\n"
        "381 Twinscan  CDS          501   650   .   +   2  "
        "gene_id \"001\"; transcript_id \"001.1\";\n"
        "381 Twinscan  CDS          700   707   .   +   2  "
        "gene_id \"001\"; transcript_id \"001.1\";\n"
        "381 Twinscan  start_codon  380   382   .   +   0  "
        "gene_id \"001\"; transcript_id \"001.1\";\n";
    
    CNcbiIstrstream str(kData_Gtf);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eGtf);
}


BOOST_AUTO_TEST_CASE(TestPsl)
{
     const string kData_Psl =
        "186559	"
        "44	"
        "0	"
        "0	"
        "5	"
        "282	"
        "6	"
        "65	"
        "+	"
        "NW_009646194.1	"
        "186494	"
        "0	"
        "186494	"
        "NC_000001.11	"
        "248956422	"
        "41250327	"
        "41436604	"
        "12	"
        "113384,4962,1189,5577,3627,1816,1275,6064,4707,2536,1724,39351	"
        "0,113384,118357,119600,125177,128804,130621,132096,138160,142867,145419,147143	"
        "41250327,41363720,41368682,41369871,41375472,41379109,41380925,41382200,41388270,41392978,41395514,41397253";

    CNcbiIstrstream str(kData_Psl);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::ePsl);
}


BOOST_AUTO_TEST_CASE(TestGvf)
{
    const string kData_Gvf =
        "NC_000008.9    dbVar    CNV    151699    186841    .    .    .    ID=nsv6034;Variant_seq=A;Name=nsv6034(CNV);Start_range=151699,152699;End_range=185641,186841\n"
        "NC_000008.9    dbVar    SNV    212185    257141    .    .    .    ID=nsv6035;Variant_seq=A;Name=nsv6035(CNV)\n"
        "NC_000008.9    dbVar    CNV    577296    606629    .    .    .    ID=nsv6036;Variant_seq=A;Name=nsv6036(CNV)\n";

    CNcbiIstrstream str(kData_Gvf);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eGvf);
}


BOOST_AUTO_TEST_CASE(TestGff3)
{
    const string kData_Gff3 =
        "NC_000008.9    dbVar    misc    151699    186841    .    .    .    ID=nsv6034;Name=nsv6034(CNV)\n"
        "NC_000008.9    dbVar    misc    212185    257141    .    .    .    ID=nsv6035;Name=nsv6035(CNV)\n"
        "NC_000008.9    dbVar    misc    577296    606629    .    .    .    ID=nsv6036;Name=nsv6036(CNV)\n";

    CNcbiIstrstream str(kData_Gff3);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eGff3);
}


BOOST_AUTO_TEST_CASE(TestGff2)
{
    const string kData_Gff2 =
        "NC_000008.9    dbVar    misc    151699    186841    .    .    .    feat=a\n"
        "NC_000008.9    dbVar    misc    212185    257141    .    .    .    feat=b\n"
        "NC_000008.9    dbVar    misc    577296    606629    .    .    .    feat=c\n";

    CNcbiIstrstream str(kData_Gff2);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eGff2);
}


BOOST_AUTO_TEST_CASE(TestGff3Comment)
{
    // Test for handling cases with huge comments
    const string kData_Gff3_Comment = 
        "##gff-version 3\n"
        "##species https://www.ncbi.nlm.nih.gov/Taxonomy/Browser/wwwtax.cgi?id=9606\n"
        "##date 2009-11-04\n"
        "# Study_accession: nstd17\n"
        "# Display_name: Conrad et al 2006\n"
        "# PMID: 16327808\n"
        "# Study_description: We report a new method that uses SNP genotype data"
        " from parent-offspring trios to identify polymorphic deletions. We applied"
        " this method to data from the International HapMap Project to produce the first"
        " high-resolution population surveys of deletion polymorphism. Approximately"
        " 100 of these deletions have been experimentally validated using comparative"
        " genome hybridization on tiling-resolution oligonucleotide microarrays. Our"
        " analysis identifies a total of 586 distinct regions that harbor deletion"
        " polymorphisms in one or more of the families.\n"
        "# Lorem ipsum dolor sit amet, consectetur adipiscing elit. Etiam elementum"
        " arcu feugiat risus pharetra pellentesque. Suspendisse potenti. Curabitur"
        " non arcu cursus tortor consequat bibendum vel in nisl. Sed sagittis"
        " consequat velit, vel lacinia orci vestibulum vel. Sed in sapien vel"
        " lectus consequat dignissim nec a neque. Quisque eget dolor tellus,"
        " eget mollis enim. Nam laoreet cursus enim, ut auctor sapien sodales quis."
        " Duis dolor eros, dictum aliquet bibendum quis, consectetur quis sem."
        " Nullam a arcu eget diam gravida aliquet a nec est. Proin ac vehicula"
        " mauris. Sed non erat lectus. Nullam id sollicitudin urna. Nullam"
        " placerat, justo in lacinia consectetur, lectus nulla vehicula est,"
        " eu aliquam mauris velit interdum lorem.\n"
        "NC_000001.7    dbVar    misc    10415637    10427143    .    .    .    ID=nsv436924;Name=nsv436924(CNV)\n"
        "NC_000001.7    dbVar     misc    101474397    101476638    .    .    .    ID=nsv436925;Name=nsv436925(CNV)\n"
        "NC_000003.8    dbVar     misc    164983304    164985198    .    .    .    ID=nsv436926;Name=nsv436926(CNV)\n";

    CNcbiIstrstream str(kData_Gff3_Comment);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eGff3);
}


BOOST_AUTO_TEST_CASE(TestGlimmer3)
{
    const string kData_Glimmer3 =
        ">gms:3447|cmr:632 chromosome 1 {Mycobacterium smegmatis MC2}\n"
        "orf00001 499 1692 +1 13.14\n"
        "orf00004 1721 2614 +2 14.20\n"
        "orf00006 2624 3778 +2 10.35\n"
        "orf00009 3775 4359 +1 9.34\n";

    CNcbiIstrstream str(kData_Glimmer3);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eGlimmer3);
}


BOOST_AUTO_TEST_CASE(TestAgp)
{
    const string kData_Agp =
        //"# ORGANISM: Homo sapiens\n"
        //"# TAX_ID: 9606\n"
        //"# ASSEMBLY NAME: EG1\n"
        //"# ASSEMBLY DATE: 06-September-2006\n"
        //"# GENOME CENTER: NCBI\n"
        //"# DESCRIPTION: Example AGP specifying the assembly of scaffolds from WGS contigs\n"
        "EG1_scaffold1\t1\t3043\t1\tW\tAADB02037551.1\t1\t3043\t+\n"
        "EG1_scaffold2\t1\t40448\t1\tW\tAADB02037552.1\t1\t40448\t+\n"
        "EG1_scaffold2\t40449\t40548\t2\tN\t100    fragment\tyes\t\n"
        "EG1_scaffold2\t40549\t117529\t3\tW\tAADB02037553.1\t1\t76981\t+\n"
        "EG1_scaffold2\t117530\t117629\t4\tN\t100\tfragment\tyes\t\n"
        "EG1_scaffold2\t117630\t145298\t5\tW\tAADB02037554.1\t1\t27669\t+\n"
        "EG1_scaffold2\t145299\t145398\t6\tN\t100\tfragment\tyes\t\n"
        "EG1_scaffold2\t145399\t148350\t7\tW\tAADB02037555.1\t1\t2952\t+\n"
        "EG1_scaffold2\t148351\t148450\t8\tN\t100\tfragment\tyes\t\n"
        "EG1_scaffold2\t148451\t152599\t9\tW\tAADB02037556.1\t1\t4149\t-\n"
        "EG1_scaffold2\t152600\t152699\t10\tN\t100\tfragment\tyes\t\n";
    
    CNcbiIstrstream str(kData_Agp);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eAgp);
}


BOOST_AUTO_TEST_CASE(TestXml)
{
    const string kData_Xml =
        "<?xml version=\"1.0\"?>\n"
        "<!DOCTYPE Seq-entry PUBLIC \"-//NCBI//NCBI Seqset/EN\" "
        "\"https://www.ncbi.nlm.nih.gov/dtd/NCBI_Seqset.dtd\">\n"
        "<Seq-entry>\n";

    CNcbiIstrstream str(kData_Xml);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eXml);
}


BOOST_AUTO_TEST_CASE(TestWiggle)
{
    const string kData_Wiggle =
        "browser position chr19:59302001-59311000\n"
        "browser hide all\n"
        "browser pack refGene encodeRegions\n"
        "browser full altGraph\n"
        "#    300 base wide bar graph, autoScale is on by default == graphing\n"
        "#    limits will dynamically change to always show full range of data\n"
        "#    in viewing window, priority = 20 positions this as the second graph\n"
        "#    Note, zero-relative, half-open coordinate system in use for bed format\n"
        "track type=wiggle_0 name=\"Bed Format\" description=\"BED format\" \\\n"
        "    visibility=full color=200,100,0 altColor=0,100,200 priority=20\n"
        "chr19 59302000 59302300 -1.0\n"
        "chr19 59302300 59302600 -0.75\n"
        "chr19 59302600 59302900 -0.50\n"
        "chr19 59302900 59303200 -0.25\n"
        "chr19 59303200 59303500 0.0\n"
        "chr19 59303500 59303800 0.25\n"
        "chr19 59303800 59304100 0.50\n"
        "chr19 59304100 59304400 0.75\n"
        "chr19 59304400 59304700 1.00\n";

    CNcbiIstrstream str(kData_Wiggle);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eWiggle);
}


BOOST_AUTO_TEST_CASE(TestBed)
{
    const string kData_Bed =
        "track name=pairedReads description=\"Clone Paired Reads\" useScore=1\n"
        "chr22 1000 5000 cloneA 960 + 1000 5000 0 2 567,488, 0,3512\n"
        "chr22 2000 6000 cloneB 900 - 2000 6000 0 2 433,399, 0,3601\n";
    const string kData_Bed15 =
        "#chrom\tchromStart\tchromEnd\tname\tscore\tstrand\tthickStart\tthickEnd\t"
        "reserved\tblockCount\tblockSizes\tchromStarts\texpCount\texpIds\texpScores\n"
        "chr1\t159639972\t159640031\t2440848\t500\t-\t159639972\t159640031\t"
        "0\t1\t59,\t0,\t33\t0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,"
        "20,21,22,23,24,25,26,27,28,29,30,31,32,\t0.593000,1.196000,-0.190000,"
        "-1.088000,0.093000,-0.731000,0.130000,-0.008000,-1.087000,0.609000,"
        "-1.061000,-1.092000,0.807000,0.499000,-0.322000,-0.985000,0.309000,"
        "0.000000,0.812000,-0.457000,-0.560000,0.096000,0.186000,-1.092000,"
        "0.045000,0.573000,1.170000,1.336000,1.251000,1.919000,-0.056000,-0.189000,"
        "0.028000,\n"
        "chr1\t159640161\t159640190\t2440849\t500\t-\t159640161\t159640190\t"
        "0\t1\t29,\t0,\t33\t0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,"
        "19,20,21,22,23,24,25,26,27,28,29,30,31,32,    -0.906000,-1.247000,0.111000,"
        "-0.515000,-0.057000,-0.892000,0.167000,1.278000,0.051000,-0.596000,"
        "-0.251000,-0.826000,0.487000,0.714000,0.674000,1.046000,0.694000,0.236000,"
        "-0.718000,-1.196000,-1.274000,-1.278000,-1.055000,0.838000,-0.494000,"
        "1.137000,0.000000,0.690000,0.166000,-0.232000,0.174000,-1.253000,1.363000,\n"
        "chr1\t159640215\t159640242\t2440850\t500\t-\t159640215\t159640242\t0\t1\t"
        "27,\t0,\t33\t0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,"
        "23,24,25,26,27,28,29,30,31,32,    -0.465000,0.127000,1.215000,-0.073000,"
        "-0.465000,-0.141000,0.507000,-0.462000,-0.464000,0.570000,1.356000,"
        "0.559000,-0.459000,-0.464000,-0.458000,0.000000,0.322000,-0.454000,"
        "0.887000,-0.464000,1.196000,-0.463000,0.376000,-0.461000,0.547000,"
        "0.032000,-0.464000,0.066000,0.762000,-0.465000,-0.456000,0.919000,"
        "-0.464000,\n";
    {{
        CNcbiIstrstream str(kData_Bed);
        CFormatGuess guess(str);
        BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eBed);
    }}
    {{
        CNcbiIstrstream str(kData_Bed15);
        CFormatGuess guess(str);
        BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eBed15);
    }}
}


BOOST_AUTO_TEST_CASE(TestNewick)
{
    const string kData_Newick =
        "(Bovine:0.69395,(Hylobates:0.36079,(Pongo:0.33636,(G._Gorilla:0.17147, "
        "(P._paniscus:0.19268,H._sapiens:0.11927):0.08386):0.06124):0.15057):"
        "0.54939, Rodent:1.21460);";

    CNcbiIstrstream str(kData_Newick);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eNewick);
}


BOOST_AUTO_TEST_CASE(TestAlignment)
{
    const string kData_Alignment1 =
        "#NEXUS\n"
        "[TITLE: NoName]\n\n"
        "begin data;\n"
        "dimensions ntax=3 nchar=384;\n"
        "format interleave datatype=protein   gap=- symbols=\"FSTNKEYVQMCLAWPHDRIG\";\n\n"
        "matrix\n"
        "CYS1_DICDI          -----MKVIL LFVLAVFTVF VSS------- --------RG IPPEEQ---- \n"
        "ALEU_HORVU          MAHARVLLLA LAVLATAAVA VASSSSFADS NPIRPVTDRA ASTLESAVLG \n"
        "CATH_HUMAN          ------MWAT LPLLCAGAWL LGV------- -PVCGAAELS VNSLEK---- ";

    const string kData_Alignment2 =
        "CLUSTAL W (1.83) multiple sequence alignment\n\n"
        "aboA            -NLFV-ALYDFVASGDNTLSITKGEKLRV-------LGYNHNG-------EWCEA--QTK 42\n"
        "ycsB            KGVIY-ALWDYEPQNDDELPMKEGDCMTI-------IHREDEDEI-----EWWWA--RLN 45\n"
        "pht             -GYQYRALYDYKKEREEDIDLHLGDILTVNKGSLVALGFSDGQEARPEEIGWLNGYNETT 59\n";

    {{
        CNcbiIstrstream str(kData_Alignment1);
        CFormatGuess guess(str);
        BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eAlignment);
    }}
    {{
        CNcbiIstrstream str(kData_Alignment2);
        CFormatGuess guess(str);
        BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eAlignment);
    }}
}


BOOST_AUTO_TEST_CASE(TestCLUSTAL)
{
    const string kData_CLUSTAL1 =
    R"(gi|109899834|ref|YP_663089.1|       MRIKDCILAIDQGTTSTRAIIFAPDSSIIAVAQQEFTQHYPNDGWVEHDP
    gi|115525772|ref|YP_782683.1|       ---MSFVLAIDQGTTSSRAIVFRDDISIAAVAQQEFSQHFPASGWVEHEP
    gi|108762389|ref|YP_634888.1|       MPKAKYVLALDQGTTGTHVSILDTKLQVVGRSYKEFTQHFPKPSWVEHDL
                                            . ::.:* ***. :. :   . .:       :. :    . .*.: 
                                                                                      
    gi|109899834|ref|YP_663089.1|       EDIWSSTVVVCRQAISEAIAKGARIAAIGVTNQRETTVVWDRNTGQAIYN
    gi|115525772|ref|YP_782683.1|       EDIWSSTLATSRAAIEQAGLKASDIAAIGITNQRETVVLWDRVTGQAIHR
    gi|108762389|ref|YP_634888.1|       DEIWASSEWCIARALKSAGLRGKDIAAIGITNQRETTGLWMRGSGQPLSH)";

    const string kData_CLUSTAL2 =
    R"(gi|109899834|ref|YP_663089.1|       MRIKDCILAIDQGTTSTRAIIFAPDSSIIAVAQQEFTQHYPNDGWVEHDP
    gi|115525772|ref|YP_782683.1|       ---MSFVLAIDQGTTSSRAIVFRDDISIAAVAQQEFSQHFPASGWVEHEP
    gi|108762389|ref|YP_634888.1|       MPKAKYVLALDQGTTGTHVSILDTKLQVVGRSYKEFTQHFPKPSWVEHDL
                                                                                      
    gi|109899834|ref|YP_663089.1|       EDIWSSTVVVCRQAISEAIAKGARIAAIGVTNQRETTVVWDRNTGQAIYN
    gi|115525772|ref|YP_782683.1|       EDIWSSTLATSRAAIEQAGLKASDIAAIGITNQRETVVLWDRVTGQAIHR
    gi|108762389|ref|YP_634888.1|       DEIWASSEWCIARALKSAGLRGKDIAAIGITNQRETTGLWMRGSGQPLSH)";

    {{
        CNcbiIstrstream str(kData_CLUSTAL1);
        CFormatGuess guess(str);
        BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eAlignment);
    }}
    {{
        CNcbiIstrstream str(kData_CLUSTAL2);
        CFormatGuess guess(str);
        BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eAlignment);
    }}
}


BOOST_AUTO_TEST_CASE(TestNotCLUSTAL)
{
    const string kData_NotCLUSTAL1 =
    R"(gi|109899834|ref|YP_663089.1|       MRIKDCILAIDQGTTSTRAIIFAPDSSIIAVAQQEFTQHYPNDGWVEHDP
    gi|115525772|ref|YP_782683.1|       ---MSFVLAIDQGTTSSRAIVFRDDISIAAVAQQEFSQHFPASGWVEHEP
    gi|115525772|ref|YP_782683.1|       ---MSFVLAIDQGTTSSRAIVFRDDISIAAVAQQEFSQHFPASGWVEHEP
                                                                                      
    gi|109899834|ref|YP_663089.1|       EDIWSSTVVVCRQAISEAIAKGARIAAIGVTNQRETTVVWDRNTGQAIYN
    gi|115525772|ref|YP_782683.1|       EDIWSSTLATSRAAIEQAGLKASDIAAIGITNQRETVVLWDRVTGQAIHR
    gi|108762389|ref|YP_634888.1|       DEIWASSEWCIARALKSAGLRGKDIAAIGITNQRETTGLWMRGSGQPLSH)";

    const string kData_NotCLUSTAL2 =
    R"(CLUSTAL W (1.83) multiple sequence alignment
    "aboA            -NLFV-ALYDFVASGDNTLSITKGEKLRV-------LGYNHNG-------EWCEA--QTK 42
    "ycsB            KGVIY-ALWDYEPQNDDELPMKEGDCMTI-------IHREDEDEI-----EWWWA--RLN abc
    "pht             -GYQYRALYDYKKEREEDIDLHLGDILTVNKGSLVALGFSDGQEARPEEIGWLNGYNETT 59)";

    const string kData_NotCLUSTAL3 =
    R"(CLUSTAL W (1.83) multiple sequence alignment
    "aboA            -NLFV-ALYDFVASGDNTLSITKGEKLRV-------LGYNHNG-------EWCEA--QTK 42
    "ycsB            KGVIY-ALWDYEPQNDDELPMKEGDCMTI-------IHREDEDEI-----EWWWA--RLN 45
    "pht             -GYQYRALYDYKKEREEDIDLHLGDILTVNKGSLVALGFSDGQEARPEEIGWLNGNETT 59)";

    const string kData_NotCLUSTAL4 =
    R"(CLUSTAL W (1.83) multiple sequence alignment
    "aboA            -NLFV-ALYDFVASGDNTLSITKGEKLRV-------LGYNHNG-------EWCEA--QTK 42
    "ycsB            KGVIY-ALWDYEPQNDDELPMKEGDCMTI-------IHREDEDEI-----EWWWA--RLN 44
    "pht             -GYQYRALYDYKKEREEDIDLHLGDILTVNKGSLVALGFSDGQEARPEEIGWLNGYNETT 59)";

    {{
        CNcbiIstrstream str(kData_NotCLUSTAL1);
        CFormatGuess guess(str);
        BOOST_CHECK(guess.GuessFormat() != CFormatGuess::eAlignment);
    }}
    {{
        CNcbiIstrstream str(kData_NotCLUSTAL2);
        CFormatGuess guess(str);
        BOOST_CHECK(guess.GuessFormat() != CFormatGuess::eAlignment);
    }}
    {{
        CNcbiIstrstream str(kData_NotCLUSTAL3);
        CFormatGuess guess(str);
        BOOST_CHECK(guess.GuessFormat() != CFormatGuess::eAlignment);
    }}
    {{
        CNcbiIstrstream str(kData_NotCLUSTAL4);
        CFormatGuess guess(str);
        BOOST_CHECK(guess.GuessFormat() != CFormatGuess::eAlignment);
    }}
}


BOOST_AUTO_TEST_CASE(TestDistanceMatrix)
{
    const string kData_DistanceMatrix =
        "   14\n"
        "Mouse     \n"
        "Bovine      1.7043\n"
        "Lemur       2.0235  1.1901\n"
        "Tarsier     2.1378  1.3287  1.2905\n";
        //"Squir Monk  1.5232  1.2423  1.3199  1.7878\n"
        //"Jpn Macaq   1.8261  1.2508  1.3887  1.3137  1.0642\n";

    CNcbiIstrstream str(kData_DistanceMatrix);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eDistanceMatrix);
}


BOOST_AUTO_TEST_CASE(TestFlatFileSequence)
{
    const string kData_FlatFileSequence =
        "        1 ccagaatggt tactatggac atccgccaac catacaagct atggtgaaat gctttatcta\n"
        "       61 tctcattttt agtttcaaag cttttgttat aacacatgca aatccatatc cgtaaccaat\n"
        "      121 atccaatcgc ttgacatagt ctgatgaagt ttttggtagt taagataaag ctcgagactg\n"
        "      181 atatttcata tactggatga tttagggaaa cttgcattct attcatgaac gaatgagtca\n"
        "      241 atacgagaca caaccaagca tgcaaggagc tgtgagttga tgttctatgc tatttaagta\n";

    CNcbiIstrstream str(kData_FlatFileSequence);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eFlatFileSequence);

    // With hint it becomes table
    guess.GetFormatHints().AddDisabledFormat(CFormatGuess::eFlatFileSequence);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eTable);
}


BOOST_AUTO_TEST_CASE(TestFiveColFeatureTable)
{
    const string kData_FiveColFeatureTable =
        ">Feature Sc_16\n"
        "1    7000    REFERENCE\n"
        "            PubMed        8849441\n"
        "<1    1050    gene\n"
        "            gene        ATH1\n"
        "            locus_tag    YPR026W\n";

    CNcbiIstrstream str(kData_FiveColFeatureTable);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eFiveColFeatureTable);
}


BOOST_AUTO_TEST_CASE(TestSnpMarkers)
{
    const string kData_SnpMarkers =
        "rs10509971\t10\t114.981618\tA\n"
        "rs7580303\t2\t2.065249\tC\n"
        "rs7527281\t1\t213.591486\tC\n"
        "rs1358064\t7\t86.58632\tG\n"
        "rs4237768\t11\t5.963848\tG\n"
        "rs11771665\t7\t86.510866\tA\n"
        "rs6542185\t2\t114.423281\tT\n";

    CNcbiIstrstream str(kData_SnpMarkers);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eSnpMarkers);
}


BOOST_AUTO_TEST_CASE(TestFasta)
{
    const string kData_Fasta =
        ">gi|13990994|dbj|BAA33523.2| hedgehog [Homo sapiens]\n"
        "MSPARLRPRLHFCLVLLLLLVVPAAWGCGPGRVVGSRRRPPRKLVPLAYKQFSPNVPEKTLGASGRYEGK\n"
        "IARSSERFKELTPNYNPDIIFKDEENTGADRLMTQRCKDRLNSLAISVMNQWPGVKLRVTEGWDEDGHHS\n"
        "EESLHYEGRAVDITTSDRDRNKYGLLARLAVEAGFDWVYYESKAHVHCSVKSEHSAAAKTGGCFPAGAQV";

    CNcbiIstrstream str(kData_Fasta);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eFasta);
}


BOOST_AUTO_TEST_CASE(TestTextASN)
{
    const string kData_TextASN =
        "Seq-entry ::= set {\n"
        "  level 1 ,\n"
        "  class nuc-prot ,\n"
        "  descr {\n"
        "    pub {\n";

    CNcbiIstrstream str(kData_TextASN);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eTextASN);
}


/*
BOOST_AUTO_TEST_CASE(TestTaxplot)
{
    const char* kData_Taxplot =
        "Not implemented";

    // Taxplot format check is not implemented yet
    CNcbiIstrstream str(kData_Taxplot);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eTaxplot);
}
*/


BOOST_AUTO_TEST_CASE(TestPhrapAce)
{
    const string kData_PhrapAceOld =
        "DNA Contig35\n"
        "GATAAGataATAAtGGAAAATaGAAaccGGAaAaATaATAAaATaaTTTc\n"
        "aGATcGcTGaAGAaGAaGaGAAGaGAATAGcAGccCaATGTGAGAAGCTC\n"
        "GGcAAAAAAGGACTCGAAGaaGcGGGAAAGAGTCTgGAAGcTGCCATTCT";
    const string kData_PhrapAceNew =
        "AS 1 6\n\n"
        "CO Contig1 1222 6 0 U\n"
        "AGTTTTAGTTTTCCTCTGAAGCAAGCACACCTTCCCTTTCCCGTCTGTCTATCCATCCCT\n"
        "GACCCTGTTGTCTGTCTATCCCTGACCCCGTAGTCTCCTAAGTCGCCCCAGATTTTGTGA\n"
        "ACACCCTCTGGAACTAGAATCTAGTGGGCGGATGGACCATTTACTAGACGGAGGTAGAGG\n";
    {{
        CNcbiIstrstream str(kData_PhrapAceOld);
        CFormatGuess guess(str);
        BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::ePhrapAce);
    }}
    {{
        CNcbiIstrstream str(kData_PhrapAceNew);
        CFormatGuess guess(str);
        BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::ePhrapAce);
    }}
}


BOOST_AUTO_TEST_CASE(TestTable)
{
    const string kData_Table =
        " a         b         c         d         f         g         h\n"
        "-0.465000 -0.141000  0.507000 -0.462000 -0.464000  0.570000  1.356000\n"
        " 0.559000 -0.459000 -0.464000 -0.458000  0.000000  0.322000 -0.454000\n"
        " 0.887000 -0.464000  1.196000 -0.463000  0.376000 -0.461000  0.547000\n"
        " 0.032000 -0.464000  0.066000  0.762000 -0.465000 -0.456000  0.919000\n";

    CNcbiIstrstream str(kData_Table);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eTable);
}


BOOST_AUTO_TEST_CASE(TestTable2)
{
    const string kData_Table2 =
        "DNA        a         b         c         d         f         g\n"
        "-0.465000 -0.141000  0.507000 -0.462000 -0.464000  0.570000  1.356000\n"
        " 0.559000 -0.459000 -0.464000 -0.458000  0.000000  0.322000 -0.454000\n";

    CNcbiIstrstream str(kData_Table2);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eTable);
}


BOOST_AUTO_TEST_CASE(TestHgvs)
{
    const string kData_Hgvs =
        "NC_000023.9:g.107688969G>A\n"
        "NC_000023.9:g.107693786delG\n"
        "NG_008472.1:g.10295_10296insT\n";

    CNcbiIstrstream str(kData_Hgvs);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eHgvs);
}


BOOST_AUTO_TEST_CASE(TestNotHgvs)
{
    const string kData_NotHgvs = 
        "Seq-annot ::= {desc {name Primer.\n";

    CNcbiIstrstream str(kData_NotHgvs);
    CFormatGuess guess(str);
    BOOST_CHECK(guess.GuessFormat() != CFormatGuess::eHgvs);
}


BOOST_AUTO_TEST_CASE(TestZip)
{
    const string kData_Zip = 
        "\x50\x4b\x03\x04\x0a\x00\x00\x00\x00\x00\x41\x73\x58\x3f\xb3\xf1"
        "\x7f\x5a\x09\x00\x00\x00\x09\x00\x00\x00\x0c\x00\x15\x00\x7a\x69"
        "\x70\x5f\x74\x65\x73\x74\x2e\x74\x78\x74\x55\x54\x09\x00\x03\xba"
        "\xad\xa5\x4e\xba\xad\xa5\x4e\x55\x78\x04\x00\x12\x16\xff\x01\x7a"
        "\x69\x70\x20\x74\x65\x73\x74\x0a\x50\x4b\x01\x02\x17\x03\x0a\x00"
        "\x00\x00\x00\x00\x41\x73\x58\x3f\xb3\xf1\x7f\x5a\x09\x00\x00\x00"
        "\x09\x00\x00\x00\x0c\x00\x0d\x00\x00\x00\x00\x00\x01\x00\x00\x00"
        "\xa4\x81\x00\x00\x00\x00\x7a\x69\x70\x5f\x74\x65\x73\x74\x2e\x74"
        "\x78\x74\x55\x54\x05\x00\x03\xba\xad\xa5\x4e\x55\x78\x00\x00\x50"
        "\x4b\x05\x06\x00\x00\x00\x00\x01\x00\x01\x00\x47\x00\x00\x00\x48"
        "\x00\x00\x00\x00\x00\x00"s;
    
    CNcbiIstrstream str(kData_Zip);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eZip);
}


BOOST_AUTO_TEST_CASE(TestGZip)
{
    const string kData_GZip = 
        "\x1f\x8b\x08\x08\xba\xad\xa5\x4e\x00\x03\x7a\x69\x70\x5f\x74\x65"
        "\x73\x74\x2e\x74\x78\x74\x00\xab\xca\x2c\x50\x28\x49\x2d\x2e\xe1"
        "\x02\x00\xb3\xf1\x7f\x5a\x09\x00\x00\x00"s;
    
    CNcbiIstrstream str(kData_GZip);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eGZip);
}


BOOST_AUTO_TEST_CASE(TestBZip2)
{
    const string kData_BZip2 = 
        "\x42\x5a\x68\x39\x31\x41\x59\x26\x53\x59\x9a\x7c\x2e\xc9\x00\x00"
        "\x04\x51\x80\x00\x10\x40\x00\x02\x20\x4c\x10\x20\x00\x22\x00\xf2"
        "\x84\x30\x20\xea\x41\x5f\x17\x72\x45\x38\x50\x90\x9a\x7c\x2e\xc9"s;

    CNcbiIstrstream str(kData_BZip2);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eBZip2);
}


BOOST_AUTO_TEST_CASE(TestLzo)
{
    const string kData_Lzo = 
        "\x4c\x5a\x4f\x00\x0b\x00\x00\x60\x00\x00\x00\x18\x00\x00\x00\x25"
        "\x7a\x69\x70\x20\x74\x65\x73\x74\x00\x73\x6f\x75\x72\x63\x65\x20"
        "\x73\x74\x72\x3a\x11\x00\x00\x00\x00\x00\x00\x00"s;

    CNcbiIstrstream str(kData_Lzo);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eLzo);
}


BOOST_AUTO_TEST_CASE(TestSra)
{
    const string kData_Sra_BigEndian = 
        "NCBI.sra\x05\x03\x19\x88\x00\x00\x00\x01"s;
    const string kData_Sra_LittleEndian = 
        "NCBI.sra\x88\x19\x03\x05\x01\x00\x00\x00"s;
    {{
        CNcbiIstrstream str(kData_Sra_BigEndian);
        CFormatGuess guess(str);
        BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eSra);
    }}
    {{
        CNcbiIstrstream str(kData_Sra_LittleEndian);
        CFormatGuess guess(str);
        BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eSra);
    }}
}


/* note: format_guesser cannot currently detect BAM. all it does return "false".
BOOST_AUTO_TEST_CASE(TestBam)
{
    const string kData_Bam =
        "\x1f\x8b\x08\x04\x00\x00\x00\x00\x00\xff\x06\x00\x42\x43\x02\x00"
        "\x19\x64\xc4\xbd\x0b\x70\x2c\x6d\x5a\x1e\xd6\xbf\x8e\x8e\x8e\xa4"s;

    CNcbiIstrstream str(kData_Bam, sizeof(kData_Bam));
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eBam);
}
*/


BOOST_AUTO_TEST_CASE(TestJSON1)
{
    const string kData_JSON1 = 
        R"( { )"
        R"(    "Search": { )"
        R"(     "query\"_id": "lcl|1", )"
        R"(    "hits": [ )"
        R"(      { )"
        R"(        "num1": 1, )"
        R"(        "num2": -1.6E-05, )"
        R"(        "test true": true, )"
        R"(        "test false": false, )"
        R"(        "test null": null, )"
        R"(        "test open\\\" string )";

    CNcbiIstrstream str(kData_JSON1);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eJSON);
}


BOOST_AUTO_TEST_CASE(TestJSON2)
{
    const string kData_JSON2 = 
        R"( { )"
        R"(    "Search": { )"
        R"(     "truncated boolean": fal )";

    CNcbiIstrstream str(kData_JSON2);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eJSON);
}


BOOST_AUTO_TEST_CASE(TestJSON3)
{
    const string kData_JSON3 = 
        R"( { )"
        R"(    "Search": { )"
        R"(     "truncated number": 1.7E- )";

    CNcbiIstrstream str(kData_JSON3);
    CFormatGuess guess(str);
    BOOST_CHECK_EQUAL(guess.GuessFormat(), CFormatGuess::eJSON);
}


BOOST_AUTO_TEST_CASE(TestNotJSON1)
{
    // Missing starting brace
    const string kData_NotJSON1 = 
        R"(    "Search": { )"
        R"(    "query_id": "lcl|1", )"
        R"(    "hits": [ )"
        R"(      {  )"
        R"(        "num": 1, )"
        R"(        "test true": true, )"
        R"(        "test false": false, )"
        R"(        "test null": null, )"
        R"(        "test open string )";

    CNcbiIstrstream str(kData_NotJSON1);
    CFormatGuess guess(str);
    BOOST_CHECK(guess.GuessFormat() != CFormatGuess::eJSON);
}


BOOST_AUTO_TEST_CASE(TestNotJSON2)
{
    // Unexpected word    
    const string kData_NotJSON2 = 
        R"( { )"
        R"(    "Search": { )"
        R"(    "query_id": "lcl|1", )"
        R"(    "hits": [ )"
        R"(      {  )"
        R"(        "num": 1, )"
        R"(        "test true": true, )"
        R"(        "test false": false, )"
        R"(        unexpected, )"
        R"(        "test open string )";

    CNcbiIstrstream str(kData_NotJSON2);
    CFormatGuess guess(str);
    BOOST_CHECK(guess.GuessFormat() != CFormatGuess::eJSON);

    static const vector<string> strandSpecs = {
 	        "   ", "ss-", "ds-", "ms-"
 	    };
 	    static const auto strandSpecCount = strandSpecs.size();
 	
        for (auto i = 0; i < strandSpecCount; ++i) {
            if (NStr::StartsWith("", strandSpecs[i])) {
                return;
            }
        }


}
