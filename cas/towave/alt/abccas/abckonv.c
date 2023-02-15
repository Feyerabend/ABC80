/***************************************************
* Abckonv - by Robert Juhasz, 2008
*
* usage: abckonc < fil.bas > fil2.bas
*
* strips unwanted linefeeds from ABC80 .bas files
****************************************************/
#include <stdio.h>


int main()
{
int c,oldc,wp,lp;
oldc=0;
while (!feof(stdin))
{
c=fgetc(stdin);
if (c==10) c=13;
if (c!=13) { fputc(c,stdout); }
if ((c==13) && (oldc!=13)){ fputc(c,stdout);}

oldc=c;
}

}