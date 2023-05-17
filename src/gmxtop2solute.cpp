const char * software_name = "gmxtop2solute";
const char * software_version = "1.2.2.329";
const char * copyright_string = "(c) 2023 Cao Siqin";

#include    <errno.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <stdint.h>
#include    <string.h>
#include    <math.h>
#include    <signal.h>
#include    <fcntl.h>
#include    <ctype.h>
#include    <time.h>
#include    <sys/time.h>
#include    <sys/types.h>
#include    <sys/wait.h>
#include    <sys/stat.h>
#include    <sys/mman.h>
#include    <sys/resource.h>

#include    "header.h"
#include    "main-header.h"
#include    "String2.cpp"

#include    "Element.h"
#include    "main-atom-lists.h"
#include    "read_top.h"

//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
/*
class FFAtomType { public:
    char name[MAX_NAME], mole[MAX_NAME]; double mass; double charge; double sigma; double epsilon;
    void init(char * _name, char * _mole, double _mass, double _charge, double _sigma, double _epsilon){ int len = 0;
        memset(name, 0, sizeof(name)); len = (int)strlen(_name); if (len>MAX_NAME-1) len = MAX_NAME-1; memcpy(name, _name, len);
        memset(mole, 0, sizeof(mole)); len = (int)strlen(_mole); if (len>MAX_NAME-1) len = MAX_NAME-1; memcpy(mole, _mole, len);
        mass = _mass; charge = _charge; sigma = _sigma; epsilon = _epsilon;
        //printf("init %12s : %12f %12f %12f\n", name, charge, sigma, epsilon);
    }
};
const int MAX_BONDS_PER_SOLVENT_ATOM = 12;
class FFAtomList : public FFAtomType { public:
    int index; int iaa; int nb; int ib[MAX_BONDS_PER_SOLVENT_ATOM];
    FFAtomList * next;
    void init(int _index, char * _name, char * _mole, double _mass, double _charge, double _sigma, double _epsilon){
        index = _index; nb = 0; iaa = 1;
        FFAtomType::init(_name, _mole, _mass, _charge, _sigma, _epsilon); next = nullptr;
    }
};
class FFMoleType { public:
    char name[MAX_NAME]; FFAtomList * ar;
    void init(char * _name){
        memset(name, 0, sizeof(name)); int len = (int)strlen(_name); if (len>MAX_NAME-1) len = MAX_NAME-1;
        memcpy(name, _name, len); ar = nullptr;
    }
};

const int __GMXTOP2SOLUTE_MAX_STR = 50;
const int __GMXTOP2SOLUT_MAX_RECURS = 50;
const int __GMXTOP2SOLUT_MAX_EXCL_GRPS = 10;
template <class ADT>
class ListContainer {
  public:
    int count_max, count_max_increase;
  public:
    ADT * data; int count;
  public:
    ListContainer(int page_size_ = 1000){ init(page_size_); }
    void init(int page_size_ = 1000){
        data = nullptr; count = count_max = 0; count_max_increase = page_size_;
    }
    void dispose(){ if (data) free(data); count = 0; data = nullptr; }
    void insert(ADT * src){
        if (count>=count_max){
            count_max += count_max_increase;
            ADT * data_new = (ADT*) malloc(sizeof(ADT) * count_max);
            memset(data_new, 0, sizeof(ADT)*count_max);
            if (data){ memcpy(data_new, data, sizeof(ADT)*count); free(data); }
            data = data_new;
        }
        memcpy(&data[count], src, sizeof(ADT)); count ++;
    }
};

class AnalysisTopParameters {
  public:   // input
    char szfn_ffpaths[3][MAX_PATH]; int nszfn_ffpath;
    int software_argc;
    char ** software_argv;
    char excl_grp[__GMXTOP2SOLUT_MAX_EXCL_GRPS][32]; int n_excl_grp;
  private:   // internal tables
    ListContainer <FFAtomType> lat;   // [ atomtypes ]
    ListContainer <FFMoleType> lmt;   // [ moleculetype ]
    ListContainer <FFAtomList> las;   // [ atoms ]
  public:   // options
    bool solvent_format;        // option --solvent-format : prepare for gensolvent
    bool show_atom_spc_name;    // option --use-atom-name
    bool abbreviate_format;     // option --ab
    bool allow_bond;            // option --bond or --no-bond
    int debug_level;
    int nrecursive;
  private:  // internal variables
    int on_compile;
    int imolnow, iindex, iimol, iaa_base, iaa_now;
  public:   // outputs
    char system_title[MAX_PATH];
    FILE * flog; FILE * fout;
  public:
    bool is_group_excluded(StringNS::string grp){
        for (int i=0; i<n_excl_grp; i++) if (grp == excl_grp[i]) return true;
        return false;
    }
    void add_exclude_group(const char * grp){
        if (n_excl_grp+1<__GMXTOP2SOLUT_MAX_EXCL_GRPS){
            memset(excl_grp[n_excl_grp], 0, sizeof(excl_grp[n_excl_grp]));
            strncpy(excl_grp[n_excl_grp], grp, sizeof(excl_grp[n_excl_grp]));
            n_excl_grp ++;
        }
    }
    void init(int argc, char * argv[]){
        memset(szfn_ffpaths, 0, sizeof(szfn_ffpaths)); nszfn_ffpath = 0;
        software_argc = argc; software_argv = argv;
        memset(excl_grp, 0, sizeof(excl_grp)); n_excl_grp = 0;
        lat.init(); lmt.init(); las.init();
        solvent_format = false;
        show_atom_spc_name = true;
        abbreviate_format = false;
        allow_bond = true;
        debug_level = 0;
        nrecursive = 0;

        on_compile = 0; imolnow = 0; iindex = 0; iimol = 0; iaa_base = -1; iaa_now = 0;

        memset(system_title, 0, sizeof(system_title));
        flog = stderr; fout = stdout;

        char * sz_env_ffgmxt = getenv ((char*)"GMXDATA");
        if (sz_env_ffgmxt){
            memset(szfn_ffpaths, 0, sizeof(szfn_ffpaths));
            strncpy(szfn_ffpaths[0], sz_env_ffgmxt, sizeof(szfn_ffpaths[0]));
            strncpy(szfn_ffpaths[1], sz_env_ffgmxt, sizeof(szfn_ffpaths[1])-4); strncat(szfn_ffpaths[1], "/top", MAX_PATH-1-strlen(szfn_ffpaths[1]));
            strncpy(szfn_ffpaths[2], sz_env_ffgmxt, sizeof(szfn_ffpaths[2])-12); strncat(szfn_ffpaths[2], "/gromacs/top", MAX_PATH-1-strlen(szfn_ffpaths[2]));
            nszfn_ffpath = 3;
        }
    }
    void dispose(){
        lat.dispose(); lmt.dispose(); las.dispose();
    }


    bool analysis_top(const char * filename, char * last_file_name, int last_line, ListContainer <SoluteAtomSite> * as){
        bool success = true;
        bool ret = true; FILE * file = nullptr; int nline = 0; char fn[MAX_PATH];
        StringNS::string sl[__GMXTOP2SOLUTE_MAX_STR]; char input[4096];
        if (!file){
            strcpy(fn, last_file_name);
            for (int i=(int)strlen(fn)-1; i>=0; i--) if (i==0 || fn[i]=='/'){ fn[i+1] = 0; if (i==0) fn[i] = 0; break; }
            strncat(fn, filename, MAX_PATH-1-strlen(fn)); file = fopen(fn, "r");
        }
        if (!file) for (int i=1; i<nszfn_ffpath; i++){
            if (szfn_ffpaths[i][0]) { strcpy(fn, szfn_ffpaths[i]); strncat(fn, "/", MAX_PATH-1-strlen(fn)); } else fn[0] = 0;
            strncat(fn, filename, MAX_PATH-1-strlen(fn)); file = fopen(fn, "r"); if (file) break;
        }
        if (!file){ fprintf(stderr, "%s : %s[%d] : cannot open \"%s\"\n", software_name, last_file_name, last_line, filename); return false; }
        if (debug_level>0) fprintf(stderr, "%s : debug : handling %s\n", software_name, fn);

        while (success && fgets(input, sizeof(input), file)){ nline++;
            for (int i=0; i<sizeof(input) && input[i]; i++) if (input[i] == '\r' || input[i] =='\n') input[i] = 0;
            int nw = analysis_line(input, sl, __GMXTOP2SOLUTE_MAX_STR, true); if (nw<=0) continue ;
            if (sl[0] == "#include" && nw>1){
                nrecursive ++; //printf("\033[31m%s include file: %s\033[0m\n", fn, sl[1].text);
                if (nrecursive >= __GMXTOP2SOLUT_MAX_RECURS){
                    fprintf(stderr, "%s : %s[%d] : recursive overflow when opening \"%s\"\n", software_name, last_file_name, last_line, filename);
                } else {
                    ret &= analysis_top(sl[1].text, fn, nline, as);
                }
                nrecursive --; //printf("\033[32mend recursive handling\033[0m\n");
            } else if (sl[0].text[0] == ';' || sl[0].text[0] == '#' || sl[0].text[0] == '*'){
            } else if (sl[0].text[0] == '['){
                if (sl[0] != "["){ sl[1] = &sl[0].text[1]; if (nw<2) nw = 2; }
                if (nw < 2) continue;
                if (sl[1] == "atomtypes"){ on_compile = 1;
                } else if (sl[1] == "moleculetype"){ on_compile = 2;
                } else if (sl[1] == "atoms"){ on_compile = 3;
                } else if (sl[1] == "molecules"){ on_compile = 4;
                    if (fout){
                        fprintf(fout, "# %s %s\n", software_name, software_version);
                        // fprintf(fout, "%s", szLicence);
                        if (solvent_format) fprintf(fout, "[atom]\n"); else fprintf(fout, "[solute]\n");
                        fprintf(fout, "#"); for (int i=0; i<software_argc; i++) fprintf(fout, " %s", software_argv[i]); fprintf(fout, "\n");
                        if (system_title[0]) fprintf(fout, "# system: %s\n", system_title);
                    }
                } else if (sl[1] == "system"){ on_compile = 5;
                } else if (sl[1] == "bonds"){ on_compile = 6;
                } else { on_compile = false;
                }
    //if (on_compile) printf("section: %s[%d]: %s\n", fn, nline, sl[1].text);
            } else {
                if (on_compile==1){ // atomtypes
                    for (int i=0; i<nw; i++) if (sl[i][0]=='#' || sl[i][0]==';') nw = i;
                    if (nw>=7){
                        int icol_mass = 3; int icol_charge = 4; int icol_sigma = 6; int icol_epsilon = 7;
                        if (nw<8 || sl[4]=="A" || is_string_number(sl[5])){
                            icol_mass = 2; icol_charge = 3; icol_sigma = 5; icol_epsilon = 6;
                        }
                        int ist = -1;
                        for (int i=0; i<lat.count; i++) if (sl[0].Compare(lat.data[i].name) == 0){ ist = i; break; }
                        if (debug_level>0 && ist>=0) fprintf(stderr, "%s : warning : redefined FFAtomType %s ignored\n", software_name, sl[0].text);
                        if (ist<0){
                            FFAtomType a; a.init(sl[0].text, (char*)"*", atof(sl[icol_mass].text), atof(sl[icol_charge].text), atof(sl[icol_sigma].text), atof(sl[icol_epsilon].text));
                            lat.insert(&a);
                            //printf("insert: %s -> %s.%s %g %g %g\n", sl[0].text, lat.data[lat.count-1].mole, lat.data[lat.count-1].name, lat.data[lat.count-1].charge, lat.data[lat.count-1].sigma, lat.data[lat.count-1].epsilon);
                        }
                    } else {
                        fprintf(stderr, "%s : %s[%d] : syntex error in atom type %s\n", software_name, fn, nline, sl[0].text); success = false;
                    }
                    //if (nat >= natmax){ fprintf(stderr, "%s : error : too many atomtypes (max: %d)\n", software_name, natmax); success = false; }
                } else if (on_compile==2){ // moleculetype
                    imolnow = -1;
                    for (int i=0; i<lmt.count; i++) if (sl[0] == lmt.data[i].name){ imolnow = i; break; }
                    if (imolnow>=0) fprintf(stderr, "%s : warning : redefied FFMoleType %s ignored\n", software_name, sl[0].text);
                    if (imolnow<0){
                        FFMoleType this_mt; this_mt.init(sl[0].text);
                        lmt.insert(&this_mt);
                    }
                    imolnow = lmt.count - 1;
                } else if (on_compile==3){ // atoms
                    if (imolnow<0 || imolnow>=lmt.count){
                        fprintf(stderr, "%s : warning : ignore atom %s without molecules\n", software_name, sl[0].text);
                    } else {
                        //int idef = -1; for (int i=0; i<nat; i++) if (sl[1].Equ(at[i].name)){ idef = i; break; }
                        int idef = -1; for (int i=0; i<lat.count; i++) if (sl[1].Equ(lat.data[i].name)){ idef = i; break; }
                        if (idef<0){
                            fprintf(stderr, "%s : %s[%d] : error : atom %s not defined\n", software_name, fn, nline, sl[1].text); success = false;
                        } else {
                            FFAtomList this_a;
                            this_a.init(atoi(sl[0].text), show_atom_spc_name?sl[4].text:lat.data[idef].name, sl[3].text, lat.data[idef].mass, lat.data[idef].charge, lat.data[idef].sigma, lat.data[idef].epsilon);
                            this_a.iaa = atoi(sl[2].text);
                            if (nw>=6) this_a.charge = atof(sl[6].text);
                            if (nw>=7) this_a.mass = atof(sl[7].text);
                            //printf("Atom[%d] = %d%s.%s %g %g %g (%s)\n", this_a.index, this_a.iaa, this_a.mole, this_a.name, this_a.charge, this_a.sigma, this_a.epsilon, lat.data[idef].name);
                            las.insert(&this_a);
                            if (!lmt.data[imolnow].ar) lmt.data[imolnow].ar = &las.data[las.count-1]; else {
                                FFAtomList * q = lmt.data[imolnow].ar; while (q->next) q = q->next; q->next = &las.data[las.count-1];
                            }
                        }
                    }
                } else if (on_compile==6 ){ // bond
                    for (int i=0; i<nw; i++) if (sl[i][0]=='#' || sl[i][0]==';') nw = i;
                    if (nw<2){
                        fprintf(stderr, "%s : warning : incomplete bond line \"%s\" ignored\n", software_name, sl[0].text);
                    } else if (!(StringNS::is_string_number(sl[0]) && StringNS::is_string_number(sl[1]))){
                        fprintf(stderr, "%s : warning : incorrect bond line \"%s %s\" ignored\n", software_name, sl[0].text, sl[1].text);
                    } else if (imolnow<0 || imolnow>=lmt.count){
                        fprintf(stderr, "%s : warning : ignore bond %s-%s without molecules\n", software_name, sl[0].text, sl[1].text);
                    } else {
                        int bondi = atoi(sl[0].text); int bondj = atoi(sl[1].text);
                        FFAtomList * ai = nullptr; FFAtomList * aj = nullptr;
                        //for (FFAtomList * ax = mt[imolnow].ar; ax && (!ai || !aj); ax=ax->next){
                        for (FFAtomList * ax = lmt.data[imolnow].ar; ax && (!ai || !aj); ax=ax->next){
                            if (ax->index==bondi) ai = ax; if (ax->index==bondj) aj = ax;
                        }
                        if (ai && aj){
                            if (ai->nb+1<MAX_BONDS_PER_SOLVENT_ATOM) ai->ib[ai->nb++] = aj->index - ai->index;
                            if (aj->nb+1<MAX_BONDS_PER_SOLVENT_ATOM) aj->ib[aj->nb++] = ai->index - aj->index;
                        }
                        //printf("define bond for %d : %d (%s:%s) - %d (%s:%s)\n", imolnow, bondi, ai?ai->mole:"nullptr", ai?ai->name:"nullptr", bondj, aj?aj->mole:"nullptr", aj?aj->name:"nullptr");
                    }
                } else if (on_compile==4){ // molecules
                    if (is_group_excluded(sl[0])){
                        fprintf(stderr, "# exclude: %s %d\n", sl[0].text, atoi(sl[1].text));
                        continue;
                    }
                    if (nw<2){
                        fprintf(stderr, "%s : %s[%d] : error : incomplete mole line\n", software_name, fn, nline); success = false; continue;
                    }

                  if (true){
                    int imol = -1; for (int i=0; i<lmt.count; i++) if (sl[0] == lmt.data[i].name){ imol = i; break; }
                    if (imol<0){
                        fprintf(stderr, "%s : %s[%d] : error : molecule %s undefined\n", software_name, fn, nline, sl[0].text); success = false; continue;
                    }
                    int nm = atoi(sl[1].text);
                    if (fout) fprintf(fout, "# molecule: %s\n", lmt.data[imol].name);
                    int n_atom_in_mol = 0; int n_aa_in_mol = 0;

                    int count_display = nm; int count_abbreviate = 0;
                    if (abbreviate_format){ count_display = 1; count_abbreviate = nm-1; }

                    for (int ec=0; ec<count_display; ec++){
                        for (FFAtomList * p = lmt.data[imol].ar; p; p=p->next){
                            ++iindex; n_atom_in_mol ++;
                            if (iaa_base<0) iaa_base = 0; else if (p->iaa+iaa_base < iaa_now) iaa_base = iaa_now;
                            iaa_now = p->iaa + iaa_base; if (n_aa_in_mol<p->iaa) n_aa_in_mol = p->iaa;
                          // save atom
                            if (as){
                                SoluteAtomSite this_sas; memset(&this_sas, 0, sizeof(this_sas));
                                this_sas.init(iindex, p->name, iaa_now, show_atom_spc_name?p->mole:sl[0].text, p->mass, p->charge, p->sigma, p->epsilon);
                                this_sas.nbond = p->nb;
                                for (this_sas.nbond=0; this_sas.nbond<p->nb; this_sas.nbond++) this_sas.ibond[this_sas.nbond] = p->ib[this_sas.nbond];
                                as->insert(&this_sas);
                            }
                            //fprintf(fout, "%6d %4s %6d %4s %5.2f %12g %12g %12g\n", iindex, p->name, iaa_now, show_atom_spc_name?p->mole:sl[0].text, p->mass, p->charge, p->sigma, p->epsilon);
                          // print atom
                            if (fout){
                                if (solvent_format){
                                    fprintf(fout, "%4s %4s %3d %3d %12g %12g %12g\n", p->name, show_atom_spc_name?p->mole:sl[0].text, iindex, iindex, p->charge, p->sigma, p->epsilon);
                                } else {
                                    //fprintf(fout, "%4s %4s %5.2f %12g %12g %12g\n", p->name, show_atom_spc_name?p->mole:sl[0].text, p->mass, p->charge, p->sigma, p->epsilon);
                                    fprintf(fout, "%6d %4s %6d %4s %5.2f %12g %12g %12g", iindex, p->name, iaa_now, show_atom_spc_name?p->mole:sl[0].text, p->mass, p->charge, p->sigma, p->epsilon);
                                    if (allow_bond && p->nb>0){
                                        fprintf(fout, "  bond:");
                                        for (int i=0; i<p->nb; i++)fprintf(fout, "%d%s", p->ib[i], i+1==p->nb?"":",");
                                    }
                                    fprintf(fout, "\n");
                                }
                            }
                        }
                        iaa_base += n_aa_in_mol;
                    }
                    if (count_abbreviate>0){
                      // save atom
                        if (as){
                            for (int im=0; im<count_abbreviate; im++){
                                int last_iaa_before_repeat = as->data[as->count-1].iaa;
                                for (int ima=0; ima<n_atom_in_mol; ima++){
                                    int isrc = as->count - n_atom_in_mol; if (isrc<0) isrc = 0; if (isrc>=as->count) isrc = as->count-1;
                                    SoluteAtomSite this_sas = as->data[isrc];
                                    ++iindex; this_sas.index = iindex;
                                    this_sas.iaa = iaa_base + n_aa_in_mol - (last_iaa_before_repeat - this_sas.iaa);
                                    as->insert(&this_sas);
                                }
                                iaa_base += n_aa_in_mol;
                            }
                        } else {
                            iaa_base += count_abbreviate;
                            iindex += n_atom_in_mol*(count_abbreviate);
                        }
                      // print atom
                        if (fout) fprintf(fout, "#repeat %d atoms %d times\n", n_atom_in_mol, count_abbreviate);
                    }

                    iimol+=nm;
                  }
                } else if (on_compile==5){ // system
                    memset(system_title, 0, sizeof(system_title));
                    for (int i=0; i<nw; i++){
                        strncat(system_title, sl[i].text, sizeof(system_title)-2-strlen(system_title));
                        if (i<nw-1) strcat(system_title, " ");
                    }
                }
            }
        }

        fclose(file); return true;
    }
};
*/


