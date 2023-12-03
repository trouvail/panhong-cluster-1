#include <gmpxx.h>
#include <iostream>
#include <stdio.h>
using namespace std;
int main()
{
        mpz_t a, b, c;
        mpz_init(a);
        mpz_init(b);
        mpz_init(c);
        printf("========= Input a and b => Output a + b =========\n");
        printf("[-] a = ");
        gmp_scanf("%Zd", a);
        printf("[-] b = ");
        gmp_scanf("%Zd", b);
        mpz_add(c, a, b);
        gmp_printf("[+] c = %Zd\n", c);
        return 0;
}
// g++ zyltest-gmp.cpp -o zyltest-gmp -lgmp -lm