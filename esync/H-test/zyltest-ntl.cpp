#include <iostream>
#include <ctime>
#include <NTL/ZZ.h>

using namespace std;
using namespace NTL;

// L函数
ZZ L_function(const ZZ &x, const ZZ &n) { return (x - 1) / n; }

/* 密钥生成函数
 *
 * 参数：
 *  p：大质数
 *  q：大质数
 *  n = p * q
 *  phi = (p - 1) * (q - 1)
 *  lambda = lcm(p - 1, q - 1) = (p - 1) * (q - 1) / gcd(p - 1, q - 1)
 *  g = n + 1
 *  lamdaInverse = lambda^{-1} mod n^2
 *  k : 大质数的位数
 */
void keyGeneration(ZZ &p, ZZ &q, ZZ &n, ZZ &phi, ZZ &lambda, ZZ &g, ZZ &lambdaInverse, ZZ &r, const long &k)
{
    GenPrime(p, k), GenPrime(q, k);
    n = p * q;
    g = n + 1;
    phi = (p - 1) * (q - 1);
    lambda = phi / GCD(p - 1, q - 1);
    lambdaInverse = InvMod(lambda, n);
    r = RandomBnd(n);
    cout << "--------------------------------------------------密钥生成阶段---------------------------------------------------" << endl;
    cout << "公钥(n, g) : " << endl;
    cout << "n = " << n << endl;
    cout << "g = " << g << endl;
    cout << "---------------------------------------------------------------------------------------------------------------" << endl;
    cout << "私钥(lambda, mu) : " << endl;
    cout << "lambda = " << lambda << endl;
    cout << "mu = " << lambdaInverse << endl;
}

/* 加密函数
 *
 * 参数：
 *  m ：需要加密的明文消息
 *  (n, g) ：公钥
 *
 * 返回值：
 *  加密后得到的密文
 */
ZZ encrypt(const ZZ &m, const ZZ &n, const ZZ &g, const ZZ &r)
{
    // 生成一个随机数 r < n
    // r = RandomBnd(n);
    ZZ c = (PowerMod(g, m, n * n) * PowerMod(r, n, n * n)) % (n * n);
    cout << "----------------------------------------------------加密阶段-----------------------------------------------------" << endl;
    cout << "密文输出 : " << c << endl;
    return c;
}

/* 解密函数
 *
 * 参数：
 *  c：密文
 *  (lambda，lamdaInverse)： 私钥
 */
ZZ decrypt(const ZZ &c, const ZZ &n, const ZZ &lambda, const ZZ &lambdaInverse)
{
    ZZ m = (L_function(PowerMod(c, lambda, n * n), n) * lambdaInverse) % n;
    cout << "----------------------------------------------------解密阶段-----------------------------------------------------" << endl;
    cout << "解密得到 : " << m << endl;
    return m;
}

void validHomomorphic(const ZZ &c1, const ZZ &c2, const ZZ &n, const ZZ &lambda, const ZZ &lambdaInverse)
{
    ZZ c_sum = (c1 * c2) % (n * n);
    ZZ m_sum = (L_function(PowerMod(c_sum, lambda, n * n), n) * lambdaInverse) % n;
    cout << "----------------------------------------------------验证阶段-----------------------------------------------------" << endl;
    cout << "密文相加 : " << c_sum << endl;
    cout << "解密得到 : " << m_sum << endl;
}

int main()
{
    long k = 1024;
    ZZ p, q, n, phi, lambda, lambdaInverse, g, r;
    ZZ m1, m2, c1, c2, m1_d, m2_d;
    keyGeneration(p, q, n, phi, lambda, g, lambdaInverse, r, k);
    cout << "请输入需要加密的明文消息1 : ";
    cin >> m1;
    cout << "请输入需要加密的明文消息2 : ";
    cin >> m2;
    c1 = encrypt(m1, n, g, r);
    c2 = encrypt(m2, n, g, r);
    m1_d = decrypt(c1, n, lambda, lambdaInverse);
    m2_d = decrypt(c2, n, lambda, lambdaInverse);
    validHomomorphic(c1, c2, n, lambda, lambdaInverse);
    return 0;
}
// g++ -g -O2 -std=c++11 -pthread -march=native zyltest-ntl.cpp -o zyltest-ntl -lntl -lgmp -lm