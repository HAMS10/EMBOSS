/* @source eprimer32 application
** Picks PCR primers and hybridization oligos
**
** @author Copyright (C) Gary Williams (gwilliam@hgmp.mrc.ac.uk)
** 5 Nov 2001 - GWW - written
** @@
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
******************************************************************************/

#include "emboss.h"
#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#define fdopen _fdopen
#endif


/* estimate size of a sequence's output table */
#define TABLEGUESS 251




static FILE*  eprimer32_start_write(int fd);
static void   eprimer32_write(const AjPStr str, FILE *stream);
static void   eprimer32_end_write(FILE *stream);
static void   eprimer32_read(int fd, AjPStr * result);
static void   eprimer32_send_range(FILE * stream, const char * tag,
                                   const AjPRange value, 
                                   ajint begin);
static void   eprimer32_send_range_pair(FILE * stream, const char * tag,
                                        const AjPRange avalue,
                                        const AjPRange bvalue, 
                                        ajint begin);
static void eprimer32_send_range2(FILE * stream, const char * tag,
                                  const AjPRange value);
static void eprimer32_send_int(FILE * stream, const char * tag, ajint value);
static void eprimer32_send_float(FILE * stream, const char * tag, float value);
static void eprimer32_send_bool(FILE * stream, const char * tag, AjBool value);
static void eprimer32_send_string(FILE * stream, const char * tag,
                                  const AjPStr value);
static void eprimer32_send_stringC(FILE * stream, const char * tag,
                                   const char * value);
static void eprimer32_send_end(FILE * stream);
static void eprimer32_report (AjPFile outfile, const AjPStr output, 
                              ajint numreturn, ajint begin);
static void eprimer32_output_report(AjPFile outfile, const AjPTable table,
                                    ajint numreturn, ajint begin);
static const AjPStr eprimer32_tableget(const char * key1, ajint number,
                                       const char *key2,
                                       const AjPTable table);
static void eprimer32_write_primer(AjPFile outfile, const char *tag,
                                   const AjPStr pos,
                                   const AjPStr tm, const AjPStr gc,
                                   const AjPStr seq,
                                   AjBool rev, ajint begin);




/* @prog eprimer32 ************************************************************
**
** EMBOSS wrapper for the Whitehead's primer3 program
**
******************************************************************************/

