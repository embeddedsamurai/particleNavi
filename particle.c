/**------------------------------------------------------------------------

 Program : Particle Filter for c

 Explanation : 
  1) Make Draft cource by length and angle each time step.
  2) Length and angle have gaussian noise.
  3) robot can observe distance from 4 land mark point.
     Each istance has gaussian noise within scope range only.
  4) Particles(100) is strewed around moving point by length and angle.
  5) Calculate each likehood of particle by distances form land mark.
  6) Estimate localization by all particles.
  7) If likehood has large difference each other, resamplingt is made.

 Quation :
  This program is transrated from matlab below code.
  Thanks to Auther of the matlab code.
  http://myenigma.hatenablog.com/entry/20140628/1403956852

 Environment : C

 Author : Masato Nakai
 -----------------------------------------------------------------------**/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "mxlib.h"
#include "usrlib.h"

/** 
function [] = ParticleFilterLocalization()
**/ 

#define MAX_RANGE 20        //�ő�ϑ�����
#define NP        100       //�p�[�e�B�N����
#define NTh       NP/2.0    //���T���v�����O�����{����L���p�[�e�B�N����
#define DIM       3
#define EPS       0.0000000001

double toRadian(double);
double toDegree(double);
double doControl(double,double *);
int    Observation();
void   factor(double *,double *,double);
double Gauss(double,double,double);
void   Normalize(double *,int);
void   Resampling(double **,double *,double,int);
double pNormR(double);
 
