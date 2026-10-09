/***************************************************
* ABCcas - by Robert Juhasz, 2008
*
* usage: abccas fil.bac
*
* generates fil.bac.WAV which can be loaded by ABC80 (LOAD CAS:)
* Filename in wav-file is always TESTTT.BAC
****************************************************/
 #include <sys/types.h>
     #include <sys/stat.h>
#include <string.h>
#include <stdio.h>

unsigned char buffer[256];
char outname[80];
char name[8]="TESTTT  ";
char ext[3]="BAC";
static int outbit=150;

void byteout(unsigned char b,FILE *f)
{
int i,t=1,ofs;
ofs=64;
for (i=0;i<8;i++)
 {
	if(outbit) outbit=0; else outbit=150; // flip output bit
	fputc(outbit+ofs,f);
	fputc(outbit+ofs,f);
	fputc(outbit+ofs,f);
	 fputc(outbit+ofs,f);
	fputc(outbit+ofs,f);
	fputc(outbit+ofs,f);
	fputc(outbit+ofs,f);
	 fputc(outbit+ofs,f);
	fputc(outbit+ofs,f);
	fputc(outbit+ofs,f);
	fputc(outbit+ofs,f);
	 fputc(outbit+ofs,f);
	fputc(outbit+ofs,f);
	fputc(outbit+ofs,f);
	fputc(outbit+ofs,f);
	 fputc(outbit+ofs,f);

	if (t & b) {
	// send "1"
	if(outbit) outbit=0; else outbit=150; // flip output bit again if "1"
	}
	fputc(outbit+ofs,f);
	fputc(outbit+ofs,f);
	fputc(outbit+ofs,f);
	fputc(outbit+ofs,f);
		fputc(outbit+ofs,f);
	fputc(outbit+ofs,f);
	fputc(outbit+ofs,f);
	fputc(outbit+ofs,f);
	fputc(outbit+ofs,f);
	fputc(outbit+ofs,f);
	fputc(outbit+ofs,f);
	fputc(outbit+ofs,f);
	fputc(outbit+ofs,f);
	fputc(outbit+ofs,f);
	fputc(outbit+ofs,f);
	fputc(outbit+ofs,f);

	t*=2; // next bit
 }
}

void blockout(FILE *f)
{
int i,csum;
for (i=0;i<32;i++) byteout(0,f); // 32 0 bytes
for (i=0;i<3;i++) byteout(0x16,f); // 3 sync bytes 16H
byteout(0x2,f); //STX
for (i=0;i<256;i++) byteout(buffer[i],f); // output the buffer
byteout(0x3,f); // ETX
csum=0;
for (i=0;i<256;i++) csum+=buffer[i]; // calculate the checksum
csum+=3; // csum includes ETX char!!! (as correctly stated in Mikrodatorns ABC)
byteout(csum & 0xff,f);
byteout((csum >> 8) & 0xff,f);
}

void nameblockout(FILE *f)
{
	int i;
for (i=0;i<3;i++) buffer[i]=0xff; // Header
for (i=3;i<11;i++) buffer[i]=name[i-3]; // Name
for (i=11;i<14;i++) buffer[i]=ext[i-11]; // Ext
for (i=14;i<256;i++) buffer[i]=0; // zeroes
blockout(f);
}

void datablockout(FILE *fin,FILE *fout)
{
int blcnt;
blcnt=0;
while (!feof(fin))	
	{
	buffer[0]=0;
	buffer[1]=blcnt & 0xff;
	buffer[2]=(blcnt >> 8) & 0xff;	
	fread(&buffer[3],253,1,fin);
	blockout(fout);
	blcnt++;
	printf("Block out #%d\n",blcnt);
	}

}


void wavheaderout(FILE *f,int numsamp)
{
	int totlen,srate;
fprintf(f,"%s","RIFF");	
totlen=12-8+24+8+numsamp;
fputc((totlen & 0xff),f);
fputc((totlen >> 8) & 0xff,f);
fputc((totlen >> 16) & 0xff,f);
fputc((totlen >> 24) & 0xff,f);


srate=5977*2*2;	
fprintf(f,"%s","WAVE");
fprintf(f,"%s","fmt ");
fprintf(f,"%c%c%c%c",0x10,0,0,0); //format chunk length
fprintf(f,"%c%c",0x1,0x0); // always 0x1
fprintf(f,"%c%c",0x1,0x0); //always 0x01=Mono, 0x02=Stereo)
fprintf(f,"%c%c%c%c",srate&255,srate>>8,0,0); //	Sample Rate (Binary, in Hz)
fprintf(f,"%c%c%c%c",srate&255,srate>>8,0,0); //	BYtes /s same as sample rate if mono 8bit
fprintf(f,"%c%c",0x01,0); //	Bytes Per Sample: 1=8 bit Mono, 2=8 bit Stereo or 16 bit Mono, 4=16 bit Stereo
fprintf(f,"%c%c",0x08,0); //	Bits Per Sample
fprintf(f,"%s","data");// data hdr (ASCII Characters)
fprintf(f,"%c%c%c%c",numsamp &0xff,(numsamp >>8)&0xff,(numsamp >>16)&0xff,(numsamp >>24)&0xff); //	Length Of Data To Follow

}

int main(char argc, char *argv[])
{
int filelen,numblk,numsamp,numbyte,blockcnt;
struct stat filestat;
FILE *fin,*fout;
printf("%d args, %s %s %s\n",argc,argv[0],argv[1],argv[2]);	
strcpy(outname,argv[1]);
strcat(outname,".WAV");
fin=fopen(argv[1],"rb");
fout=fopen(outname,"wb");
stat(argv[1],&filestat);
filelen=filestat.st_size;

numblk=filelen / 253;
if (filelen % 253) numblk++; //if not exact add block	
numblk++; // add one for name block	
numbyte=(7+32+256)*numblk; //256+32+7 bytes per block
numsamp=32*8*numbyte; // 32 samples per bit, 8 bits per byte
printf("Size:%d Blk:%d Byte:%d Samp:%d\n",filelen,numblk,numbyte,numsamp);
wavheaderout(fout,numsamp);

nameblockout(fout);
datablockout(fin,fout);
fclose(fin);
fclose(fout);
	
}