int main(int argc, char **argv)
{
    /* Global details */
    AjBool explain_flag;
    AjBool file_flag;
    AjPStr* task;
    AjBool do_primer;
    AjBool do_hybrid;
    ajint num_return;

    /* "Sequence" Input Tags */
    AjPSeqall sequence;
    AjPRange included_region;
    AjPRange target;
    AjPRange excluded_region;
    AjPStr left_input;
    AjPStr right_input;

    /* Primer details */
    AjBool pick_anyway;
    AjPFile mispriming_library;
    float max_mispriming;
    float pair_max_mispriming;
    ajint gc_clamp;
    ajint opt_size;
    ajint min_size;
    ajint max_size;
    float opt_tm;
    float min_tm;
    float max_tm;
    float max_diff_tm;
    float opt_gc_percent;
    float min_gc;
    float max_gc;
    float salt_conc;
    float dna_conc;
    ajint num_ns_accepted;
    float self_any;
    float self_end;
    AjPStr tmfstr = NULL;
    AjPStr scstr  = NULL;
    ajint tm_formula;
    ajint salt_corr;
    ajint max_poly_x;

    /* Sequence Quality. These are not (yet) implemented */
    /*
       AjPFile sequence_quality;
       ajint	min_quality;
       ajint	min_end_quality;
       ajint	quality_range_min;
       ajint	quality_range_max;
       */

    /* Product details */
    ajint product_opt_size;
    AjPRange product_size_range;
    float product_opt_tm;
    float product_min_tm;
    float product_max_tm;

    /* Objective Function Penalty Weights for Primers */
    float max_end_stability;

    /* these are not (yet) implemented */
    /*
       float		inside_penalty;
       float		outside_penalty;
    */

    /* Primer penalties */
    /* these are not (yet) implemented */

    /* Internal Oligo "Sequence" Input Tags */
    AjPRange internal_oligo_excluded_region;
    AjPRange okleft_region;
    AjPRange okright_region;

    /* Internal Oligo "Global" Input Tags */
    AjPStr internal_oligo_input;
    ajint internal_oligo_opt_size;
    ajint internal_oligo_min_size;
    ajint internal_oligo_max_size;
    float internal_oligo_opt_tm;
    float internal_oligo_min_tm;
    float internal_oligo_max_tm;
    float internal_oligo_opt_gc_percent;
    float internal_oligo_min_gc;
    float internal_oligo_max_gc;
    float internal_oligo_salt_conc;
    float internal_oligo_dna_conc;
    float internal_oligo_self_any;
    float internal_oligo_self_end;
    ajint internal_oligo_max_poly_x;
    AjPFile internal_oligo_mishyb_library;
    float internal_oligo_max_mishyb;

    /*
       ajint		internal_oligo_min_quality;
    */

    /* Internal Oligo penalties */
    /* these are not (yet) implemented */

    /* EMBOSS-wrapper-specific stuff */
    AjPFile	outfile;

    /* other variables */
    AjPStr result = NULL;
    AjPStr strand = NULL;
    AjPStr substr = NULL;
    AjPSeq seq    = NULL;
    ajint begin   = 0;
    ajint end;
    FILE* stream;
    AjPStr taskstr  = NULL;
    const AjPStr program = NULL;

    /* pipe variables */

    int *pipeto;	  /* pipe to feed the exec'ed program input */
    int *pipefrom;	  /* pipe to get the exec'ed program output */

    embInit("eprimer32", argc, argv);

    /* Global details */
    explain_flag     = ajAcdGetBoolean("explainflag");
    file_flag        = ajAcdGetBoolean("fileflag");
    task             = ajAcdGetList("task");
    do_primer        = ajAcdGetToggle("primer");
    do_hybrid        = ajAcdGetToggle("hybridprobe");
    num_return       = ajAcdGetInt("numreturn");

    /* "Sequence" Input Tags */
    sequence        = ajAcdGetSeqall("sequence");
    included_region = ajAcdGetRange("includedregion");
    target          = ajAcdGetRange("targetregion");
    excluded_region = ajAcdGetRange("excludedregion");
    left_input      = ajAcdGetString("forwardinput");
    right_input     = ajAcdGetString("reverseinput");
    okleft_region   = ajAcdGetRange("okleftregion");
    okright_region  = ajAcdGetRange("okrightregion");

    /* Primer details */
    pick_anyway         = ajAcdGetBoolean("pickanyway");
    mispriming_library  = ajAcdGetInfile("mispriminglibraryfile");
    max_mispriming      = ajAcdGetFloat("maxmispriming");
    pair_max_mispriming = ajAcdGetFloat("pairmaxmispriming");
    gc_clamp            = ajAcdGetInt("gcclamp");
    opt_size            = ajAcdGetInt("optsize");
    min_size            = ajAcdGetInt("minsize");
    max_size            = ajAcdGetInt("maxsize");
    opt_tm              = ajAcdGetFloat("opttm");
    min_tm              = ajAcdGetFloat("mintm");
    max_tm              = ajAcdGetFloat("maxtm");
    max_diff_tm         = ajAcdGetFloat("maxdifftm");
    opt_gc_percent      = ajAcdGetFloat("ogcpercent");
    min_gc              = ajAcdGetFloat("mingc");
    max_gc              = ajAcdGetFloat("maxgc");
    salt_conc           = ajAcdGetFloat("saltconc");
    dna_conc            = ajAcdGetFloat("dnaconc");
    num_ns_accepted     = ajAcdGetInt("numnsaccepted");
    self_any            = ajAcdGetFloat("selfany");
    self_end            = ajAcdGetFloat("selfend");
    scstr               = ajAcdGetListSingle("scorrection");
    tmfstr              = ajAcdGetListSingle("tmformula");
    max_poly_x          = ajAcdGetInt("maxpolyx");

    ajStrToInt(scstr,&salt_corr);
    ajStrToInt(tmfstr,&tm_formula);
    
    AJCNEW0(pipeto,2);
    AJCNEW0(pipefrom,2);
    
    /* Sequence Quality */
    /* these are not (yet) implemented */
    /*
       sequence_quality  = ajAcdGetInfile("sequencequality");
       min_quality       = ajAcdGetInt("minquality");
       min_end_quality   = ajAcdGetInt("minendquality");
       quality_range_min = ajAcdGetInt("qualityrangemin");
       quality_range_max = ajAcdGetInt("qualityrangemax");
       */

    /* Product details */
    product_opt_size    = ajAcdGetInt("psizeopt");
    product_size_range  = ajAcdGetRange("prange");
    product_opt_tm      = ajAcdGetFloat("ptmopt");
    product_min_tm      = ajAcdGetFloat("ptmmin");
    product_max_tm      = ajAcdGetFloat("ptmmax");

    /* Objective Function Penalty Weights for Primers */
    max_end_stability   = ajAcdGetFloat("maxendstability");
    /* these are not (yet) implemented */
    /*
       inside_penalty      = ajAcdGetFloat("insidepenalty");
       outside_penalty     = ajAcdGetFloat("outsidepenalty");
    */

    /* Primer penalties */
    /* these are not (yet) implemented */

    /* Internal Oligo "Sequence" Input Tags */
    internal_oligo_excluded_region = ajAcdGetRange("oexcludedregion");
    internal_oligo_input           = ajAcdGetString("oligoinput");

    /* Internal Oligo "Global" Input Tags */
    internal_oligo_opt_size       = ajAcdGetInt("osizeopt");
    internal_oligo_min_size       = ajAcdGetInt("ominsize");
    internal_oligo_max_size       = ajAcdGetInt("omaxsize");
    internal_oligo_opt_tm         = ajAcdGetFloat("otmopt");
    internal_oligo_min_tm         = ajAcdGetFloat("otmmin");
    internal_oligo_max_tm         = ajAcdGetFloat("otmmax");
    internal_oligo_opt_gc_percent = ajAcdGetFloat("ogcopt");
    internal_oligo_min_gc         = ajAcdGetFloat("ogcmin");
    internal_oligo_max_gc         = ajAcdGetFloat("ogcmax");
    internal_oligo_salt_conc      = ajAcdGetFloat("osaltconc");
    internal_oligo_dna_conc       = ajAcdGetFloat("odnaconc");
    internal_oligo_self_any       = ajAcdGetFloat("oanyself");
    internal_oligo_self_end       = ajAcdGetFloat("oendself");
    internal_oligo_max_poly_x     = ajAcdGetInt("opolyxmax");
    internal_oligo_mishyb_library = ajAcdGetInfile("mishyblibraryfile");
    internal_oligo_max_mishyb     = ajAcdGetFloat("omishybmax");
    /*
       internal_oligo_min_quality    = ajAcdGetInt("oligominquality");
    */

    /* Internal Oligo penalties */
    /* these are not (yet) implemented */
    

    /* EMBOSS-wrapper-specific stuff */
    outfile = ajAcdGetOutfile("outfile");
    
    
    ajStrRemoveWhite(&left_input);
    ajStrRemoveWhite(&right_input);

    /*
    ** OK - we will now try to do a separate fork-exec for each sequence.
    */

    result = ajStrNew();

    while(ajSeqallNext(sequence, &seq))
    {
        program = ajAcdGetpathC("primer32_core");

        if(!ajSysExecRedirectC(ajStrGetPtr(program),&pipeto,&pipefrom))
            ajFatal("eprimer32: Could not exec primer32_core");
        
        stream = eprimer32_start_write(pipeto[1]);
        
        /* send primer3 Primer "Global" parameters */
        eprimer32_send_bool(stream, "PRIMER_EXPLAIN_FLAG", explain_flag);
        eprimer32_send_bool(stream, "P3_FILE_FLAG", file_flag);
        
        if(do_hybrid)
        {
            if(!ajStrCmpC(task[0], "1"))
                ajStrAssignC(&taskstr, "pick_pcr_primers_and_hyb_probe");
            else if(!ajStrCmpC(task[0], "2"))
                ajStrAssignC(&taskstr, "pick_left_only");
            else if(!ajStrCmpC(task[0], "3"))
                ajStrAssignC(&taskstr, "pick_right_only");
            else if(!ajStrCmpC(task[0], "4"))
                ajStrAssignC(&taskstr, "pick_hyb_probe_only");
        
            if (!do_primer)
                ajStrAssignC(&taskstr, "pick_hyb_probe_only");
        }
        else
        {
            if(!ajStrCmpC(task[0], "1"))
                ajStrAssignC(&taskstr, "pick_pcr_primers");
            else if(!ajStrCmpC(task[0], "2"))
                ajStrAssignC(&taskstr, "pick_left_only");
            else if(!ajStrCmpC(task[0], "3"))
                ajStrAssignC(&taskstr, "pick_right_only");
            else if(!ajStrCmpC(task[0], "4"))
                ajStrAssignC(&taskstr, "pick_hyb_probe_only");
        }
        
        eprimer32_send_string(stream, "PRIMER_TASK", taskstr);
        eprimer32_send_int(stream, "PRIMER_NUM_RETURN", num_return);
        eprimer32_send_int(stream, "PRIMER_FIRST_BASE_INDEX", 
                           1);
        eprimer32_send_bool(stream, "PRIMER_PICK_ANYWAY", pick_anyway);
        
        /* mispriming library may not have been specified */
        if(mispriming_library)
            eprimer32_send_stringC(stream, "PRIMER_MISPRIMING_LIBRARY",
                                   ajFileGetPrintnameC(mispriming_library));
    
        eprimer32_send_float(stream, "PRIMER_MAX_LIBRARY_MISPRIMING", 
                             max_mispriming);
        eprimer32_send_float(stream, "PRIMER_PAIR_MAX_LIBRARY_MISPRIMING",
                             pair_max_mispriming);
        eprimer32_send_int(stream, "PRIMER_GC_CLAMP", gc_clamp);
        eprimer32_send_int(stream, "PRIMER_OPT_SIZE", opt_size);
        eprimer32_send_int(stream, "PRIMER_MIN_SIZE", min_size);
        eprimer32_send_int(stream, "PRIMER_MAX_SIZE", max_size);
        eprimer32_send_float(stream, "PRIMER_OPT_TM", opt_tm);
        eprimer32_send_float(stream, "PRIMER_MIN_TM", min_tm);
        eprimer32_send_float(stream, "PRIMER_MAX_TM", max_tm);
        eprimer32_send_float(stream, "PRIMER_MAX_DIFF_TM", max_diff_tm);
        eprimer32_send_float(stream, "PRIMER_OPT_GC_PERCENT", 
                             opt_gc_percent);
        eprimer32_send_float(stream, "PRIMER_MIN_GC", min_gc);
        eprimer32_send_float(stream, "PRIMER_MAX_GC", max_gc);
        eprimer32_send_float(stream, "PRIMER_SALT_MONOVALENT", salt_conc);
        eprimer32_send_float(stream, "PRIMER_DNA_CONC", dna_conc);
        eprimer32_send_int(stream, "PRIMER_MAX_NS_ACCEPTED", 
                           num_ns_accepted);
        eprimer32_send_float(stream, "PRIMER_MAX_SELF_ANY", self_any);
        eprimer32_send_float(stream, "PRIMER_MAX_SELF_END", self_end);
        eprimer32_send_int(stream, "PRIMER_TM_FORMULA", tm_formula);
        eprimer32_send_int(stream, "PRIMER_SALT_CORRECTIONS", salt_corr);
        eprimer32_send_int(stream, "PRIMER_MAX_POLY_X", max_poly_x);
        eprimer32_send_int(stream, "PRIMER_PRODUCT_OPT_SIZE", 
                           product_opt_size);
        eprimer32_send_range2(stream, "PRIMER_PRODUCT_SIZE_RANGE",
                              product_size_range);
        eprimer32_send_float(stream, "PRIMER_PRODUCT_OPT_TM", 
                             product_opt_tm);
        eprimer32_send_float(stream, "PRIMER_PRODUCT_MIN_TM", 
                             product_min_tm);
        eprimer32_send_float(stream, "PRIMER_PRODUCT_MAX_TM", 
                             product_max_tm);
        eprimer32_send_float(stream, "PRIMER_MAX_END_STABILITY",
                             max_end_stability);
    
        /* send primer3 Internal Oligo "Global" parameters */
        eprimer32_send_int(stream, "PRIMER_INTERNAL_OPT_SIZE",
                           internal_oligo_opt_size);
        eprimer32_send_int(stream, "PRIMER_INTERNAL_MIN_SIZE",
                           internal_oligo_min_size);
        eprimer32_send_int(stream, "PRIMER_INTERNAL_MAX_SIZE",
                           internal_oligo_max_size);
        eprimer32_send_float(stream, "PRIMER_INTERNAL_OPT_TM",
                             internal_oligo_opt_tm);
        eprimer32_send_float(stream, "PRIMER_INTERNAL_MIN_TM",
                             internal_oligo_min_tm);
        eprimer32_send_float(stream, "PRIMER_INTERNAL_MAX_TM",
                             internal_oligo_max_tm);
        eprimer32_send_float(stream, "PRIMER_INTERNAL_OPT_GC_PERCENT",
                             internal_oligo_opt_gc_percent);
        eprimer32_send_float(stream, "PRIMER_INTERNAL_MIN_GC",
                             internal_oligo_min_gc);
        eprimer32_send_float(stream, "PRIMER_INTERNAL_MAX_GC",
                             internal_oligo_max_gc);
        eprimer32_send_float(stream, "PRIMER_INTERNAL_SALT_MONOVALENT",
                             internal_oligo_salt_conc);
        eprimer32_send_float(stream, "PRIMER_INTERNAL_DNA_CONC",
                             internal_oligo_dna_conc);
        eprimer32_send_float(stream, "PRIMER_INTERNAL_MAX_SELF_ANY",
                             internal_oligo_self_any);
        eprimer32_send_float(stream, "PRIMER_INTERNAL_MAX_SELF_END",
                             internal_oligo_self_end);
        eprimer32_send_int(stream, "PRIMER_INTERNAL_MAX_POLY_X",
                           internal_oligo_max_poly_x);

        /* 
        ** internal oligo mishybridising library may not have been
        ** specified 
        */
        if(internal_oligo_mishyb_library)
            eprimer32_send_stringC(stream,
                         "PRIMER_INTERNAL_MISHYB_LIBRARY",
                         ajFileGetPrintnameC(internal_oligo_mishyb_library));

        eprimer32_send_float(stream, "PRIMER_INTERNAL_MAX_LIBRARY_MISHYB",
                             internal_oligo_max_mishyb);
        
        
        /* 
        ** Start sequence-specific stuff 
        */

        begin = ajSeqallGetseqBegin(sequence) - 1;
        end   = ajSeqallGetseqEnd(sequence) - 1;

        strand = ajSeqGetSeqCopyS(seq);

        ajStrFmtUpper(&strand);
        ajStrAssignSubC(&substr,ajStrGetPtr(strand), begin, end);

        /* send flags to turn on using optimal product size */
        eprimer32_send_float(stream, "PRIMER_PAIR_WT_PRODUCT_SIZE_GT",
                             (float)0.05);
        eprimer32_send_float(stream, "PRIMER_PAIR_WT_PRODUCT_SIZE_LT",
                             (float)0.05);

        /* send primer3 Primer "Sequence" parameters */
        eprimer32_send_string(stream, "SEQUENCE_TEMPLATE", substr);

        /* if no ID name, use the USA */
        if(ajStrMatchC(ajSeqGetNameS(seq),""))
            eprimer32_send_string(stream, "SEQUENCE_ID",
                                  ajSeqGetUsaS(seq));
        else
            eprimer32_send_string(stream, "PRIMER_SEQUENCE_ID",
                                  ajSeqGetNameS(seq));

        eprimer32_send_range(stream, "SEQUENCE_INCLUDED_REGION",
                             included_region, begin);
        eprimer32_send_range(stream, "SEQUENCE_TARGET", target, begin);
        eprimer32_send_range(stream, "SEQUENCE_EXCLUDED_REGION",
                             excluded_region, begin);
        eprimer32_send_string(stream, "SEQUENCE_PRIMER", left_input);
        eprimer32_send_string(stream, "SEQUENCE_PRIMER_REVCOMP", right_input);

        /* send primer3 Internal Oligo "Sequence" parameters */
        eprimer32_send_range(stream,
                            "SEQUENCE_INTERNAL_EXCLUDED_REGION",
                            internal_oligo_excluded_region, begin);
        eprimer32_send_string(stream, "SEQUENCE_INTERNAL_OLIGO",
                             internal_oligo_input);
        eprimer32_send_range_pair(stream, "SEQUENCE_PRIMER_PAIR_OK_REGION_LIST",
                                  okleft_region, okright_region, begin);


        /* end the primer3 input sequence record with a '=' */
        eprimer32_send_end(stream);
        /* and close the ouput pipe stream */
        eprimer32_end_write(stream);
    
        /* read the primer3 output */
        eprimer32_read(pipefrom[0], &result);
            
        eprimer32_report(outfile, result, num_return, begin);
    
        ajStrSetClear(&result);

#ifndef WIN32
        close(pipeto[1]);
        close(pipefrom[0]);
#endif            
    }	/* end of sequence loop */


    ajStrDel(&result);
    ajSeqDel(&seq);
    ajStrDel(&strand);
    ajStrDel(&substr);
    ajFileClose(&outfile);
    ajStrDel(&taskstr);
    ajStrDel(&scstr);
    ajStrDel(&tmfstr);
    ajStrDelarray(&task);

    ajSeqallDel(&sequence);
    ajSeqDel(&seq);

    ajRangeDel(&included_region);
    ajRangeDel(&target);
    ajRangeDel(&excluded_region);
    ajRangeDel(&product_size_range);
    ajRangeDel(&internal_oligo_excluded_region);
    ajRangeDel(&okleft_region);
    ajRangeDel(&okright_region);

    ajStrDel(&left_input);
    ajStrDel(&right_input);
    ajStrDel(&internal_oligo_input);

    AJFREE(pipeto);
    AJFREE(pipefrom);
    
    ajFileClose(&mispriming_library);

    embExit();

    return 0;
}




