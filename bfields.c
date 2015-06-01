
#include "paul.h"

void initial( double * , double * ); 
double get_moment_arm( double * , double * );
double get_dA( double * , double * , int );
double get_dV( double * , double * );
void setup_faces( struct domain * , struct face ** , int * , int );
int get_num_rzFaces( int , int , int );
void B_faces_to_cells( struct domain * , int , int );
 
void set_B_fields( struct domain * theDomain ){

   int i,j,k;
   struct cell ** theCells = theDomain->theCells;
   int Nr = theDomain->Nr;
   int Nz = theDomain->Nz;
   int * Np = theDomain->Np;
   double * r_jph = theDomain->r_jph;
   double * z_kph = theDomain->z_kph;

   for( j=0 ; j<Nr ; ++j ){
      for( k=0 ; k<Nz ; ++k ){
         int jk = j+Nr*k;
         for( i=0 ; i<Np[jk] ; ++i ){
            struct cell * c = &(theCells[jk][i]);
            double phip = c->piph;
            double phim = phip-c->dphi;
            double xp[3] = { r_jph[j]   , phip , z_kph[k]   };
            double xm[3] = { r_jph[j-1] , phim , z_kph[k-1] };
            double r = get_moment_arm( xp , xm );
            double x[3] = { r , phip , .5*(z_kph[k]+z_kph[k-1])};
            double prim[NUM_Q];
            initial( prim , x ); 
            double dA = get_dA( xp , xm , 0 );
            double Phi = 0.0;
            if( NUM_Q > BPP ) Phi = prim[BPP]*dA;
            c->Phi[0] = Phi;
         }    
      }    
   }

   int NRZ1 = get_num_rzFaces( Nr , Nz , 1 );
   int * nfr = (int *) malloc( (NRZ1+1)*sizeof(int) );
   setup_faces( theDomain , &(theDomain->theFaces_1) , nfr , 1 ); 

   int n;
   for( n=0 ; n<nfr[NRZ1] ; ++n ){
      struct face * f = theDomain->theFaces_1 + n;
      double prim[NUM_Q];
      initial( prim , f->cm );
      double Phi = 0.0;
      if( NUM_Q > BRR ) Phi = prim[BRR]*f->dA;
      if( f->LRtype == 0 ){
         f->L->Phi[2] = Phi;
      }else{
         f->R->Phi[1] = Phi;
      }
   }

   B_faces_to_cells( theDomain , nfr[NRZ1] , 0 );
   B_faces_to_cells( theDomain , nfr[NRZ1] , 1 );
 
   free( nfr );
   free( theDomain->theFaces_1 );

}

void B_faces_to_cells( struct domain * theDomain , int Nf , int type ){

   struct cell ** theCells = theDomain->theCells;
   struct face * theFaces_1 = theDomain->theFaces_1;
   
   int Nr = theDomain->Nr;
   int Nz = theDomain->Nz;
   int * Np = theDomain->Np;
   double * r_jph = theDomain->r_jph;
   double * z_kph = theDomain->z_kph;
   
   int i,j,k;

   for( j=0 ; j<Nr ; ++j ){
      for( k=0 ; k<Nz ; ++k ){
         int jk = j+Nr*k;
         for( i=0 ; i<Np[jk] ; ++i ){
            struct cell * c = &(theCells[jk][i]);
            c->tempDoub = 0.0;
            if( type==0 ){
               c->prim[BRR] = 0.0;
               c->prim[BPP] = 0.0;
            }else{
               c->cons[BRR] = 0.0;
               c->cons[BPP] = 0.0;
            }
         }
      }
   }   

   int n;
   for( n=0 ; n<Nf ; ++n ){
      struct face * f = theFaces_1 + n;
      struct cell * cL = f->L;
      struct cell * cR = f->R;
      double Phi;
      if( f->LRtype==0 ){
         Phi = cL->Phi[2];
      }else{
         Phi = cR->Phi[1];
      }
      cL->tempDoub += f->dA;
      cR->tempDoub += f->dA;

      if( type==0 ){
         cL->prim[BRR] += Phi;
         cR->prim[BRR] += Phi;
      }else{
         cL->cons[BRR] += Phi;
         cR->cons[BRR] += Phi;
      }
   }
 
   for( j=0 ; j<Nr ; ++j ){
      for( k=0 ; k<Nz ; ++k ){
         int jk = j+Nr*k;
         for( i=0 ; i<Np[jk] ; ++i ){
            int im = i-1;
            if( im==-1 ) im = Np[jk]-1;

            struct cell * c = &(theCells[jk][i]);
            struct cell * cm = &(theCells[jk][im]);

            double xp[3] = { r_jph[j]   , c->piph  , z_kph[k]   };
            double xm[3] = { r_jph[j-1] , cm->piph , z_kph[k-1] };
            double r = get_moment_arm( xp , xm );
            double dA = get_dA( xp , xm , 0 );
            double dV = get_dV( xp , xm );

            if( type==0 ){
               c->prim[BRR] /= c->tempDoub;
               c->prim[BPP] = .5*(c->Phi[0]+cm->Phi[0])/dA;
            }else{
               c->cons[BRR] *= dV/r/c->tempDoub;
               c->cons[BPP] = .5*(c->Phi[0]+cm->Phi[0])*dV/dA/r;
            }
         }
      }
   }

}