// Main loop
int main(argc,argv)
int argc;
char *argv[];
{
  int i,j;  

  double endtime; // [sec]
  double time,dt;
  int nSteps;

  int ip;
  int iz;

  double xEst[]={0.0, 0.0, 0.0}; //[x y yo]
  // State Vector [x y yaw]'
  /* xEst=[0 0 0]'; */
  double xTrue[DIM]; 
  double xd[DIM];
  double **px;      //�e�p�[�e�B�N���̐���ʒu
  double pw[NP];    //�d�ݕϐ�

  double u[2];
  //�ϑ��ʒuRFIF�^�O�̈ʒu [x, y]
  /**
  RFID=[10 0;
        10 10;
        0  15
        -5 20];
  **/
  double RFID[][2]={{10,0},{10,10},{0,15},{-5,20}};

  double **Qsigma;
  double **Q;
  double Rsigma;
  double vec[DIM];
  double **z;
  double R,w,pz,dz;
  double x[DIM];
  double **XRFID;
  int    izn;

  FILE *fdbg;

  if(argc < 2) {
    fprintf(stderr,"USAGE command traceFile\n");
    exit(-9);
  }
  if(!(fdbg = fopen(argv[1],"w"))) {
    fprintf(stderr,"Cannot write tracefile=[%s]\n",argv[1]);
  }

  time=0;   //sec
  dt = 0.1; 
  endtime = 60; // [sec]
  //nSteps = ceil((endtime - time)/dt);
  nSteps = (int)((endtime - time)/dt+0.5);
  
  /* initialize */
  px=(double **)comMxAlloc(DIM,NP,sizeof(double));
  for(i=0;i<NP;i++) {
    for(j=0;j<DIM;j++) {
      px[j][i] = xEst[j];
    }
  }

  for(i=0;i<NP;i++) {
    pw[i] = 1.0/NP;
  }
  Q=(double **)comMxAlloc(DIM,DIM,sizeof(double));

  vec[0] = pow(0.1,2);
  vec[1] = pow(0.1,2);
  vec[2] = pow(toRadian(3),2);
  mxDiag(Q,DIM,vec);
  // Covariance Matrix for predict
  /* Q=diag([0.1 0.1 toRadian(3)]).^2; */

  // Simulation parameter
  Qsigma=(double **)comMxAlloc(2,2,sizeof(double));

  vec[0] = pow(0.1,2);
  vec[1] = pow(toRadian(5),2);
  mxDiag(Qsigma,2,vec);
  /* 
  global Qsigma 
  Qsigma=diag([0.1 toRadian(5)]).^2;
  */

  Rsigma = pow(0.1,2);
  /* 
  global Rsigma
  Rsigma=diag([0.1]).^2;
  */
  R = 1.0;
  // Covariance Matrix for observation
  /* R=diag([1]).^2; */ //[range [m]
  
  /* px=repmat(xEst,1,NP); */ //�p�[�e�B�N���i�[�ϐ�
  /* pw=zeros(1,NP)+1/NP; */  //�d�ݕϐ�
  for(j=0;j<DIM;j++) {
    xTrue[j]=xEst[j];
    xd   [j]=xEst[j];
  }
  //True State
  /* xTrue=xEst; */
 
  // Dead Reckoning Result
  /* xd=xTrue; */
  /* �ϑ��_����̋����i�m�C�Y�t���j�Ɗϑ��ʒu�̎��[�ꏊ */
  XRFID = (double **)comMxAlloc(4,2,sizeof(double));
  for(i=0;i<4;i++) for(j=0;j<2;j++) XRFID[i][j]=RFID[i][j];
  z    = (double **)comMxAlloc(4,3,sizeof(double));

  for(i=0; i <nSteps; i++) {
    time = time + dt;
    /* Input out u[0]:r�i�s�� u[1]:�ƕ����� time ->(1 5�x) �ɑQ�� */ 
    doControl(time,u);
    /* Observation  
      out xTrue[0]:X�� dt*r*cos�� x[0]: Y�� dt*r*sin�� x[2]: �p�x dt*��
      out xd : xTrue�̃m�C�Y�t��
      out z:�ϑ��\(MAX_RANGE�ȉ�)���̊ϑ��_����̋���(�m�C�Y)�Ɗϑ��_�̈ʒu
    */
    izn=Observation(xTrue, xd, u, XRFID, MAX_RANGE,Qsigma,Rsigma,dt,z);
    
    // ------ Particle Filter --------
    for(ip=0; ip <NP ; ip++) {
      for(j=0;j<DIM;j++) {
        x[j]=px[j][ip];
      }
      w=pw[ip];
      /* u�ňړ� */
      factor(x,u,dt);

      for(j=0;j<DIM;j++) {
        // Dead Reckoning and random sampling
        // ip�Ԗڗ��q�̈ʒu�Ƀm�C�Y��ǉ� sqrt(Q)=[0.1 0.1 3�x]
        x[j] += sqrt(Q[j][j])*pNormR(rand()/(double)RAND_MAX);
        //x=f(x, u)+sqrt(Q)*randn(3,1);
      }
      // Calc Inportance Weight
      for(iz=0; iz <izn /*length(z(:,1)*/; iz++) {
        for(pz=0,j=0;j<2;j++) {
          pz += pow(x[j]-z[iz][j+1],2);
        }
        //�e�ϑ��_����̋�����ip�Ԗڗ��q�̐���ʒu����̋����̍�
        dz=sqrt(pz)-z[iz][0];  
        //���q�ޓx �� dz�̃K�E�X�m���̐ςōX�V
        w=w*Gauss(dz,0,sqrt(R));        
      }
      for(j=0;j<DIM;j++) {  
        px[j][ip]=x[j];  //Q�̃m�C�Y��^��������ʒu���i�[
      }
      pw[ip]=w;
      
    }
    
    Normalize(pw,NP);//���K��
    //���T���v�����O
    Resampling(px,pw,NTh,NP); 

    for(j=0;j<DIM;j++) xEst[j]=0;
    for(j=0;j<DIM;j++) {
      for(ip=0;ip<NP;ip++) {
        xEst[j] += px[j][ip]*pw[ip]; //�ŏI����l�͊��Ғl
      }
    }
    if(i == 0) {
      fprintf(fdbg,"$no,xT,yT,tT,xD,yD,tD,xE,yE,tE\n");
    }
    fprintf(fdbg,"%d,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf\n",i,xTrue[0],xTrue[1],xTrue[2],xd[0],xd[1],xd[2],xEst[0],xEst[1],xEst[2]);
  }
  fclose(fdbg);

  /* �̈�̊J�� */
  comMxFree(px,DIM,NP);
  comMxFree(Q,DIM,DIM);
  comMxFree(Qsigma,2,2);
  comMxFree(XRFID,4,2);
  comMxFree(z,4,3);
  

  return(0);

}
/***********
  pNormR
************/
double pNormR(double rnd)
{
   if(rnd <= 0) rnd += EPS;
   if(rnd >= 1) rnd -= EPS;
   return(comPnorm(rnd));
}