/* @funcstatic eprimer32_read *************************************************
**
** Reads the output from primer3_core into a returned AjPStr until EOF
**
** @param [r] fd [int] file descriptor
** @param [u] result [AjPStr*] Returned string
** @return [void]
**
******************************************************************************/

static void eprimer32_read(int fd, AjPStr* result)
{
    FILE *stream;
    int ch;

    ajDebug("reading primer3_core output\n");

    if((stream = fdopen(fd, "r")) == NULL)
        ajFatal("fdopen() read error");

    while((ch = getc( stream )) != EOF)
        ajStrAppendK(result, ch);

    ajDebug("primer3_core output:\n%S\n", *result);


    fclose(stream);
    ajDebug("reading done\n");

    return;
}




/* @funcstatic eprimer32_send_end *********************************************
**
** Writes the end-of-input flag '=' to the input stream of primer3_core
**
** @param [u] stream [FILE *] File handle
** @return [void]
**
******************************************************************************/

static void eprimer32_send_end(FILE * stream)
{
    fputs( "=\n", stream );

    return;
}




/* @funcstatic eprimer32_send_range *******************************************
**
** Write range data to primer3_core as 'start,length start2,length2,etc'
**
** @param [u] stream [FILE *] File handle
** @param [r] tag [const char *] Tag of primer3 data type
** @param [r] value [const AjPRange] Ranges to write
** @param [r] begin [ajint] Start position of subsequence (-sbegin)
** @return [void]
**
******************************************************************************/

