/* 2-D Low Rank Finite-difference wave extrapolation */
/*
  Copyright (C) 2008 University of Texas at Austin
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <rsf.h>
#include <math.h>
#include <limits.h>
#include <time.h>

#include "source.h"
#include "pml.h"

static float ***Gx, ***Gz;
static int *sxx, *sxz, *szx, *szz;
static int lenx, lenz;
static int marg;


static float ldx(float **data, int ix, int iz)
/*<Low rank finite difference : d/dx>*/
{
    float res = 0.0;
    int il;
    for (il = 0; il < lenx; il++) {
	//res += 0.5*(data[ix+sxx[il]][iz+sxz[il]] - data[ix-sxx[il]+1][iz-sxz[il]])*Gx[il][ix][iz];
	//res += -0.5*(data[ix+sxx[il]][iz+sxz[il]] - data[ix-sxx[il]-1][iz-sxz[il]])*Gx[il][ix][iz];
	res += 0.5*(data[ix-sxx[il]][iz-sxz[il]] - data[ix+sxx[il]-1][iz+sxz[il]])*Gx[il][ix][iz];
    }
    return res;
}

static float ldz(float **data, int ix, int iz)
/*<Low rank finite difference : d/dz>*/
{
    float res = 0.0;
    int il;
    for (il = 0; il < lenz; il++) {
	res += 0.5*(data[ix-szx[il]][iz-szz[il]] - data[ix+szx[il]][iz+szz[il]-1])*Gz[il][ix][iz];
    }
    return res;
}

