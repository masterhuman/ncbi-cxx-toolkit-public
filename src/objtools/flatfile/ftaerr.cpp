#include <ncbi_pch.hpp>

#include <string.h>
#include <time.h>
#include <sstream>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>

#include "flatfile_message_reporter.hpp"
#include "ftaerr.hpp"
#include "ftacpp.hpp"

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "ftaerr.cpp"

#define MESSAGE_DIR "/am/ncbiapdata/errmsg"


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

struct FtaErrCode {
    const char *module;
    const char *fname;
    int  line;
};

struct FtaMsgModTagCtx {
    char                       *strsubtag;
    int                        intsubtag;
    int                        intseverity;
    FtaMsgModTagCtx*           next;
};

struct FtaMsgModTag {
    char*                  strtag;
    int                    inttag;
    FtaMsgModTagCtx        *bmctx;
    FtaMsgModTag*          next;
};

struct FtaMsgModFiles {
    string modname;      /* NCBI_MODULE or THIS_MODULE value */
    string filename;     /* Name with full path of .msg file */
    FtaMsgModTag             *bmmt;
    FtaMsgModFiles*          next;
};

//typedef struct bsv_msg_post {
struct FtaMsgPost {
    FILE               *lfd;            /* Opened logfile */
    char               *logfile;        /* Logfile full name */
    std::string         appname;
    char               *prefix_accession;
    char               *prefix_locus;
    char               *prefix_feature;
    bool               to_stderr;
    bool               show_msg_codeline;
    bool               show_log_codeline;
    bool               show_msg_codes;
    bool               show_log_codes;
    bool               hook_only;
    ErrSev             msglevel;        /* Filter out messages displaying on
                                           stderr only: ignode those with
                                           severity lower than msglevel */
    ErrSev             loglevel;        /* Filter out messages displaying in
                                           logfile only: ignode those with
                                           severity lower than msglevel */
    FtaMsgModFiles     *bmmf;

    FtaMsgPost() :
        lfd(NULL),
        logfile(NULL),
        prefix_accession(NULL),
        prefix_locus(NULL),
        prefix_feature(NULL),
        to_stderr(true),
        show_msg_codeline(false),
        show_log_codeline(false),
        show_msg_codes(false),
        show_log_codes(false),
        hook_only(false),
        msglevel(SEV_NONE),
        loglevel(SEV_NONE),
        bmmf(nullptr)
    {}

    virtual ~FtaMsgPost() {
        if (lfd) {
            fclose(lfd);
        }
        if (logfile) {
            MemFree(logfile);
        }
        if (prefix_locus) {
            MemFree(prefix_locus);
        }
        if (prefix_accession) {
            MemFree(prefix_accession);
        }
        if (prefix_feature) {
            MemFree(prefix_feature);    
        }
        delete bmmf;
    };
};

struct FtaPostInfo {
    const char *module;
    char *severity;
    char* strcode;
    char *strsubcode;
    char *buffer;
    const char *fname;
    int  sevcode;
    int  intcode;
    int  intsubcode;
    int  line;
};

const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

FtaPostInfo fpi;
thread_local unique_ptr<FtaMsgPost>  bmp;
FtaErrCode  fec;

/**********************************************************/
static int FtaStrSevToIntSev(const char *strsevcode)
{
    if(!strsevcode)
        return(-1);

    if(!strcmp(strsevcode, "SEV_INFO"))
        return(1);
    if(!strcmp(strsevcode, "SEV_WARNING"))
        return(2);
    if(!strcmp(strsevcode, "SEV_ERROR"))
        return(3);
    if(!strcmp(strsevcode, "SEV_REJECT"))
        return(4);
    if(!strcmp(strsevcode, "SEV_FATAL"))
        return(5);
    return(-1);
}