/*********************************************/
/* function [px,pw]=Resampling(px,pw,NTh,NP) */
/*********************************************/
void Resampling(double **px,double *pw,double NThv,int NPS)
{
//���T���v�����O�����{����֐�
//�A���S���Y����Low Variance Sampling
  double Neff;
  int    i,j,ind,ip;
  double sum;
  double wcum[NP];
  double base[NP];
  double resampleID[NP];
  double ppx[DIM][NP];
  double random;

  for(sum=0,i=0;i<NPS;i++) {
    sum += pow(pw[i],2);
  }
  Neff = 1.0/sum;
  //Neff=1.0/(pw*pw');

  if(Neff<NThv) {  //���T���v�����O
    wcum[0] = pw[0];
    for(i=1;i<NPS;i++) {
      wcum[i] = wcum[i-1] + pw[i];
    }
    //wcum=cumsum(pw);
    base[0] = 0.0;
    for(i=1;i<NPS;i++) {
      base[i] = base[i-1] + 1.0/NPS;
    }
    random = (rand()/(double)RAND_MAX)/(double)NPS;
    for(i=0;i<NPS;i++) {
      resampleID[i] = base[i] + random;
    }
    //base=cumsum(pw*0+1/NP)-1/NP; //������������O��base
    //resampleID=base+rand/NP;     //���[���b�g�𗐐������₷
    for(j=0;j<DIM;j++) {
      for(i=0;i<NPS;i++) {
        ppx[j][i] = px[j][i];
      }
    }
    //ppx=px;//�f�[�^�i�[�p
    ind=0;//�V����ID
    for(ip=0; ip <NPS; ip++) {
      while(resampleID[ip]>wcum[ind]) {
            ind=ind+1;
      }
      for(j=0;j<DIM;j++) {
        px[j][ip]=ppx[j][ind]; //LVS�őI�΂ꂽ�p�[�e�B�N���ɒu������
      }
      pw[ip]=1.0/NPS;         //�ޓx�͏�����        
    }
  }
}