static void eprimer32_send_range(FILE * stream, const char * tag,
                                 const AjPRange value, 
                                 ajint begin)
{
    AjPStr str;
    ajuint n;
    ajuint start;
    ajuint end;

    str = ajStrNew();

    if(ajRangeGetSize(value))
    {
        ajFmtPrintS(&str, "%s=", tag);
        eprimer32_write(str, stream);
        ajStrSetClear(&str);

        for(n=0; n < ajRangeGetSize(value); n++)
        {
            ajRangeElementGetValues(value, n, &start, &end);
            start -= begin;
            end   -= begin;
            ajFmtPrintS(&str, "%d,%d ", start, end-start+1);
            eprimer32_write(str, stream);
            ajStrSetClear(&str);
        }

        ajFmtPrintS(&str, "\n");
        eprimer32_write(str, stream);
    }

    ajStrDel(&str);

    return;
}




/* @funcstatic eprimer32_send_range2 ******************************************
**
** Write alternate display of ranges as 'a-b c-d' to primer3_core
**
** @param [u] stream [FILE *] File handle
** @param [r] tag [const char *] Tag of primer3 data type
** @param [r] value [const AjPRange] Ranges to write
** @return [void]
**
******************************************************************************/

static void eprimer32_send_range2(FILE * stream, const char * tag,
                                  const AjPRange value)
{
    AjPStr str;
    ajuint n;
    ajuint start;
    ajuint end;

    str=ajStrNew();

    if(ajRangeGetSize(value))
    {
        ajFmtPrintS(&str, "%s=", tag);
        eprimer32_write(str, stream);
        ajStrSetClear(&str);

        for(n=0; n < ajRangeGetSize(value); n++)
        {
            ajRangeElementGetValues(value, n, &start, &end);
            ajFmtPrintS(&str, "%d-%d ", start, end);
            eprimer32_write(str, stream);
            ajStrSetClear(&str);
        }

        ajFmtPrintS(&str, "\n");
        eprimer32_write(str, stream);
    }

    ajStrDel(&str);

    return;
}