/**********************************************************/
void FtaErrGetMsgCodes(const char *module, int code, int subcode,
                       char **strcode, char **strsubcode,
                       int *sevcode)
{
    FtaMsgModTagCtx *bmctxp;
    FtaMsgModFiles  *bmmfp;
    FtaMsgModTag    *bmmtp;
    FILE            *fd;
    char*           val1;
    char            *val3;
    char            *p;
    char            *q;
    char            s[2048];
    char            ch;
    bool            got_mod;
    int             val2;

    if(!bmp)
        FtaErrInit();

    for(got_mod = false, bmmfp = bmp->bmmf; bmmfp; bmmfp = bmmfp->next)
    {
        if (bmmfp->modname != module) {
            continue;
        }

        got_mod = true;
        for(bmmtp = bmmfp->bmmt; bmmtp; bmmtp = bmmtp->next)
        {
            if(bmmtp->inttag != code)
                continue;

            *strcode = bmmtp->strtag;
            for(bmctxp = bmmtp->bmctx; bmctxp; bmctxp = bmctxp->next)
            {
                if(bmctxp->intsubtag != subcode)
                    continue;

                *strsubcode = bmctxp->strsubtag;
                *sevcode = bmctxp->intseverity;
                break;
            }
            break;
        }
        break;
    }

    if(got_mod)
        return;

    string curdir = CDir::GetCwd();
    string buf = curdir + "/" + module + ".msg";
    fd = fopen(buf.c_str(), "r");
    if(!fd)
    {
        buf = string(MESSAGE_DIR) + "/" + module + ".msg";
        fd = fopen(buf.c_str(), "r");
        if (!fd) {
            return;
        }
    }

    bmmfp = new FtaMsgModFiles;
    bmmfp->modname = module;
    bmmfp->filename = buf;

    if(bmp->bmmf)
        bmmfp->next = bmp->bmmf;
    bmp->bmmf = bmmfp;
    
    val2 = 0;
    val1 = NULL;
    val3 = NULL;
    bmmtp = NULL;
    while(fgets(s, 2047, fd))
    {
        if(s[0] != '$' || (s[1] != '^' && s[1] != '$'))
            continue;

        val2 = 0;
        val1 = NULL;
        val3 = NULL;

        for(p = s + 2; *p == ' ' || *p == '\t'; p++);
        for(q = p; *p && *p != ','; p++);
        if(*p != ',')
            continue;

        *p = '\0';
        val1 = (char *)MemNew(strlen(q) + 1);
        strcpy(val1, q);

        for(*p++ = ','; *p == ' ' || *p == '\t'; p++);
        for(q = p; *p >= '0' && *p <= '9'; p++);

        if(q == p)
        {
            MemFree(val1);
            continue;
        }

        ch = *p;
        *p = '\0';
        val2 = atoi(q);
        *p = ch;

        if(val2 < 1)
        {
            MemFree(val1);
            continue;
        }

        if(s[1] == '^' && ch == ',')
        {
            for(p++; *p == ' ' || *p == '\t'; p++);
            for(q = p;
                *p && *p != ' ' && *p != '\t' && *p != '\n' && *p != ','; p++);
            if(p > q)
            {
                ch = *p;
                *p = '\0';
                if(!strcmp(q, "SEV_INFO") || !strcmp(q, "SEV_WARNING") ||
                   !strcmp(q, "SEV_ERROR") || !strcmp(q, "SEV_REJECT") ||
                   !strcmp(q, "SEV_FATAL"))
                {
                    val3 = (char *)MemNew(strlen(q) + 1);
                    strcpy(val3, q);
                }
                *p = ch;
            }
        }

        if(s[1] == '$')
        {
            bmmtp = (FtaMsgModTag *) calloc(1, sizeof(FtaMsgModTag));

            if(bmmfp->bmmt)
                bmmtp->next = bmmfp->bmmt;
            bmmfp->bmmt = bmmtp;

            bmmtp->strtag = val1;
            bmmtp->inttag = val2;
            if(val2 == code && *strcode == NULL)
                *strcode = val1;

            if(val3)
            {
                MemFree(val3);
                val3 = NULL;
            }

            continue;
        }

        if(!bmmfp->bmmt || !bmmtp)
        {
            if(val1)
                MemFree(val1);
            if(val3)
                MemFree(val3);
            val2 = 0;
            continue;
        }

        bmctxp = (FtaMsgModTagCtx *) calloc(1, sizeof(FtaMsgModTagCtx));

        if(bmmtp->bmctx)
            bmctxp->next = bmmtp->bmctx;
        bmmtp->bmctx = bmctxp;

        bmctxp->strsubtag = val1;
        bmctxp->intsubtag = val2;
        bmctxp->intseverity = FtaStrSevToIntSev(val3);

        if(val3)
        {
            MemFree(val3);
            val3 = NULL;
        }

        if(val2 == subcode && *strsubcode == NULL && *strcode != NULL)
        {
            *strsubcode = val1;
            if(*sevcode < 0)
                *sevcode = bmctxp->intseverity;
        }
    }

    fclose(fd);
}

/**********************************************************/
static const char *FtaIntSevToStrSev(int sevcode)
{
    if(sevcode < 1 || sevcode > 5)
        return(NULL);

    if(sevcode == 1)
        return("NOTE");
    if(sevcode == 2)
        return("WARNING");
    if(sevcode == 3)
        return("ERROR");
    if(sevcode == 4)
        return("REJECT");
    return("FATAL ERROR");
}

