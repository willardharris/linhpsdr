/* Copyright (C)
* 2018 - John Melton, G0ORX/N6LYT
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/

#include <stdio.h>
#include <stdlib.h>

#include "mode.h"
#include "filter.h"
#include "property.h"

FILTER filterLSB[FILTERS]={
    {-5150,-150,"5.0k"},
    {-4550,-150,"4.4k"},
    {-3950,-150,"3.8k"},
    {-3450,-150,"3.3k"},
    {-3050,-150,"2.9k"},
    {-2850,-150,"2.7k"},
    {-2550,-150,"2.4k"},
    {-2250,-150,"2.1k"},
    {-1950,-150,"1.8k"},
    {-1150,-150,"1.0k"},
    {-2850,-150,"Var1"},
    {-2850,-150,"Var2"}
    };

FILTER filterDIGL[FILTERS]={
    {-5000,    0, "5.0k"},
    {-4000,    0, "4.0k"},
    {-3000,    0, "3.0k"},
    {-2750, -250, "2.5k"},
    {-2500, -500, "2.0k"},
    {-2250, -750, "1.5k"},
    {-2000, -1000, "1.0k"},
    {-1875, -1125, "750"},
    {-1750, -1250, "500"},
    {-1625, -1375, "250"},
    {-3000,0,"Var1"},
    {-3000,0,"Var2"}
    };

FILTER filterUSB[FILTERS]={
    {150,5150,"5.0k"},
    {150,4550,"4.4k"},
    {150,3950,"3.8k"},
    {150,3450,"3.3k"},
    {150,3050,"2.9k"},
    {150,2850,"2.7k"},
    {150,2550,"2.4k"},
    {150,2250,"2.1k"},
    {150,1950,"1.8k"},
    {150,1150,"1.0k"},
    {150,2850,"Var1"},
    {150,2850,"Var2"}
    };

FILTER filterDIGU[FILTERS]={
    {   0, 5000, "5.0k"},
    {   0, 4000, "4.0k"},
    {   0, 3000, "3.0k"},
    { 250, 2750, "2.5k"},
    { 500, 2500, "2.0k"},
    { 750, 2250, "1.5k"},
    {1000, 2000, "1.0k"},
    {1125, 1875, "750"},
    {1250, 1750, "500"},
    {1375, 1625, "250"},
    {0,3000,"Var1"},
    {0,3000,"Var2"}
    };

FILTER filterCWL[FILTERS]={
    {500,500,"1.0k"},
    {400,400,"800"},
    {375,375,"750"},
    {300,300,"600"},
    {250,250,"500"},
    {200,200,"400"},
    {125,125,"250"},
    {50,50,"100"},
    {25,25,"50"},
    {13,13,"25"},
    {250,250,"Var1"},
    {250,250,"Var2"}
    };

FILTER filterCWU[FILTERS]={
    {500,500,"1.0k"},
    {400,400,"800"},
    {375,375,"750"},
    {300,300,"600"},
    {250,250,"500"},
    {200,200,"400"},
    {125,125,"250"},
    {50,50,"100"},
    {25,25,"50"},
    {13,13,"25"},
    {250,250,"Var1"},
    {250,250,"Var2"}
    };

FILTER filterAM[FILTERS]={
    {-8000,8000,"16k"},
    {-6000,6000,"12k"},
    {-5000,5000,"10k"},
    {-4000,4000,"8k"},
    {-3300,3300,"6.6k"},
    {-2600,2600,"5.2k"},
    {-2000,2000,"4.0k"},
    {-1550,1550,"3.1k"},
    {-1450,1450,"2.9k"},
    {-1200,1200,"2.4k"},
    {-3300,3300,"Var1"},
    {-3300,3300,"Var2"}
    };

FILTER filterSAM[FILTERS]={
    {-8000,8000,"16k"},
    {-6000,6000,"12k"},
    {-5000,5000,"10k"},
    {-4000,4000,"8k"},
    {-3300,3300,"6.6k"},
    {-2600,2600,"5.2k"},
    {-2000,2000,"4.0k"},
    {-1550,1550,"3.1k"},
    {-1450,1450,"2.9k"},
    {-1200,1200,"2.4k"},
    {-3300,3300,"Var1"},
    {-3300,3300,"Var2"}
    };

FILTER filterFMN[FILTERS]={
    {-8000,8000,"8k"},
    {-6000,6000,"16k"},
    {-5000,5000,"10k"},
    {-4000,4000,"8k"},
    {-3300,3300,"6.6k"},
    {-2600,2600,"5.2k"},
    {-2000,2000,"4.0k"},
    {-1550,1550,"3.1k"},
    {-1450,1450,"2.9k"},
    {-1200,1200,"2.4k"},
    {-3300,3300,"Var1"},
    {-3300,3300,"Var2"}
    };

FILTER filterDSB[FILTERS]={
    {-8000,8000,"16k"},
    {-6000,6000,"12k"},
    {-5000,5000,"10k"},
    {-4000,4000,"8k"},
    {-3300,3300,"6.6k"},
    {-2600,2600,"5.2k"},
    {-2000,2000,"4.0k"},
    {-1550,1550,"3.1k"},
    {-1450,1450,"2.9k"},
    {-1200,1200,"2.4k"},
    {-3300,3300,"Var1"},
    {-3300,3300,"Var2"}
    };

FILTER filterSPEC[FILTERS]={
    {-8000,8000,"16k"},
    {-6000,6000,"12k"},
    {-5000,5000,"10k"},
    {-4000,4000,"8k"},
    {-3300,3300,"6.6k"},
    {-2600,2600,"5.2k"},
    {-2000,2000,"4.0k"},
    {-1550,1550,"3.1k"},
    {-1450,1450,"2.9k"},
    {-1200,1200,"2.4k"},
    {-3300,3300,"Var1"},
    {-3300,3300,"Var2"}
    };

FILTER filterDRM[FILTERS]={
    {-8000,8000,"16k"},
    {-6000,6000,"12k"},
    {-5000,5000,"10k"},
    {-4000,4000,"8k"},
    {-3300,3300,"6.6k"},
    {-2600,2600,"5.2k"},
    {-2000,2000,"4.0k"},
    {-1550,1550,"3.1k"},
    {-1450,1450,"2.9k"},
    {-1200,1200,"2.4k"},
    {-3300,3300,"Var1"},
    {-3300,3300,"Var2"}
    };

FILTER *filters[]={
    filterLSB
    ,filterUSB
    ,filterDSB
    ,filterCWL
    ,filterCWU
    ,filterFMN
    ,filterAM
    ,filterDIGU
    ,filterSPEC
    ,filterDIGL
    ,filterSAM
    ,filterDRM

};

void filterSaveState() {
    char value[128];

    // save the Var1 and Var2 settings
    sprintf(value,"%d",filterLSB[FVar1].low);
    setProperty("filter.lsb.var1.low",value);
    sprintf(value,"%d",filterLSB[FVar1].high);
    setProperty("filter.lsb.var1.high",value);
    sprintf(value,"%d",filterLSB[FVar2].low);
    setProperty("filter.lsb.var2.low",value);
    sprintf(value,"%d",filterLSB[FVar2].high);
    setProperty("filter.lsb.var2.high",value);
    
    sprintf(value,"%d",filterDIGL[FVar1].low);
    setProperty("filter.digl.var1.low",value);
    sprintf(value,"%d",filterDIGL[FVar1].high);
    setProperty("filter.digl.var1.high",value);
    sprintf(value,"%d",filterDIGL[FVar2].low);
    setProperty("filter.digl.var2.low",value);
    sprintf(value,"%d",filterDIGL[FVar2].high);
    setProperty("filter.digl.var2.high",value);
    
    sprintf(value,"%d",filterCWL[FVar1].low);
    setProperty("filter.cwl.var1.low",value);
    sprintf(value,"%d",filterCWL[FVar1].high);
    setProperty("filter.cwl.var1.high",value);
    sprintf(value,"%d",filterCWL[FVar2].low);
    setProperty("filter.cwl.var2.low",value);
    sprintf(value,"%d",filterCWL[FVar2].high);
    setProperty("filter.cwl.var2.high",value);
    
    sprintf(value,"%d",filterUSB[FVar1].low);
    setProperty("filter.usb.var1.low",value);
    sprintf(value,"%d",filterUSB[FVar1].high);
    setProperty("filter.usb.var1.high",value);
    sprintf(value,"%d",filterUSB[FVar2].low);
    setProperty("filter.usb.var2.low",value);
    sprintf(value,"%d",filterUSB[FVar2].high);
    setProperty("filter.usb.var2.high",value);
    
    sprintf(value,"%d",filterDIGU[FVar1].low);
    setProperty("filter.digu.var1.low",value);
    sprintf(value,"%d",filterDIGU[FVar1].high);
    setProperty("filter.digu.var1.high",value);
    sprintf(value,"%d",filterDIGU[FVar2].low);
    setProperty("filter.digu.var2.low",value);
    sprintf(value,"%d",filterDIGU[FVar2].high);
    setProperty("filter.digu.var2.high",value);
    
    sprintf(value,"%d",filterCWU[FVar1].low);
    setProperty("filter.cwu.var1.low",value);
    sprintf(value,"%d",filterCWU[FVar1].high);
    setProperty("filter.cwu.var1.high",value);
    sprintf(value,"%d",filterCWU[FVar2].low);
    setProperty("filter.cwu.var2.low",value);
    sprintf(value,"%d",filterCWU[FVar2].high);
    setProperty("filter.cwu.var2.high",value);
    
    sprintf(value,"%d",filterAM[FVar1].low);
    setProperty("filter.am.var1.low",value);
    sprintf(value,"%d",filterAM[FVar1].high);
    setProperty("filter.am.var1.high",value);
    sprintf(value,"%d",filterAM[FVar2].low);
    setProperty("filter.am.var2.low",value);
    sprintf(value,"%d",filterAM[FVar2].high);
    setProperty("filter.am.var2.high",value);
    
    sprintf(value,"%d",filterSAM[FVar1].low);
    setProperty("filter.sam.var1.low",value);
    sprintf(value,"%d",filterSAM[FVar1].high);
    setProperty("filter.sam.var1.high",value);
    sprintf(value,"%d",filterSAM[FVar2].low);
    setProperty("filter.sam.var2.low",value);
    sprintf(value,"%d",filterSAM[FVar2].high);
    setProperty("filter.sam.var2.high",value);
    
    sprintf(value,"%d",filterFMN[FVar1].low);
    setProperty("filter.fmn.var1.low",value);
    sprintf(value,"%d",filterFMN[FVar1].high);
    setProperty("filter.fmn.var1.high",value);
    sprintf(value,"%d",filterFMN[FVar2].low);
    setProperty("filter.fmn.var2.low",value);
    sprintf(value,"%d",filterFMN[FVar2].high);
    setProperty("filter.fmn.var2.high",value);
    
    sprintf(value,"%d",filterDSB[FVar1].low);
    setProperty("filter.dsb.var1.low",value);
    sprintf(value,"%d",filterDSB[FVar1].high);
    setProperty("filter.dsb.var1.high",value);
    sprintf(value,"%d",filterDSB[FVar2].low);
    setProperty("filter.dsb.var2.low",value);
    sprintf(value,"%d",filterDSB[FVar2].high);
    setProperty("filter.dsb.var2.high",value);
    
}

void filterRestoreState() {
    char* value;

    value=getProperty("filter.lsb.var1.low");
    if(value) filterLSB[FVar1].low=atoi(value);
    value=getProperty("filter.lsb.var1.high");
    if(value) filterLSB[FVar1].high=atoi(value);
    value=getProperty("filter.lsb.var2.low");
    if(value) filterLSB[FVar2].low=atoi(value);
    value=getProperty("filter.lsb.var2.high");
    if(value) filterLSB[FVar2].high=atoi(value);

    value=getProperty("filter.digl.var1.low");
    if(value) filterDIGL[FVar1].low=atoi(value);
    value=getProperty("filter.digl.var1.high");
    if(value) filterDIGL[FVar1].high=atoi(value);
    value=getProperty("filter.digl.var2.low");
    if(value) filterDIGL[FVar2].low=atoi(value);
    value=getProperty("filter.digl.var2.high");
    if(value) filterDIGL[FVar2].high=atoi(value);

    value=getProperty("filter.cwl.var1.low");
    if(value) filterCWL[FVar1].low=atoi(value);
    value=getProperty("filter.cwl.var1.high");
    if(value) filterCWL[FVar1].high=atoi(value);
    value=getProperty("filter.cwl.var2.low");
    if(value) filterCWL[FVar2].low=atoi(value);
    value=getProperty("filter.cwl.var2.high");
    if(value) filterCWL[FVar2].high=atoi(value);

    value=getProperty("filter.usb.var1.low");
    if(value) filterUSB[FVar1].low=atoi(value);
    value=getProperty("filter.usb.var1.high");
    if(value) filterUSB[FVar1].high=atoi(value);
    value=getProperty("filter.usb.var2.low");
    if(value) filterUSB[FVar2].low=atoi(value);
    value=getProperty("filter.usb.var2.high");
    if(value) filterUSB[FVar2].high=atoi(value);

    value=getProperty("filter.digu.var1.low");
    if(value) filterDIGU[FVar1].low=atoi(value);
    value=getProperty("filter.digu.var1.high");
    if(value) filterDIGU[FVar1].high=atoi(value);
    value=getProperty("filter.digu.var2.low");
    if(value) filterDIGU[FVar2].low=atoi(value);
    value=getProperty("filter.digu.var2.high");
    if(value) filterDIGU[FVar2].high=atoi(value);

    value=getProperty("filter.cwu.var1.low");
    if(value) filterCWU[FVar1].low=atoi(value);
    value=getProperty("filter.cwu.var1.high");
    if(value) filterCWU[FVar1].high=atoi(value);
    value=getProperty("filter.cwu.var2.low");
    if(value) filterCWU[FVar2].low=atoi(value);
    value=getProperty("filter.cwu.var2.high");
    if(value) filterCWU[FVar2].high=atoi(value);

    value=getProperty("filter.am.var1.low");
    if(value) filterAM[FVar1].low=atoi(value);
    value=getProperty("filter.am.var1.high");
    if(value) filterAM[FVar1].high=atoi(value);
    value=getProperty("filter.am.var2.low");
    if(value) filterAM[FVar2].low=atoi(value);
    value=getProperty("filter.am.var2.high");
    if(value) filterAM[FVar2].high=atoi(value);

    value=getProperty("filter.sam.var1.low");
    if(value) filterSAM[FVar1].low=atoi(value);
    value=getProperty("filter.sam.var1.high");
    if(value) filterSAM[FVar1].high=atoi(value);
    value=getProperty("filter.sam.var2.low");
    if(value) filterSAM[FVar2].low=atoi(value);
    value=getProperty("filter.sam.var2.high");
    if(value) filterSAM[FVar2].high=atoi(value);

    value=getProperty("filter.fmn.var1.low");
    if(value) filterFMN[FVar1].low=atoi(value);
    value=getProperty("filter.fmn.var1.high");
    if(value) filterFMN[FVar1].high=atoi(value);
    value=getProperty("filter.fmn.var2.low");
    if(value) filterFMN[FVar2].low=atoi(value);
    value=getProperty("filter.fmn.var2.high");
    if(value) filterFMN[FVar2].high=atoi(value);

    value=getProperty("filter.dsb.var1.low");
    if(value) filterDSB[FVar1].low=atoi(value);
    value=getProperty("filter.dsb.var1.high");
    if(value) filterDSB[FVar1].high=atoi(value);
    value=getProperty("filter.dsb.var2.low");
    if(value) filterDSB[FVar2].low=atoi(value);
    value=getProperty("filter.dsb.var2.high");
    if(value) filterDSB[FVar2].high=atoi(value);

}