/* @funcstatic eprimer32_send_range_pair **************************************
**
** Write range data to primer3_core as 'start,length start2,length2,etc'
**
** @param [u] stream [FILE *] File handle
** @param [r] tag [const char *] Tag of primer3 data type
** @param [r] avalue [const AjPRange] First ranges to write
** @param [r] bvalue [const AjPRange] Second ranges to write
** @param [r] begin [ajint] Start position of subsequence (-sbegin)
** @return [void]
**
******************************************************************************/

static void eprimer32_send_range_pair(FILE * stream, const char * tag,
                                      const AjPRange avalue, 
                                      const AjPRange bvalue, 
                                      ajint begin)
{
    AjPStr str;
    ajuint n = 0;
    ajuint start;
    ajuint end;
    ajuint asize;
    ajuint bsize;

    asize = ajRangeGetSize(avalue);
    bsize = ajRangeGetSize(bvalue);

    if(asize || bsize)
    {
        str = ajStrNew();

        ajFmtPrintS(&str, "%s=", tag);

        for(n=0; n < asize; n++)
        {
            if(n)
                ajFmtPrintAppS(&str,";");
            ajRangeElementGetValues(avalue, n, &start, &end);
            start -= begin;
            end   -= begin;
            ajFmtPrintAppS(&str, "%u,%u", start, end-start+1);
            if(n < bsize)
            {
                ajRangeElementGetValues(bvalue, n, &start, &end);
                start -= begin;
                end   -= begin;
                ajFmtPrintAppS(&str, ",%u,%u", start, end-start+1);
            }
            else 
            {
                ajFmtPrintAppS(&str, ",,");
            }
        }

        for(n=asize; n < bsize; n++)
        {
            if(n)
                ajFmtPrintAppS(&str,";");
            ajFmtPrintAppS(&str, ",,");
            ajRangeElementGetValues(bvalue, n, &start, &end);
            start -= begin;
            end   -= begin;
            ajFmtPrintAppS(&str, ",%u,%u", start, end-start+1);
        }

        if(ajStrGetLen(str))
        {
            ajFmtPrintAppS(&str, "\n");
            eprimer32_write(str, stream);
        }

        ajStrDel(&str);
    }

    return;
}




/* @funcstatic eprimer32_send_int *********************************************
**
** Write integer to primer32_core
**
** @param [u] stream [FILE *] File handle
** @param [r] tag [const char *] Tag of primer3 data type
** @param [r] value [ajint] Integer value to write
** @return [void]
**
******************************************************************************/