int main(int argc, char* argv[]) 
{
    clock_t tstart,tend;
    double duration;
    bool verb;
    int nx, nz, nxz, nt, ix, iz, it;
    int nxb, nzb;
    float dt, dx, dz;
    float **txxn1, **txxn0, **vxn1, **vzn1, **vxn0, **vzn0;
    float **vel, **den, **c11, *source;
    float **denx, **denz;
    int spx, spz;
    
    int mx, mz; /*margin*/
 
    sf_file fvel, fden, fsource, fwf/*wave field*/; 
    sf_file fGx, fGz, fsxx, fsxz, fszx, fszz;
    float *sxxtmp, *sxztmp, *szxtmp, *szztmp;
    sf_axis at, ax, az;

    spara sp={0};

    int pmlout, pmld0, decaybegin;
    float decay;
    float gamma = GAMMA;


    tstart = clock();
    sf_init(argc, argv);
    if (!sf_getbool("verb", &verb)) verb=false; /*verbosity*/

    /*Set I/O file*/
    fsource = sf_input("in");  /*source wavelet*/
    fvel    = sf_input("vel"); /*velocity*/
    fden    = sf_input("den"); /*density*/
    fwf     = sf_output("out");/*wavefield snap*/
    
    /* Read/Write axes */
    at = sf_iaxa(fsource, 1); nt = sf_n(at); dt = sf_d(at); 
    ax = sf_iaxa(fvel, 2); nxb = sf_n(ax); dx = sf_d(ax); 
    az = sf_iaxa(fvel, 1); nzb = sf_n(az); dz = sf_d(az);


    fGx = sf_input("Gx");
    fGz = sf_input("Gz");
    fsxx = sf_input("sxx");
    fsxz = sf_input("sxz");
    fszx = sf_input("szx");
    fszz = sf_input("szz");
    
    if (SF_FLOAT != sf_gettype(fsource)) sf_error("Need float input");
    if (SF_FLOAT != sf_gettype(fvel)) sf_error("Need float input");
    if (SF_FLOAT != sf_gettype(fden)) sf_error("Need float input");
    
    if (!sf_getint("spx", &spx)) sf_error("Need spx input");
    /*source point in x */
    if (!sf_getint("spz", &spz)) sf_error("Need spz input");
    /* source point in z */
    if (!sf_getint("pmlsize", &pmlout)) pmlout=PMLOUT;
    /* size of PML layer */
    if (!sf_getint("pmld0", &pmld0)) pmld0=PMLD0;
    /* PML parameter */
    if (!sf_getint("decay",&decay)) decay=DECAY_FLAG;
    /* Flag of decay boundary condtion: 1 = use ; 
                                        0 = not use */
    if (!sf_getint("decaybegin",&decaybegin)) decaybegin=DECAY_BEGIN;
    /* Begin time of using decay boundary condition */
    if (!sf_histint(fGx, "n1", &nxz)) sf_error("No n1= in input");
    if (nxz != nxb*nzb) sf_error (" Need nxz = nxb*nzb");
    if (!sf_histint(fGx,"n2", &lenx)) sf_error("No n2= in input");
    if (!sf_histint(fGz,"n2", &lenz)) sf_error("No n2= in input");
    
    sxxtmp = sf_floatalloc(lenx);
    sxztmp = sf_floatalloc(lenx);
    szxtmp = sf_floatalloc(lenz);
    szztmp = sf_floatalloc(lenz);
    
    sxx = sf_intalloc(lenx);
    sxz = sf_intalloc(lenx);
    szx = sf_intalloc(lenz);
    szz = sf_intalloc(lenz);

    sf_floatread(sxxtmp, lenx, fsxx);
    sf_floatread(sxztmp, lenx, fsxz);
    sf_floatread(szxtmp, lenz, fszx);
    sf_floatread(szztmp, lenz, fszz);
    mx = 0; mz = 0;
    for (ix=0; ix<lenx; ix++) {
	sxx[ix] = (int)sxxtmp[ix];
	sxz[ix] = (int)sxztmp[ix];
	mx = abs(sxx[ix])>mx? abs(sxx[ix]):mx;
    }

    for (iz=0; iz<lenz; iz++) {
	szx[iz] = (int)szxtmp[iz];
	szz[iz] = (int)szztmp[iz];
	mz = abs(szz[iz])>mz? abs(szz[iz]):mz;
    }
    marg = mx>mz?mx:mz;
    
    nx = nxb - 2*pmlout - 2*marg;
    nz = nzb - 2*pmlout - 2*marg;
    
    //sf_setn(az, nz);
    //sf_setn(ax, nx);
    sf_oaxa(fwf, az, 1);
    sf_oaxa(fwf, ax, 2);
    sf_oaxa(fwf, at, 3);

    Gx = sf_floatalloc3(nzb, nxb, lenx);
    Gz = sf_floatalloc3(nzb, nxb, lenz);
    sf_floatread(Gx[0][0], nzb*nxb*lenx, fGx);
    sf_floatread(Gz[0][0], nzb*nxb*lenz, fGz);
    free(sxxtmp); free(sxztmp); free(szxtmp); free(szztmp);
    vel = sf_floatalloc2(nzb, nxb);
    den = sf_floatalloc2(nzb, nxb);
    c11 = sf_floatalloc2(nzb, nxb);
    
    denx = sf_floatalloc2(nzb, nxb);
    denz = sf_floatalloc2(nzb, nxb);
    

    sf_floatread(vel[0], nxz, fvel);
    sf_floatread(den[0], nxz, fden);
    for (ix = 0; ix < nxb; ix++) {
	for ( iz= 0; iz < nzb; iz++) {
	    c11[ix][iz] = den[ix][iz]*vel[ix][iz]*vel[ix][iz];
	    denx[ix][iz] = den[ix][iz];
	    denz[ix][iz] = den[ix][iz];
	    if(c11[ix][iz] == 0.0) sf_warning("c11=0: ix=%d iz%d", ix, iz);
	}
    }
    /*den[ix+1/2][iz]*/
    for ( ix = 0; ix < nxb-1; ix++) {
	for (iz = 0; iz < nzb; iz++) {
	    denx[ix][iz] = (den[ix+1][iz] + den[ix][iz])*0.5;
	}
    }
    
    /*den[ix][iz+1/2]*/
    for ( ix = 0; ix < nxb; ix++) {
	for (iz = 0; iz < nzb-1; iz++) {
	    denz[ix][iz] = (den[ix][iz+1] + den[ix][iz])*0.5;
	}
    } 

    source = sf_floatalloc(nt);
    sf_floatread(source, nt, fsource);
    
    txxn1 = sf_floatalloc2(nzb, nxb);
    txxn0 = sf_floatalloc2(nzb, nxb);
    vxn1  = sf_floatalloc2(nzb, nxb);
    vzn1  = sf_floatalloc2(nzb, nxb);
    vxn0  = sf_floatalloc2(nzb, nxb);
    vzn0  = sf_floatalloc2(nzb, nxb);

    init_pml(nz, nx, dt, pmlout, marg, pmld0, decay, decaybegin, gamma);
    
    for (ix = 0; ix < nxb; ix++) {
	for (iz = 0; iz < nzb; iz++) {
	    txxn1[ix][iz] = 0.0;
	 }
    }
    for (ix = 0; ix < nxb; ix++) {
	for (iz = 0; iz < nzb; iz++) {
	    txxn0[ix][iz] = 0.0;
	}
    }
    for (ix = 0; ix < nxb; ix++) {
	for (iz = 0; iz < nzb; iz++) {
	    vxn1[ix][iz] = 0.0;  
	}
    }
    for (ix = 0; ix < nxb; ix++) {
	for (iz = 0; iz < nzb; iz++) {
	    vxn0[ix][iz] = 0.0;
	}
    } 
    for (ix = 0; ix < nxb; ix++) {
	for (iz = 0; iz < nzb; iz++) {
	    vzn1[ix][iz] = 0.0;  
	}
    }
    for (ix = 0; ix < nxb; ix++) {
	for (iz = 0; iz < nzb; iz++) {
	    vzn0[ix][iz] = 0.0;
	}
    }  
   
    /* MAIN LOOP */
    sp.trunc=160;
    sp.srange=10;
    sp.alpha=0.5;
    sp.decay=1;
	
    sf_warning("============================");
    sf_warning("nx=%d nz=%d nt=%d", nx, nz, nt);
    sf_warning("dx=%f dz=%f dt=%f", dx, dz, dt);
    sf_warning("lenx=%d lenz=%d marg=%d", lenx, lenz, marg);
    for(ix=0; ix<lenx; ix++){
	sf_warning("[sxx,sxz]=[%d,%d] Gx=%f",sxx[ix], sxz[ix], Gx[ix][0][0]);
    }
    for(ix=0; ix<lenz;ix++){
	sf_warning("[szx,szz]=[%d,%d] Gz=%f",szx[ix], szz[ix], Gz[ix][0][0]);
    } 
    
    for (it = 0; it < nt; it++) {
	if (it%10==0) sf_warning("it=%d;", it);
	if (it<=sp.trunc) {
	    explsourcet(txxn0, source, it, spx+pmlout+marg, spz+pmlout+marg, nxb, nzb, &sp);
	}
    
	/*velocity*/
	for (ix = marg+pmlout; ix < nx+pmlout+marg; ix++ ) {
	    for (iz = marg+pmlout; iz < nz+pmlout+marg; iz++) {
		vxn1[ix][iz] = vxn0[ix][iz] - dt/denx[ix][iz]*ldx(txxn0, ix, iz);
		vzn1[ix][iz] = vzn0[ix][iz] - dt/denz[ix][iz]*ldz(txxn0, ix, iz);
	    }
	}

	/*Velocity PML */
	pml_vxz(vxn1, vzn1, vxn0, vzn0, txxn0, denx, denz, ldx, ldz);

	/*Stress*/
	for (ix = marg+pmlout; ix < nx+marg+pmlout; ix++) {
	    for ( iz = marg+pmlout; iz < nz+marg+pmlout; iz++) { 
		txxn1[ix][iz] = txxn0[ix][iz] - dt*c11[ix][iz]*(ldx(vxn1, ix+1, iz) + ldz(vzn1, ix, iz+1));
	    }
	}
	/*Stress PML */
	pml_txx(txxn1, vxn1, vzn1, c11, ldx, ldz);

	/*exchange n1 -> n0*/
	time_step_exch(txxn0, txxn1, it);
	time_step_exch(vxn0, vxn1, it);
	time_step_exch(vzn0, vzn1, it);
	pml_tstep_exch(it);
	

	
	for ( ix = 0; ix < nxb; ix++) {
	    sf_floatwrite(txxn0[ix], nzb, fwf);
	}
	
    }/*End of LOOP TIME*/
    sf_warning(".");

    tend = clock();
    duration=(double)(tend-tstart)/CLOCKS_PER_SEC;
    sf_warning(">> The CPU time of sfsglfd2 is: %f seconds << ", duration);

    exit(0);
}    
    
    
    
    
    

    