void update_B_fluxes( struct domain * theDomain , int * Nf , int Njk , double dt ){

   struct face * theFaces_1 = theDomain->theFaces_1;

   int n;
   int jk;
   int n0 = 0;
   for( jk=0 ; jk<Njk ; ++jk ){
      for( n=0 ; n<Nf[jk]-n0 ; ++n ){
         struct face * f  = theFaces_1 + n0 + n;
         int np = n+1;
         if( np==Nf[jk]-n0 ) np=0; 
         struct face * fp = theFaces_1 + n0 + np;

         double E;
         double dl = 1.0;
         if( f->LRtype == 0 ){
            E = f->L->E[1]*dl;
            f->L->Phi[2] -= E*dt;
            f->L->Phi[0] += E*dt;
         }else{
            E = f->R->E[0]*dl;
            f->R->Phi[1] -= E*dt;
            f->R->Phi[0] -= E*dt;
         }
         if( fp->LRtype == 0 ){
            fp->L->Phi[2] += E*dt;
         }else{
            fp->R->Phi[1] += E*dt;
         }
      }
      n0 = Nf[jk];
   }
 
}

double get_dp( double , double );

void avg_Efields( struct domain * theDomain , int Nf ){

   int i,j,k;
   struct cell ** theCells = theDomain->theCells;
   int Nr = theDomain->Nr;
   int Nz = theDomain->Nz;
   int * Np = theDomain->Np;

   for( j=0 ; j<Nr ; ++j ){
      for( k=0 ; k<Nz ; ++k ){
         int jk = j+Nr*k;
         for( i=0 ; i<Np[jk] ; ++i ){
            struct cell * c  = theCells[jk]+i;
            int ip = (i+1)%Np[jk];
            struct cell * cp = theCells[jk]+ip;

            double El_avg = .5*( c->E[0] + cp->E[2] );
            double Er_avg = .5*( c->E[1] + cp->E[3] );

             c->E[0] = El_avg;
             c->E[1] = Er_avg;
            cp->E[2] = El_avg;
            cp->E[3] = Er_avg;

            double Bl_avg = .5*( c->B[0] + cp->B[2] );
            double Br_avg = .5*( c->B[1] + cp->B[3] );

             c->B[0] = Bl_avg;
             c->B[1] = Br_avg;
            cp->B[2] = Bl_avg;
            cp->B[3] = Br_avg;

         }
      }
   }

   struct face * theFaces = theDomain->theFaces_1;
   int n;
   for( n=0 ; n<Nf ; ++n ){
      struct face * f = theFaces+n;
      struct cell * c1;
      struct cell * c2;
      if( f->LRtype == 0 ){
         c1 = f->L;
         c2 = f->R;
      }else{
         c1 = f->R;
         c2 = f->L;
      }
      double p1 = c1->piph;
      double p2 = c2->piph;
      double dp1 = get_dp(p2,p1);
      double dp2 = c2->dphi - dp1;
      if( f->LRtype == 0 ){
         double Eavg = ( dp2*c2->E[0] + dp1*c2->E[2] )/(dp1+dp2);
         double Bavg = ( dp2*c2->B[0] + dp1*c2->B[2] )/(dp1+dp2);
         f->E = .5*(f->L->E[1] + Eavg);
         f->B = .5*(f->L->B[1] + Bavg);
      }else{
         double Eavg = ( dp2*c2->E[1] + dp1*c2->E[3] )/(dp1+dp2);
         double Bavg = ( dp2*c2->B[1] + dp1*c2->B[3] )/(dp1+dp2);
         f->E = .5*(f->R->E[0] + Eavg);
         f->B = .5*(f->R->B[0] + Bavg);
      }
   }

   for( n=0 ; n<Nf ; ++n ){
      struct face * f = theFaces+n;
      if( f->LRtype==0 ){
         f->L->E[1] = f->E;
         f->L->B[1] = f->B;
      }else{
         f->R->E[0] = f->E;
         f->R->B[0] = f->B;
      }
      f->E = 0.0;
      f->B = 0.0;
   }

   for( j=0 ; j<Nr ; ++j ){
      for( k=0 ; k<Nz ; ++k ){
         int jk = j+Nr*k;
         for( i=0 ; i<Np[jk] ; ++i ){
            struct cell * c  = theCells[jk]+i;
            int ip = (i+1)%Np[jk];
            struct cell * cp = theCells[jk]+ip;

            cp->E[2] = c->E[0];   
            cp->E[3] = c->E[1];   
            cp->B[2] = c->B[0];   
            cp->B[3] = c->B[1];  

         }
      }
   } 
}

void subtract_advective_B_fluxes( struct domain * theDomain ){

   int i,j,k;
   struct cell ** theCells = theDomain->theCells;
   int Nr = theDomain->Nr;
   int Nz = theDomain->Nz;
   int * Np = theDomain->Np;
   double * r_jph = theDomain->r_jph;

   for( j=0 ; j<Nr ; ++j ){
      double rp = r_jph[j]; 
      double rm = r_jph[j-1];
      for( k=0 ; k<Nz ; ++k ){
         int jk = j+Nr*k;
         for( i=0 ; i<Np[jk] ; ++i ){
            struct cell * c  = theCells[jk]+i;

            double wm = c->wiph;
            double wp = c->wiph;

            c->E[0] -= wm*c->B[0];
            c->E[1] -= wp*c->B[1];

         }
      }
   }

}