static void eprimer32_send_int(FILE * stream, const char * tag, ajint value)
{
    AjPStr str;

    str = ajStrNew();

    ajFmtPrintS(&str, "%s=%d\n", tag, value);
    eprimer32_write(str, stream);

    ajStrDel(&str);

    return;
}




/* @funcstatic eprimer32_send_float *******************************************
**
** Write float to primer32_core
**
** @param [u] stream [FILE *] File handle
** @param [r] tag [const char *] Tag of primer3 data type
** @param [r] value [float] Float value to write
** @return [void]
**
******************************************************************************/

static void eprimer32_send_float(FILE * stream, const char * tag, float value)
{
    AjPStr str;

    str = ajStrNew();

    ajFmtPrintS(&str, "%s=%f\n", tag, value);
    eprimer32_write(str, stream);

    ajStrDel(&str);

    return;
}




/* @funcstatic eprimer32_send_bool ********************************************
**
** Write boolean to primer32_core
**
** @param [u] stream [FILE *] File handle
** @param [r] tag [const char *] Tag of primer3 data type
** @param [r] value [AjBool] Boolean value to write
** @return [void]
**
******************************************************************************/

static void eprimer32_send_bool(FILE * stream, const char * tag, AjBool value)
{
    AjPStr str;

    str = ajStrNew();

    ajFmtPrintS(&str, "%s=%d\n", tag, value?1:0);
    eprimer32_write(str, stream);

    ajStrDel(&str);

    return;
}




/* @funcstatic eprimer32_send_string ******************************************
**
** Write string to primer32_core
**
**
** @param [u] stream [FILE *] File handle
** @param [r] tag [const char *] Tag of primer3 data type
** @param [r] value [const AjPStr] String value to write
** @return [void]
**
******************************************************************************/

static void eprimer32_send_string(FILE * stream, const char * tag,
                                  const AjPStr value)
{
    AjPStr str;

    str = ajStrNew();

    if(ajStrGetLen(value))
    {
        ajFmtPrintS(&str, "%s=%S\n", tag, value);
        eprimer32_write(str, stream);
    }

    ajStrDel(&str);

    return;
}




/* @funcstatic eprimer32_send_stringC *****************************************
**
** Write char * to primer32_core
**
** @param [u] stream [FILE*] File handle
** @param [r] tag [const char*] Tag of primer3 data type
** @param [r] value [const char*] Char * value to write
** @return [void]
**
******************************************************************************/

static void eprimer32_send_stringC(FILE *stream, const char *tag,
                                   const char *value)
{
    AjPStr str;

    str = ajStrNew();

    if(strlen(value))
    {
        ajFmtPrintS(&str, "%s=%s\n", tag, value);
        eprimer32_write(str, stream);
    }

    ajStrDel(&str);

    return;
}




/* @funcstatic eprimer32_start_write ******************************************
**
** Open a file descriptor as a stream to pipe to primer32_core
**
** @param [r] fd [int] File descriptor
** @return [FILE*] File stream
**
******************************************************************************/

static FILE* eprimer32_start_write(int fd)
{
    FILE *stream;

    ajDebug( "start writing\n");
    if((stream = fdopen( fd, "w" )) == NULL)
        ajFatal("Couldn't open pipe with fdopen()");

    return stream;
}




/* @funcstatic eprimer32_write ************************************************
**
** Write a tag=value AjPStr to the primer32_core input stream
**
** @param [r] str [const AjPStr] Input string
** @param [u] stream [FILE*] Stream piped to primer3_core
** @return [void]
**
******************************************************************************/

static void eprimer32_write(const AjPStr str, FILE *stream)
{
    fputs(ajStrGetPtr(str), stream);
    ajDebug("eprimer32_write '%S'\n", str);

    return;
}




/* @funcstatic eprimer32_end_write ********************************************
**
** Close the stream piping in to primer32_core
**
** @param [u] stream [FILE *] Stream
** @return [void]
**
******************************************************************************/

static void eprimer32_end_write(FILE *stream)
{
    ajDebug("Closing output pipe stream\n");
    fclose(stream);

    return;
}




/* @funcstatic eprimer32_report ***********************************************
**
** Read output of primer32_core into a temporary table of tag/value results
**
** @param [u] outfile [AjPFile] Report outfile
** @param [r] output [const AjPStr] Output from primer3_core
** @param [r] numreturn [ajint] Number of results to return for each sequence
** @param [r] begin [ajint] Start position of subsequence (-sbegin)
** @return [void]
**
******************************************************************************/

static void eprimer32_report(AjPFile outfile, const AjPStr output, 
                             ajint numreturn, ajint begin)
{
    AjPStr line = NULL;
    AjPStrTok linetokenhandle;
    char eol[] = "\n\r";
    AjPStrTok keytokenhandle;
    char equals[] = "=";
    AjPStr key   = NULL;
    AjPStr value = NULL;
    AjBool gotsequenceid = AJFALSE;
    AjPTable table;

    linetokenhandle = ajStrTokenNewC(output, eol);

    /* get next line of relevant results */
    while(ajStrTokenNextParseC(linetokenhandle, eol, &line))
    {
        if(!gotsequenceid)
        {
            /*
            ** Att the start of another sequence's results?
            ** Start storing the results in the table.
            */

            if(ajStrCmpLenC(line, "PRIMER_SEQUENCE_ID=", 19) == 0)
            {
                gotsequenceid = AJTRUE;
                table = ajTablestrNew(TABLEGUESS);

            }
            else
                continue;
        }
        else
        {
            /*
            ** At the end of this sequence? - marked by a '=' in the primer3
            ** output - then output the results.
            */
            if(ajStrCmpC(line, "=") == 0)
            {
                gotsequenceid = AJFALSE;
                eprimer32_output_report(outfile, table, numreturn, begin);
                ajTablestrFree(&table);
                continue;
            }
        }

        /*
        ** store key and value in table and parse values
        ** when have all of the sequence
        ** results in the table because the LEFT, RIGHT
        ** and INTERNAL results for each
        ** resulting primer are interleaved
        */

        keytokenhandle = ajStrTokenNewC(line, equals);

        key = ajStrNew();
        ajStrTokenNextParse(keytokenhandle, &key);

        value = ajStrNew();
        ajStrTokenNextParse(keytokenhandle, &value);

        ajDebug("key=%S\tvalue=%S\n", key, value);

        ajTablePut(table,(void *)key, (void *)value);

        ajStrTokenDel(&keytokenhandle);
    }

    ajStrDel(&line);
    ajStrTokenDel(&linetokenhandle);
    ajTablestrFree(&table);

    return;
}