/**********************************************************/
void FtaErrInit()
{
    if(bmp)
        return;

    bmp.reset(new FtaMsgPost());
    bmp->appname = CNcbiApplication::GetAppName();

    fec.module = NULL;
    fec.fname = NULL;
    bmp->hook_only = false;
    fec.line = -1;
}

/**********************************************************/
void FtaErrFini(void)
{
    if (bmp) {
        bmp.reset();
    }
}


/**********************************************************/
void FtaInstallPrefix(int prefix, const char *name, const char *location)
{
    if(name == NULL || *name == '\0')
        return;

    if((prefix & PREFIX_ACCESSION) == PREFIX_ACCESSION)
    {
        if(bmp->prefix_accession != NULL)
           MemFree(bmp->prefix_accession);
        bmp->prefix_accession = (char *)MemNew(strlen(name) + 1);
        strcpy(bmp->prefix_accession, name);
    }
    if((prefix & PREFIX_LOCUS) == PREFIX_LOCUS)
    {
        if(bmp->prefix_locus != NULL)
           MemFree(bmp->prefix_locus);
        bmp->prefix_locus = (char *)MemNew(strlen(name) + 1);
        strcpy(bmp->prefix_locus, name);
    }
    if((prefix & PREFIX_FEATURE) == PREFIX_FEATURE)
    {
        if(bmp->prefix_feature != NULL)
           MemFree(bmp->prefix_feature);
        bmp->prefix_feature = (char *)MemNew(160);
        strcpy(bmp->prefix_feature, "FEAT=");
        strncat(bmp->prefix_feature, name, 20);
        bmp->prefix_feature[24] = '\0';
        strcat(bmp->prefix_feature, "[");
        strncat(bmp->prefix_feature, location, 127);
        bmp->prefix_feature[152] = '\0';
        strcat(bmp->prefix_feature, "]");
    }
}

/**********************************************************/
void FtaDeletePrefix(int prefix)
{
    if((prefix & PREFIX_ACCESSION) == PREFIX_ACCESSION)
    {
        if(bmp->prefix_accession != NULL)
           MemFree(bmp->prefix_accession);
        bmp->prefix_accession = NULL;
    }
    if((prefix & PREFIX_LOCUS) == PREFIX_LOCUS)
    {
        if(bmp->prefix_locus != NULL)
           MemFree(bmp->prefix_locus);
        bmp->prefix_locus = NULL;
    }
    if((prefix & PREFIX_FEATURE) == PREFIX_FEATURE)
    {
        if(bmp->prefix_feature != NULL)
           MemFree(bmp->prefix_feature);
        bmp->prefix_feature = NULL;
    }
}

/**********************************************************/
bool ErrSetLog(const char *logfile)
{
    struct tm *tm;
    time_t    now;
    int       i;

    if(!logfile || !*logfile)
        return(false);

    if(!bmp)
        FtaErrInit();

    if(!bmp->logfile)
    {
        bmp->logfile = (char *)MemNew(strlen(logfile) + 1);
        strcpy(bmp->logfile, logfile);
    }

    if(!bmp->lfd && bmp->logfile)
    {
        time(&now);
        tm = localtime(&now);
        i = tm->tm_hour % 12;
        if(!i)
            i = 12;
            
        bmp->lfd = fopen(bmp->logfile, "a");
        fprintf(bmp->lfd,
                "\n========================[ %s %d, %d %2d:%02d %s ]========================\n",
                months[tm->tm_mon], tm->tm_mday, tm->tm_year + 1900,
                i, tm->tm_min, (tm->tm_hour >= 12) ? "PM" : "AM");
    }

    return(true);
}

/**********************************************************/
void ErrSetFatalLevel(ErrSev sev)
{
}

/**********************************************************/
void ErrSetOptFlags(int flags)
{
    if(!bmp)
        FtaErrInit();

    if((flags & EO_MSG_CODES) == EO_MSG_CODES)
        bmp->show_msg_codes = true;
    if((flags & EO_LOG_CODES) == EO_LOG_CODES)
        bmp->show_log_codes = true;
    if((flags & EO_MSG_FILELINE) == EO_MSG_FILELINE)
        bmp->show_msg_codeline = true;
    if((flags & EO_LOG_FILELINE) == EO_LOG_FILELINE)
        bmp->show_log_codeline = true;
}

/**********************************************************/
void ErrClear(void)
{
}

/**********************************************************/
void ErrLogPrintStr(const char *str)
{
    if(str == NULL || str[0] == '\0')
        return;

    if(!bmp)
        FtaErrInit();

    fprintf(bmp->lfd, "%s", str);
}