//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const char * szHelp = "\
  The input/output files:\n\
    -p, -top              topology file, TOP\n\
    -ffpath, -include     forcefield folder, multiple separated with \":\"\n\
    -o                    output file, default: screen\n\
    -debug                show debug info\n\
    -excl                 exclude group, default: SOL\n\
    -use-atom-name        (-an) use atom name, not atom type (default)\n\
    -use-atom-type        (-at) use atom type, not atom name\n\
    -solvent-format       (-for-gensolvent) output as solvent format\n\
    -bond, -no-bond[s]    show/hide bond information, default on\n\
    -no-index             (-ni) don't output atom and molecule index\n\
    -abbreviate           (-ab) allow #repeat commands, default off\n\
    -default              reset all options\n\
  The output format:\n\
    mole_name atom_name mass charge sigma epsilon\n\
";


//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

int debug_level = 0; bool show_atom_spc_name = true; bool solvent_format = false;
    bool abbreviate_format = false; bool allow_bond = true; bool allow_index = true;
char * info_file_name = (char*)"";
char szfn_ffpath[MAX_PATH];
char szfn_top[MAX_PATH];
char szfn_out[MAX_PATH];
char excl_grps[10][MAX_PATH]; int n_excl_grp = 0;
int analysis_parameter_line(char * argv[], int * argi, int argc, char * script_name, int script_line){
    int ret = 0; int i = *argi; bool analysis_script = !script_name? false : (!script_name[0]? false : true);
    StringNS::string key = argv[i];
    if (!analysis_script && (key == "-h" || key == "-help" || key == "--h" || key == "--help")){ ret = 2;
    } else if (!analysis_script && (key == "-version" || key == "--version")){ ret = 3;
    } else if (key == "-p" || key == "--p" || key == "-top" || key == "--top"){ if (i+1<argc){ i++; strcpy(szfn_top, argv[i]); }
    } else if (key == "-o" || key == "--o"){ if (i+1<argc){ i++; strcpy(szfn_out, argv[i]); }
    } else if (key == "-excl"){
        if (i+1<argc){ i++;
            if (n_excl_grp<10){
                strcpy(excl_grps[n_excl_grp], argv[i]);
                n_excl_grp ++;
            }
        }
    } else if (key == "-debug"){ debug_level = 1;
    } else if (key=="-ffpath" || key=="--ffpath" || key=="-ffpath" || key=="-include" || key=="--include" || key=="include"){ if (i+1<argc){
        i++; strcpy(szfn_ffpath,argv[i]); for (int j=0; j<MAX_PATH && szfn_ffpath[j]; j++) if (szfn_ffpath[j]==':') szfn_ffpath[j] = 0;
      }
    } else if (key=="-an"||key=="--an"||key=="-use-atom-name"||key=="--use-atom-name"||key=="-use_atom_name"||key=="--use_atom_name"){
        show_atom_spc_name = true;
    } else if (key=="-at"||key=="--at"||key=="-use-atom-type"||key=="--use-atom-type"||key=="-use_atom_type"||key=="--use_atom_type"){
        show_atom_spc_name = false;
    } else if (key=="-solvent-format"||key=="--solvent-format"||key=="-solvent_format"||key=="--solvent_format"||key=="-for-gensolvent"||key=="--solvent-format"||key=="-for_gensolvent"||key=="--solvent_format"){
        solvent_format = true;
    } else if (key=="-ab"||key=="--ab"||key=="-abbreviate"||key=="--abbreviate"){
        abbreviate_format = true;
    } else if (key=="-nb"||key=="--nb"||key=="-no-bond"||key=="--no-bond"||key=="-no-bonds"||key=="--no-bonds"){
        allow_bond = false;
    } else if (key=="-bond"||key=="--bond"||key=="-bond"){
        allow_bond = true;
    } else if (key=="-ni"||key=="--ni"||key=="-no-index"||key=="--no-index"){
        allow_index = false;
    } else if (key=="-default"||key=="--default"||key=="-default-format"||key=="--default-format"){
        show_atom_spc_name = true; solvent_format = false;
        abbreviate_format = false; allow_bond = false; allow_index = true;
    } else {
        strcpy(szfn_top, argv[i]);
    }
    *argi = i;
    return ret;
}
int analysis_params(int argc, char * argv[]){
    bool success = true; int error = 0;
  // analysis command line params
    if (argc<2){ printf("%s %s\n", software_name, software_version); return 0; }
    for (int i=1; i<argc; i++){
        if (argv[i][0] == '#' || argv[i][0] == ';'){
        } else {
            int _error = analysis_parameter_line(argv, &i, argc, (char*)"", i);
            if (_error){ success = false; error |= _error; }
        }
    }
    if (error == 2){
        printf("%s %s %s\n", software_name, software_version, copyright_string);
        printf("%s", szHelp);
        printf("%s", szLicence);
        return 0;
    } else if (error == 3){
        printf("%s\n", software_version);
        return 0;
    }
    if (!success) return error;
  // prepare other params

    return 0;
}