/* @funcstatic eprimer32_output_report ****************************************
**
** Read the results out of the tag/value table and write to report
**
** @param [u] outfile [AjPFile] Report outfile
** @param [r] table [const AjPTable] Table of tag/value result pairs
** @param [r] numreturn [ajint] Number of results to return for each sequence
** @param [r] begin [ajint] Start position of subsequence (-sbegin)
** @return [void]
**
******************************************************************************/

static void eprimer32_output_report(AjPFile outfile, const AjPTable table,
                                    ajint numreturn, ajint begin)
{
    AjPStr key   = NULL;
    AjPStr explain    = NULL;
    AjPStr explainstr = NULL;
    const AjPStr error = NULL;		/*  value from table - do not delete */
    const AjPStr seqid      = NULL;
    ajint i;
    const AjPStr size = NULL;
    const AjPStr seq  = NULL;
    const AjPStr pos  = NULL;
    const AjPStr tm   = NULL;
    const AjPStr gc   = NULL;

     /*
     ** Typical primer3 output looks like:
     ** PRIMER_SEQUENCE_ID=em-id:HSFAU
     ** PRIMER_PAIR_PENALTY=0.1974
     ** PRIMER_LEFT_PENALTY=0.007073
     ** PRIMER_RIGHT_PENALTY=0.190341
     ** PRIMER_INTERNAL_OLIGO_PENALTY=0.132570
     ** PRIMER_LEFT_SEQUENCE=AGTCGCCAATATGCAGCTCT
     ** PRIMER_RIGHT_SEQUENCE=GGAGCACGACTTGATCTTCC
     ** PRIMER_INTERNAL_OLIGO_SEQUENCE=GGAGCTACACACCTTCGAGG
     ** PRIMER_LEFT=47,20
     ** PRIMER_RIGHT=183,20
     ** PRIMER_INTERNAL_OLIGO=80,20
     ** PRIMER_LEFT_TM=60.007
     ** PRIMER_RIGHT_TM=59.810
     ** PRIMER_INTERNAL_OLIGO_TM=59.867
     ** PRIMER_LEFT_GC_PERCENT=50.000
     ** PRIMER_RIGHT_GC_PERCENT=55.000
     ** PRIMER_INTERNAL_OLIGO_GC_PERCENT=60.000
     ** PRIMER_LEFT_SELF_ANY=4.00
     ** PRIMER_RIGHT_SELF_ANY=5.00
     ** PRIMER_INTERNAL_OLIGO_SELF_ANY=4.00
     ** PRIMER_LEFT_SELF_END=2.00
     ** PRIMER_RIGHT_SELF_END=1.00
     ** PRIMER_INTERNAL_OLIGO_SELF_END=4.00
     ** PRIMER_LEFT_END_STABILITY=7.9000
     ** PRIMER_RIGHT_END_STABILITY=8.2000
     ** PRIMER_PAIR_COMPL_ANY=5.00
     ** PRIMER_PAIR_COMPL_END=1.00
     ** PRIMER_PRODUCT_SIZE=137
     */
  
    /* check for errors */
    ajStrAssignC(&key, "PRIMER_ERROR");
    error = ajTableFetchS(table, key);
    if(error != NULL)
        ajErr("%S", error);

    ajStrAssignC(&key, "PRIMER_WARNING");
    error = ajTableFetchS(table, key);
    if(error != NULL)
        ajWarn("%S", error);

  
    /* get the sequence id */
    ajStrAssignC(&key, "PRIMER_SEQUENCE_ID");
    seqid = ajTableFetchS(table, key);
    ajFmtPrintF(outfile, "\n# EPRIMER32 RESULTS FOR %S\n\n", seqid);
  
    /* get information on the analysis */
    ajStrAssignC(&key, "PRIMER_LEFT_EXPLAIN");
    explain = ajTableFetchmodS(table, key);

    if(explain != NULL)
    {
        ajStrAssignS(&explainstr, explain);
        ajStrExchangeCC(&explainstr, ",", "\n#");
        ajFmtPrintF(outfile, "# FORWARD PRIMER STATISTICS:\n# %S\n\n",
                    explainstr);
    }
    ajStrAssignC(&key, "PRIMER_RIGHT_EXPLAIN");
    explain = ajTableFetchmodS(table, key);

    if(explain != NULL)
    {
        ajStrAssignS(&explainstr, explain);
        ajStrExchangeCC(&explainstr, ",", "\n#");
        ajFmtPrintF(outfile, "# REVERSE PRIMER STATISTICS:\n# %S\n\n",
                    explainstr);
    }
    ajStrAssignC(&key, "PRIMER_PAIR_EXPLAIN");
    explain = ajTableFetchmodS(table, key);

    if(explain != NULL)
    {
        ajStrAssignS(&explainstr, explain);
        ajStrExchangeCC(&explainstr, ",", "\n#");
        ajFmtPrintF(outfile, "# PRIMER PAIR STATISTICS:\n# %S\n\n",
                    explainstr);
    }
    ajStrAssignC(&key, "PRIMER_INTERNAL_EXPLAIN");
    explain = ajTableFetchmodS(table, key);

    if(explain != NULL)
    {
        ajStrAssignS(&explainstr, explain);
        ajStrExchangeCC(&explainstr, ",", "\n#");
        ajFmtPrintF(outfile, "# INTERNAL OLIGO STATISTICS:\n# %S\n\n",
                    explainstr);
    }
  
    /* table header */
    ajFmtPrintF(outfile,"#                      Start  Len   Tm     "
                "GC%%   Sequence\n\n");
  
    /* get the results */
    for(i=0; i <= numreturn; i++)
    {
        /* product data */
        size = eprimer32_tableget("PRIMER_PAIR", i, "PRODUCT_SIZE", table);
        if(size != NULL)
            ajFmtPrintF(outfile, "%4d PRODUCT SIZE: %S\n",
                        i+1, size);

        /* left primer data */
        tm = eprimer32_tableget("PRIMER_LEFT", i, "TM", table);
        gc = eprimer32_tableget("PRIMER_LEFT", i, "GC_PERCENT", table);
        pos = eprimer32_tableget("PRIMER_LEFT", i, "", table);
        seq = eprimer32_tableget("PRIMER_LEFT", i, "SEQUENCE", table);
        eprimer32_write_primer(outfile, "FORWARD PRIMER",
                               pos, tm, gc, seq, AJFALSE, begin);


        /* right primer data */
        tm = eprimer32_tableget("PRIMER_RIGHT", i, "TM", table);
        gc = eprimer32_tableget("PRIMER_RIGHT", i, "GC_PERCENT", table);
        pos = eprimer32_tableget("PRIMER_RIGHT", i, "", table);
        seq = eprimer32_tableget("PRIMER_RIGHT", i, "SEQUENCE", table);
        eprimer32_write_primer(outfile, "REVERSE PRIMER", 
                               pos, tm, gc, seq, AJTRUE, begin);


        /* internal oligo data */

        tm = eprimer32_tableget("PRIMER_INTERNAL_OLIGO", i, "TM", table);
        gc = eprimer32_tableget("PRIMER_INTERNAL_OLIGO", i, "GC_PERCENT",
                                table);
        pos = eprimer32_tableget("PRIMER_INTERNAL_OLIGO", i, "", table);
        seq = eprimer32_tableget("PRIMER_INTERNAL_OLIGO", i, "SEQUENCE", table);
        eprimer32_write_primer(outfile, "INTERNAL OLIGO",
                               pos, tm, gc, seq, AJFALSE, begin);

        ajFmtPrintF(outfile, "\n");

    }
  

    ajStrDel(&key);
    ajStrDel(&explainstr);
 
    return;
}