/**********************************************************/
ErrSev ErrSetLogLevel(ErrSev sev)
{
    ErrSev prev;

    if(!bmp)
        FtaErrInit();

    prev = bmp->loglevel;
    bmp->loglevel = sev;
    return(prev);
}

/**********************************************************/
ErrSev ErrSetMessageLevel(ErrSev sev)
{
    ErrSev prev;

    if(!bmp)
        FtaErrInit();

    prev = bmp->msglevel;
    bmp->msglevel = sev;
    return(prev);
}

/**********************************************************/
int Nlm_ErrSetContext(const char *module, const char *fname, int line)
{
    if(!bmp)
        FtaErrInit();

    fec.module = module;
    fec.fname = fname;
    fec.line = line;
    return(0);
}

/**********************************************************/
EDiagSev ErrCToCxxSeverity(int c_severity)
{
    EDiagSev cxx_severity;

    switch(c_severity)
    {
    case SEV_NONE:
        cxx_severity = eDiag_Trace;
        break;
    case SEV_INFO:
        cxx_severity = eDiag_Info;
        break;
    case SEV_WARNING:
        cxx_severity = eDiag_Warning;
        break;
    case SEV_ERROR:
        cxx_severity = eDiag_Error;
        break;
    case SEV_REJECT:
        cxx_severity = eDiag_Critical;
        break;
    case SEV_FATAL:
    default:
        cxx_severity = eDiag_Fatal;
        break;
    }
    return(cxx_severity);
}

/**********************************************************/
void Nlm_ErrPostEx(ErrSev sev, int lev1, int lev2, const char *fmt, ...)
{

    if(!bmp)
        FtaErrInit();


    if(fec.fname == NULL || fec.line < 0)
    {
        fec.module = NULL;
        fec.fname = NULL;
        fec.line = -1;
        return;
    }

    va_list args;
    char    buffer[1024];
    va_start(args, fmt);
    vsnprintf(buffer, 1024, fmt, args);
    va_end(args);

    fpi.buffer = buffer;
    fpi.sevcode = -1;
    fpi.strcode = NULL;
    fpi.strsubcode = NULL;

    fpi.intcode = lev1;
    fpi.intsubcode = lev2;

    fpi.line = fec.line;
    fpi.fname = fec.fname;
    fpi.module = fec.module;

    fec.module = NULL;
    fec.fname = NULL;
    fec.line = -1;

    if(fpi.module && *fpi.module)
        FtaErrGetMsgCodes(fpi.module, fpi.intcode, fpi.intsubcode,
                          &fpi.strcode, &fpi.strsubcode, &fpi.sevcode);
    else
        fpi.module = NULL;

    if(fpi.sevcode < 0)
        fpi.sevcode = (int) sev;
    fpi.severity = (char *) FtaIntSevToStrSev(fpi.sevcode);

    if(bmp->appname.empty())
        bmp->appname = CNcbiApplication::GetAppName();

    stringstream textStream;
    if (fpi.strcode) {
        textStream << "[" << fpi.strcode; 
        if (fpi.strsubcode) {
            textStream << "." << fpi.strsubcode;
        }
        textStream << "]  ";
    }

    if (bmp->show_log_codeline) {
        textStream << "{" << fpi.fname << ", line  " << fpi.line;
    }
    if (bmp->prefix_locus) {
        textStream << bmp->prefix_locus << ": ";
    }
    if (bmp->prefix_accession) {
        textStream << bmp->prefix_accession << ": ";
    }
    if (bmp->prefix_feature) {
        textStream << bmp->prefix_feature << " ";
    }
    textStream << fpi.buffer;

    static const map<ErrSev, EDiagSev> sSeverityMap
       =  {{SEV_NONE , eDiag_Trace},
          {SEV_INFO , eDiag_Info},
          {SEV_WARNING , eDiag_Warning},
          {SEV_ERROR , eDiag_Error},
          {SEV_REJECT , eDiag_Critical},
          {SEV_FATAL , eDiag_Fatal}}; 

    CFlatFileMessageReporter::GetInstance()
        .Report(fpi.module ? fpi.module : "", 
                sSeverityMap.at(static_cast<ErrSev>(fpi.sevcode)), 
                lev1, lev2, textStream.str());
}

/**********************************************************/
void Nlm_ErrPostStr(ErrSev sev, int lev1, int lev2, const char *str)
{
    Nlm_ErrPostEx(sev, lev1, lev2, str);
}

/**********************************************************/

END_NCBI_SCOPE
