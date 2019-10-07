#include <stdio.h>

static int addr[32];

void ten_two(unsigned int decimal)
{
       unsigned long long binary=0;
       unsigned long long by=0;
       unsigned long long base = 1;
       int flag=28;
       int i;
       while(decimal > 0){
                for(i=0;i<4;i++){
                        binary = binary + (decimal%2)*base;
                        decimal /= 2;
                        base = base*10;
                }
                base = 1000;
                for(i=flag;i<4+flag;i++){
                        addr[i] = binary/base;
                        binary %= base;
                        base /= 10;
                }
                binary = 0;
                base = 1;
                flag = flag - 4;
       }
}

unsigned long long two_ten(unsigned long long binary)
{
        int decimal=0;
        int base = 1;

        while(binary>0){
                decimal += (binary%10)*base;
                binary /= 10;
                base *= 2;
        }
        return decimal;
}

void vtop()
{
        int i;
        unsigned long long base = 1000000000;
        unsigned long long pdt;
        unsigned long long pt;
        unsigned long long offset;

        for(i=0;i<10;i++){
                pdt += addr[i]*base;
                base /= 10;
        }
        pdt = two_ten(pdt);

        base = 1000000000;
        for(i=0;i<10;i++){
                pt += addr[i+10]*base;
                base /= 10;
        }
        pt = two_ten(pt);

        base = 100000000000;
        for(i=0;i<12;i++){
                offset += addr[20+i]*base;
                base /= 10;
        }
        offset = two_ten(offset);

        printf("PDT[%lld]におけるPDEの先頭20bitの物理アドレスにあるPTの%lld番目のPTEの先頭20bitの物理アドレスから%lldbyte進んだアドレス\n",pdt,pt,offset);
}

int main()
{
        int i;
        unsigned int v_addr;
        for(i=0;i<32;i++){
                addr[i] = 0; 
        }
        printf("Please type virtual address for the physical address\n");
        scanf("%x",&v_addr);
        ten_two(v_addr);
        vtop();
        return 0;
}
