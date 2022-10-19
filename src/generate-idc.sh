#!/bin/bash

# Update June 8, 2022:
#   line 60 & 68: fix the algorithm of "dielect_here"
# Update Jan 9, 2022 :
#   based on generate-sigma-shrinking-2.sh of Jan 22 2022
#   line 53: when considering screening of charges, the screening charges are evenly distributed on bonds, not all to the centre atom.

# bug to be resolved:
#    the last line of repeat atoms are not shown here

IFS=''

if [ -z "$1" ]; then
    echo "generate-idc.sh solute_file_to_handle [dielect_mixing_rule]"
    echo "  dielect_mixing_rule can be: parallel, serial, serial2"
    exit
elif [ ! -e "$1" ]; then
    echo error: cannot open $1
    exit
fi

# dielectric constant mixing rule
if [ -z "$2" ]; then
  dielect_mixing="parallel";
    # support: parallel (ε=w1*ε1+w2*ε2), serial (1/ε=w1/ε1+w2/ε2), serial2 (1/ε=w1/ε+w2/ε)
else
  dielect_mixing=$2
fi

# correction for TIP3P water oxygen
q_v=-0.834;
q_ren=0.417;
b_ren=0.0954;
dielect_b=8;
dielect_u=2;

# correction for TIP3P water oxygen
sigma_v=0.315061;
epsilon_v=0.636386;
temperature=298;

echo "# IDC generated on `date`"
echo "#" $0 "$@"
echo "#" dielectric constant mixing rule: $dielect_mixing

cat $1 | awk -v dielect_mixing=$dielect_mixing -v q_v=$q_solvent -v q_v=$q_v -v q_ren=$q_ren -v b_ren=$b_ren -v dielect=$dielect_b -v dielect_u=$dielect_u -v sigma_v=$sigma_v -v epsilon_v=$epsilon_v -v temperature=$temperature '
function dielect_mix(dielect1, w1, dielect2){
  if (dmal==2) return 1/(w1/dielect1 + (1-w1)/dielect2);
  if (dmal==3) return 1/(w1^2/dielect1 + (1-w1^2)/dielect2);
  return w1*dielect1 + (1-w1)*dielect2; 
} BEGIN{
    FS=" "; na=0; ntitle=0; kT=temperature/120.27; beta=1/kT;
    ee=2.71828182845904523536028747135;
    dmal = 0;
    if (dielect_mixing=="parallel") dmal = 1;
    else if (dielect_mixing=="serial") dmal = 2;
    else if (dielect_mixing=="serial2") dmal = 3;
}{
    if (substr($1,1,1)!="#"&&substr($1,1,1)!="["&&NF>=8){
        na++;
        id[na]=$1; name[na]=$2; iaa[na]=$3; mole[na]=$4; mass[na]=$5; charge[na]=$6; sigma_u[na]=$7; epsilon_u[na]=$8;
        if (substr($9,1,5)=="bond:"){ split(substr($9,6),ab,","); nbonds[na]=length(ab); for (i=1;i<=nbonds[na];i++)bonds[na,i]=ab[i]; }
        epsilon[na]=sqrt(epsilon_u[na] * epsilon_v);
    } else if (substr($1,1,1)=="#"||substr($0,1,1)=="["){
        ntitle++; title[ntitle]=$0; title_na[ntitle]=na;
    }
}END{
    for (ia=1;ia<=na;ia++){
        charge_ia=charge[ia];
        bond_ia=0;

        if (nbonds[ia]>=1){
            for (i=1;i<=nbonds[ia];i++){
                if (na>4) charge_ia += charge[ia+bonds[ia,i]] / (nbonds[ia+bonds[ia,i]]<=1? 1 : nbonds[ia+bonds[ia,i]]);
                if (sigma_u[ia+bonds[ia,i]]>sigma_u[ia]/2 || nbonds[ia+bonds[ia,i]]>1) bond_ia ++;
            }

            # printf("# %s : %d : charge %g -> %g\n",name[ia],nbonds[ia],charge[ia],charge_ia);
            # wd = 1-(bond_ia>3?3:bond_ia)/3;
            # dielect_here = dielect_mix(dielect, wd, dielect_u);
            # printf("# %s : %d : bond_ia=%d wd=%g dielect= %g (%g,%g)\n",name[ia],nbonds[ia],bond_ia,wd,dielect_here,dielect,dielect_u);
        }

        sigma=(sigma_u[ia]+sigma_v)/2;
        x=1;
        if (charge_ia*q_v>0 && sigma>0 && epsilon[ia]>0){
          wd = 1-(bond_ia>3?3:bond_ia)/3;
          dielect_here = dielect_mix(dielect, wd, dielect_u);
          for (iscf=0;iscf<10;iscf++){
            coul=sqrt((138.9*charge_ia*q_ren*((1/(sigma/x-b_ren))-1/(sigma/x))/dielect_here)^2);
            x=(0.5+0.5*sqrt(1+coul/(4*epsilon[ia])))^(1/6);
          }
        }

        for (i=1;i<=ntitle;i++) if(title_na[i]+1==ia)printf("%s\n",title[i]);

        printf("%6d %4s %3d %4s  %6.2g %12f %12f %12f  ",id[ia],name[ia],iaa[ia],mole[ia],mass[ia],charge[ia],sigma_u[ia],epsilon_u[ia]);
        if (nbonds[ia]>0){ printf("bond:"); for(i=1;i<=nbonds[ia];i++)printf(i==nbonds[ia]?"%d":"%d,",bonds[ia,i]); }
        if (x!=1) printf(" sigmas=%g,%g", sigma/x*2-sigma_v, sigma_u[ia]);
        #if (x!=1) printf(" sigmas=%g,%g", sigma/x*2-sigma_v, sigma/x*2-sigma_v);
        #if (x!=1) printf(" # dielect=%g bond_ia=%d",dielect_here,bond_ia);
        printf("\n");
    }
    for (i=1;i<=ntitle;i++) if(title_na[i]+1>na)printf("%s\n",title[i]);
}'