/********************************/
/* function pw=Normalize(pw,NP) */
/********************************/
void Normalize(double *pw,int NPS) 
{
  //�d�݃x�N�g���𐳋K������֐�
  int i;
  double sumw;
  for(sumw=0.0,i=0;i<NPS;i++) {
    sumw += pw[i];
  }
  //sumw=sum(pw);
  if(sumw != 0) {
    for(i=0;i<NPS;i++) {
      pw[i]=pw[i]/sumw;//���K��
    }
  }
  else {
    for(i=0;i<NPS;i++) {
      pw[i] = 1.0/NPS;
    }
    //pw=zeros(1,NP)+1/NP;
  }
}
/*******************************/    
/* function p=Gauss(x,u,sigma) */
/*******************************/
double Gauss(double x,double u,double sigma)
{
//�K�E�X���z�̊m�����x���v�Z����֐�
  double p;
  p=1.0/sqrt(2*PAI*pow(sigma,2))*exp(-pow(x-u,2)/(2*pow(sigma,2)));
  return(p);
}
/************************/
/* function x = f(x, u) */
/************************/
void factor(double *x, double *u,double dt)
{
// Motion Model
  int i,j;
  double **FM,**B;
  //global dt;
 
  /*
  F = [1 0 0
       0 1 0
       0 0 1];
  */
  double F[][3] = {{1,0,0},{0,1,0},{0,0,1}};
  /*
  B = [
        dt*cos(x(3)) 0
        dt*sin(x(3)) 0
        0 dt];
  */
  //double B[3][2];
  double vec1[3],vec2[3];

  FM=(double **)comMxAlloc(3,3,sizeof(double));
  B=(double **)comMxAlloc(3,2,sizeof(double));

  for(i=0;i<3;i++) for(j=0;j<3;j++) FM[i][j]=F[i][j];

  B[0][0] = dt*cos(x[2]); B[0][1] = 0;
  B[1][0] = dt*sin(x[2]); B[1][1] = 0;
  B[2][0] = 0;            B[2][1] = dt;

  mxVecR(FM,3,3,x,vec1);
  mxVecR(B,3,2,u,vec2);

  for(i=0;i<3;i++) {
    x[i] = vec1[i] + vec2[i];
  }

  comMxFree(FM,3,3);
  comMxFree(B,3,2);
  //x= F*x+B*u;
}
/*****************/
/* function u = doControl(time) */
/*****************/
double doControl(double time,double *u)
{
 //Calc Input Parameter
  int T;
  double V;
  double yawrate;

  T=10; // [sec]
  // [V yawrate]
  V=1.0; // [m/s]
  yawrate = 5; // [deg/s]
 
  //u =[ V*(1-exp(-time/T)) toRadian(yawrate)*(1-exp(-time/T))]';
  u[0] = V*(1-exp(-time/T));
  u[1] = toRadian(yawrate)*(1-exp(-time/T));

  return(0);
}
/***************
Calc Observation from noise prameter
****************/
/* function [z, x, xd, u] = Observation(x, xd, u, RFID,MAX_RANGE) */
int Observation(x, xd, u, RFID, MAX_FRANGE, Qsigma, Rsigma, dt, z)
double *x;
double *xd;
double *u;
double **RFID;
int MAX_FRANGE;
double **Qsigma;
double Rsigma;
double dt;
double **z;
{
  //global Qsigma;
  //global Rsigma;
  int iz,j,izn;
  double d;


  factor(x, u, dt); // Ground Truth
  u[0] += sqrt(Qsigma[0][0]) * pNormR(rand()/(double)RAND_MAX);
  u[1] += sqrt(Qsigma[1][1]) * pNormR(rand()/(double)RAND_MAX);
  //u=u+sqrt(Qsigma)*randn(2,1); //add Process Noise
  factor(xd, u, dt);// Dead Reckoning

  //Simulate Observation
  //z=[];
  izn=0;
  for(iz=0; iz <4 /*length(RFID(:,1))*/; iz++) {
    //d=norm(RFID(iz,:)-x(1:2)');
    for(d=0,j=0;j<2;j++) {
      //d += pow(RFID[iz][j] - x[j],2);
      d += pow(RFID[iz][j] - xd[j],2);
    }
    d = sqrt(d);
    if(d <MAX_FRANGE) { //�ϑ��͈͓�
        z[iz][0] = d+sqrt(Rsigma)*pNormR(rand()/(double)RAND_MAX);
        z[iz][1] = RFID[iz][0];
        z[iz][2] = RFID[iz][1];   
        izn++;
    }
  }
  return(izn);
}


/* function radian = toRadian(degree) */
/******************
 degree to radian
*******************/
double toRadian(double degree)
{
  double radian;
  radian = degree/180*PAI;
  return(radian);
}
/* function degree = toDegree(radian) */
/*******************
 radian to degree
*******************/
double toDegree(double radian)
{
  double degree;
  degree = radian/PAI*180;
  return(degree);
}