/* @funcstatic eprimer32_tableget *********************************************
**
** Read the results out of the tag/value table
**
** @param [r] key1 [const char *] First half of table key base string
** @param [r] number [ajint] Table key numeric part
** @param [r] key2 [const char *] Second half of table key base string
**                                (minus '_')
** @param [r] table [const AjPTable] Table of tag/value result pairs
** @return [const AjPStr] Table value
**
******************************************************************************/

static const AjPStr eprimer32_tableget(const char *key1, ajint number,
                                 const char *key2,
                                 const AjPTable table)
{
    AjPStr fullkey = NULL;
    AjPStr keynum  = NULL;
    const AjPStr value   = NULL;

    ajStrAssignC(&fullkey, key1);

    ajStrAppendC(&fullkey, "_");
    ajStrFromInt(&keynum, number);
    ajStrAppendS(&fullkey, keynum);

    if(strcmp(key2, ""))
        ajStrAppendC(&fullkey, "_");

    ajStrAppendC(&fullkey, key2);
    ajDebug("Constructed key=%S\n", fullkey);
    value = ajTableFetchS(table, fullkey);


    ajStrDel(&fullkey);
    ajStrDel(&keynum);

    return value;
}




/* @funcstatic eprimer32_write_primer *****************************************
**
** Write out one primer or oligo line to the output file
**
** @param [u] outfile [AjPFile] Report outfile
** @param [r] tag [const char *] Tag on output line
** @param [r] pos [const AjPStr] Start and length string
** @param [r] tm [const AjPStr] Tm of primer
** @param [r] gc [const AjPStr] GC% of primer
** @param [r] seq [const AjPStr] Sequence of primer
** @param [r] rev [AjBool] Sequence is the reverse-complement primer
** @param [r] begin [ajint] Start position of subsequence (-sbegin)
** @return [void]
**
******************************************************************************/

static void eprimer32_write_primer(AjPFile outfile, const char *tag,
                                   const AjPStr pos,
                                   const AjPStr tm, const AjPStr gc,
                                   const AjPStr seq,
                                   AjBool rev, ajint begin)
{
    ajint startint;
    ajint lenint;
    float tmfloat;
    float gcfloat;
    AjPStr start = NULL;
    AjPStr lenstr = NULL;
    ajlong comma;

    if(ajStrGetLen(pos))
    {
        ajStrToFloat(tm, &tmfloat);
        ajStrToFloat(gc, &gcfloat);
        comma = ajStrFindC(pos, ",");
        ajStrAssignS(&start, pos);
        ajStrCutRange(&start, comma, ajStrGetLen(start)-1);
        ajStrToInt(start, &startint);
        startint += begin;
        ajStrAssignS(&lenstr, pos);
        ajStrCutRange(&lenstr, 0, comma);
        ajStrToInt(lenstr, &lenint);
        if(rev)
            startint = startint - lenint + 1;

        ajFmtPrintF(outfile, "     %s  %6d %4d  %2.2f  %2.2f  %S\n\n",
                    tag, startint, lenint, tmfloat, gcfloat, seq);
    }


    ajStrDel(&start);
    ajStrDel(&lenstr);

    return;
}