int main(int argc, char * argv[]){
    bool success = true; int error = 0; szfn_out[0] = 0;
    #ifdef DISTRIBUTE_VERSION
        software_version = DISTRIBUTE_VERSION;
    #endif

    error = analysis_params(argc, argv); if (error) return error;
    if (error) success = false;

    FILE * fout = stdout;
    if (success){
        if (StringNS::string(szfn_out)=="con" || StringNS::string(szfn_out)=="stdout" || StringNS::string(szfn_out)=="screen"){
            fout = stdout;
        } else if (StringNS::string(szfn_out)=="stderr"){
            fout = stderr;
        } else if (szfn_out[0]) {
            fout = fopen(szfn_out, "w");
            if (!fout){ fprintf(stderr, "%s : error : cannot write to %s\n", software_name, szfn_out); success = false; }
        }
    }

    if (success && szfn_top[0]){
        AnalysisTopParameters atp;
        if (szfn_ffpath[0]) atp.init(argc, argv, szfn_ffpath); else atp.init(argc, argv);
        atp.flog = stderr; atp.fout = fout;
        atp.solvent_format = solvent_format;
        atp.show_atom_spc_name = show_atom_spc_name;
        atp.abbreviate_format = abbreviate_format;
        atp.allow_bond = allow_bond;
        atp.debug_level = debug_level;

        atp.analysis_top(szfn_top, "", 1, nullptr);

        if (fout && fout!=stdout && fout!=stderr) fclose(fout);

        atp.dispose();
    }
    return 0;
}
