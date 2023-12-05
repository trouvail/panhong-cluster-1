#include <iostream>
#include <ctime>
#include <sstream>
#include <vector>
#include <string>
#include <NTL/ZZ.h>
#include <fstream>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>

using namespace std;
using namespace NTL;

// L函数
ZZ L_function(const ZZ &x, const ZZ &n) { return (x - 1) / n; }

// 全局的时间参数，用于计算时间
clock_t cStart, cEnd, dStart, dEnd;

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
// 通过k来生成关于密钥的所有其他参数，k是大素数的长度
void keyGeneration(ZZ &p, ZZ &q, ZZ &n, ZZ &phi, ZZ &lambda, ZZ &g, ZZ &lambdaInverse, ZZ &r, const long &k)
{
    // 每一次的生成的公私钥都不同，所以得保存
    GenPrime(p, k), GenPrime(q, k);
    n = p * q;
    g = n + 1;
    phi = (p - 1) * (q - 1);
    lambda = phi / GCD(p - 1, q - 1);
    lambdaInverse = InvMod(lambda, n);
    r = RandomBnd(n);
    // cout << "--------------------------------------------------密钥生成阶段---------------------------------------------------" << endl;
    // cout << "公钥(n, g) : " << endl;
    // cout << "n = " << n << endl;
    // cout << "g = " << g << endl;
    // cout << "---------------------------------------------------------------------------------------------------------------" << endl;
    // cout << "私钥(lambda, mu) : " << endl;
    // cout << "lambda = " << lambda << endl;
    // cout << "mu = " << lambdaInverse << endl;
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
    // cout << "----------------------------------------------------加密阶段-----------------------------------------------------" << endl;
    // cout << "密文输出 : " << c << endl;
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
    // cout << "----------------------------------------------------解密阶段-----------------------------------------------------" << endl;
    // cout << "解密得到 : " << m << endl;
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

ZZ stringToNumber(string str)
{
    ZZ number = conv<ZZ>(str[0]);
    long len = str.length();
    for (long i = 1; i < len; i++)
    {
        number *= 128;
        number += conv<ZZ>(str[i]);
    }

    return number;
}

// 解决十进制的string和数字的转换
ZZ stringToNumber1(string str)
{
    ZZ number = conv<ZZ>(str[0] - '0');
    // cout << str[0] << endl;
    // cout << number << endl;
    long len = str.length();
    for (long i = 1; i < len; i++)
    {
        // if(i < 10)
        // {
        //     cout << number << endl;
        // }

        number *= 10;
        number += conv<ZZ>(str[i] - '0');
    }

    return number;
}

string numberToString(ZZ num)
{
    long len = ceil(log(num) / log(128));
    char str[len];
    for (long i = len - 1; i >= 0; i--)
    {
        str[i] = conv<int>(num % 128);
        num /= 128;
    }

    return (string)str;
}

// 返回1为发生错误，返回0为正常，接收参数为文件路径以及密钥
int encryptFile(const string &sourceFilePath, const string &targetFilePath, const ZZ &n, const ZZ &g, const ZZ &r)
{
    // 打开源文件
    ifstream sourceFile(sourceFilePath);
    if (!sourceFile.is_open())
    {
        cerr << "无法打开源文件：" << sourceFilePath << endl;
        return 1;
    }

    // 打开目标文件，如果不存在则创建
    ofstream targetFile(targetFilePath);
    if (!targetFile.is_open())
    {
        cerr << "无法创建或打开目标文件：" << targetFilePath << endl;
        return 1;
    }

    // 读取源文件
    string plainText1((istreambuf_iterator<char>(sourceFile)),
                      istreambuf_iterator<char>());

    // cout << plainText1 << endl;

    // 逐字符的ascii码转换 ------------------------- start
    vector<int> asciiSequence;
    vector<ZZ> cryAsciiSequence, decryAsciiSequence; // 加解密后的ascii序列

    for (char c : plainText1)
    {
        // 先将字符串转换为ASCII码
        asciiSequence.push_back(static_cast<int>(c));
    }

    cStart = clock();
    for (int asciiValue : asciiSequence)
    {
        cryAsciiSequence.push_back(encrypt(ZZ(asciiValue), n, g, r));
    }
    cEnd = clock();

    for (ZZ cryAsciiValue : cryAsciiSequence)
    {
        targetFile << cryAsciiValue << endl;
    }

    printf("加密的时间为：%.2fms\n", (double)(1000.0 * (cEnd - cStart) / CLOCKS_PER_SEC));

    // 关闭文件流
    sourceFile.close();
    targetFile.close();

    cout << "文件加密传输完成！" << endl;

    return 0;
}

// 返回1为发生错误，返回0为正常，接收参数为文件路径以及密钥
int decryptFile(const string &sourceFilePath, const string &targetFilePath, const ZZ &n, const ZZ &lambda, const ZZ &lambdaInverse)
{
    // 打开源文件
    ifstream sourceFile(sourceFilePath);
    if (!sourceFile.is_open())
    {
        cerr << "无法打开源文件：" << sourceFilePath << endl;
        return 1;
    }

    // 打开目标文件，如果不存在则创建
    ofstream targetFile(targetFilePath);
    if (!targetFile.is_open())
    {
        cerr << "无法创建或打开目标文件：" << targetFilePath << endl;
        return 1;
    }

    vector<int> asciiSequence;
    vector<ZZ> cryAsciiSequence, decryAsciiSequence; // 加解密后的ascii序列
    string decryCipherText1, decryCipherText2;

    // 逐行复制源文件内容到目标文件
    dStart = clock();
    string line;
    while (getline(sourceFile, line))
    {
        // cout << line << endl;
        // cout << stringToNumber1(line) << endl;
        decryAsciiSequence.push_back(decrypt(stringToNumber1(line), n, lambda, lambdaInverse));
    }
    dEnd = clock();

    for (ZZ decryAsciiValue : decryAsciiSequence)
    {
        cout << decryAsciiValue << endl;
        // 先将ZZ（ASCII码）转换为int类型，再将int转换为char类型，conv<>函数实现ZZ和其他类型的转换
        decryCipherText1 += static_cast<char>(conv<int>(decryAsciiValue));
    }

    cout << "解密后的字符串为：" << endl;
    cout << decryCipherText1 << endl;

    printf("解密的时间为：%.2fms\n", (double)(1000.0 * (dEnd - dStart) / CLOCKS_PER_SEC));

    // // 逐行复制源文件内容到目标文件
    // string line;
    // while (getline(sourceFile, line))
    // {
    //     targetFile << line << endl;
    // }

    // 关闭文件流
    sourceFile.close();
    targetFile.close();

    cout << "文件解密传输完成！" << endl;

    return 0;
}

int transmitFile(const string &sourceFilePath, const string &targetFilePath)
{
    // 打开源文件
    ifstream sourceFile(sourceFilePath);
    if (!sourceFile.is_open())
    {
        cerr << "无法打开源文件：" << sourceFilePath << endl;
        return 1;
    }

    // 打开目标文件，如果不存在则创建
    ofstream targetFile(targetFilePath);
    if (!targetFile.is_open())
    {
        cerr << "无法创建或打开目标文件：" << targetFilePath << endl;
        return 1;
    }

    // // 读取源文件
    // string sourceContent((istreambuf_iterator<char>(sourceFile)),
    //                      istreambuf_iterator<char>());

    // cout << sourceContent << endl;

    // 逐行复制源文件内容到目标文件
    string line;
    while (getline(sourceFile, line))
    {
        targetFile << line << endl;
    }

    // 关闭文件流
    sourceFile.close();
    targetFile.close();

    cout << "文件传输完成！" << endl;

    return 0;
}

int main(int argc, char *argv[])
{
    clock_t time = clock();
    // k是大素数的长度
    long k = 1024;
    // 关于密钥的各个参数，r是随机数
    ZZ p, q, n, phi, lambda, lambdaInverse, g, r;
    // 分别是明文、密文、解密密文后的明文
    // ZZ m1, m2, c1, c2, m1_d, m2_d; // 测试同态的特性用，实际应用可去掉
    // string plainText1, plainText2, decryCipherText1, decryCipherText2;
    string decryCipherText1, decryCipherText2;

    keyGeneration(p, q, n, phi, lambda, g, lambdaInverse, r, k);

    // 传输文件部分 -------------------------------------------- start
    if ((argc != 4) && (argc != 3))
    {
        cerr << "用法: " << argv[0] << " <源文件路径> <目标文件路径> or ";
        cerr << argv[0] << " <源文件路径> <目标文件路径> <encrypt/decrypt>" << endl;
        return 1;
    }

    string sourceFilePath = argv[1];
    string targetFilePath = argv[2];

    if (argc == 3)
    {
        int ret = transmitFile(sourceFilePath, targetFilePath);

        if (ret)
        {
            return 1;
        }

        return 0;
    }

    string operation = argv[3];

    if (operation != "encrypt" && operation != "decrypt")
    {
        cerr << "Invalid operation. Use 'encrypt' or 'decrypt'." << endl;
        return 1;
    }

    if (operation == "encrypt")
    {
        int ret = encryptFile(sourceFilePath, targetFilePath, n, g, r);

        if (ret)
        {
            return 1;
        }
    }
    else
    {
        // int ret = decryptFile(sourceFilePath, targetFilePath, n, lambda, lambdaInverse);

        // if (ret)
        // {
        //     return 1;
        // }

        string p1;
        ZZ p2;
        cout << "请输入加密数据：" << endl;
        getline(cin, p1, '\n');
        p2 = encrypt(stringToNumber1(p1), n, g, r);
        cout << p2 << endl;


        cout << "请输入解密数据：" << endl;
        ZZ mid1, mid2;
        cout << "1" << endl;
        cin >> mid1;
        cout << "2" << endl;
        mid2 = decrypt(mid1, n, lambda, lambdaInverse);
        cout << "3" << endl;
        cout << mid2 << endl;
        cout << "4" << endl;
    }

    // // 传输文件测试加解密 ------------------ start

    // // 打开源文件
    // ifstream sourceFile(sourceFilePath);
    // if (!sourceFile.is_open())
    // {
    //     cerr << "无法打开源文件：" << sourceFilePath << endl;
    //     return 1;
    // }

    // // 打开目标文件，如果不存在则创建
    // ofstream targetFile(targetFilePath);
    // if (!targetFile.is_open())
    // {
    //     cerr << "无法创建或打开目标文件：" << targetFilePath << endl;
    //     return 1;
    // }

    // // 读取源文件
    // string plainText1((istreambuf_iterator<char>(sourceFile)),
    //                      istreambuf_iterator<char>());

    // cout << plainText1 << endl;

    // // 逐字符的ascii码转换 ------------------------- start
    // vector<int> asciiSequence;
    // vector<ZZ> cryAsciiSequence, decryAsciiSequence; // 加解密后的ascii序列

    // for (char c : plainText1)
    // {
    //     // 先将字符串转换为ASCII码
    //     asciiSequence.push_back(static_cast<int>(c));
    // }

    // cStart = clock();
    // for (int asciiValue : asciiSequence)
    // {
    //     cryAsciiSequence.push_back(encrypt(ZZ(asciiValue), n, g, r));
    // }
    // cEnd = clock();

    // for (ZZ cryAsciiValue : cryAsciiSequence)
    // {
    //     targetFile << cryAsciiValue << endl;;
    // }

    // dStart = clock();
    // for (ZZ cryAsciiValue : cryAsciiSequence)
    // {
    //     decryAsciiSequence.push_back(decrypt(cryAsciiValue, n, lambda, lambdaInverse));
    // }
    // dEnd = clock();

    // for (ZZ decryAsciiValue : decryAsciiSequence)
    // {
    //     // 先将ZZ（ASCII码）转换为int类型，再将int转换为char类型，conv<>函数实现ZZ和其他类型的转换
    //     decryCipherText1 += static_cast<char>(conv<int>(decryAsciiValue));
    // }

    // cout << "解密后的字符串为：" << endl;
    // cout << decryCipherText1 << endl;

    // printf("加密的时间为：%.2fms\n",(double)(1000.0 * (cEnd - cStart) / CLOCKS_PER_SEC));
    // printf("解密的时间为：%.2fms\n",(double)(1000.0 * (dEnd - dStart) / CLOCKS_PER_SEC));

    // // // 逐行复制源文件内容到目标文件
    // // string line;
    // // while (getline(sourceFile, line))
    // // {
    // //     targetFile << line << endl;
    // // }

    // // 关闭文件流
    // sourceFile.close();
    // targetFile.close();

    // cout << "文件传输完成！" << endl;
    // // 传输文件测试加解密 ------------------ end

    // 传输文件部分 -------------------------------------------- end

    // // 测试加密的部分 ---------------------------------------------------------------------- start

    // cout << "请输入需要加密的明文消息1 : ";
    // // cin >> m1;
    // // cin >> plainText1;

    // // 防止遇到空格就停止
    // getline(cin, plainText1, '\n');

    // // 测试函数 ---------------------------------------------- start
    // // mid = stringToNumber(plainText1);

    // // cout << mid << endl;

    // // decryCipherText1 = numberToString(mid);
    // // 测试有问题 会出现未识别的字符 ：“�G��”
    // // 测试函数 ---------------------------------------------- end

    // // 逐字符的ascii码转换 ------------------------- start
    // vector<int> asciiSequence;
    // vector<ZZ> cryAsciiSequence, decryAsciiSequence; // 加解密后的ascii序列

    // for (char c : plainText1)
    // {
    //     // 先将字符串转换为ASCII码
    //     asciiSequence.push_back(static_cast<int>(c));
    // }

    // // // 输出ASCII码序列
    // // cout << "ASCII sequence: ";
    // // for (int asciiValue : asciiSequence)
    // // {
    // //     cout << asciiValue << " ";
    // // }
    // // cout << endl;

    // for (int asciiValue : asciiSequence)
    // {
    //     cryAsciiSequence.push_back(encrypt(ZZ(asciiValue), n, g, r));
    // }

    // // // 输出加密后的ASCII码序列
    // // cout << "加密后的 ASCII sequence: ";
    // // for (ZZ cryAsciiValue : cryAsciiSequence)
    // // {
    // //     cout << cryAsciiValue << " ";
    // // }
    // // cout << endl;

    // for (ZZ cryAsciiValue : cryAsciiSequence)
    // {
    //     decryAsciiSequence.push_back(decrypt(cryAsciiValue, n, lambda, lambdaInverse));
    // }

    // // // 输出解密后的ASCII码序列
    // // cout << "解密后的 ASCII sequence: ";
    // // for (ZZ decryAsciiValue : decryAsciiSequence)
    // // {
    // //     cout << decryAsciiValue << " ";
    // // }
    // // cout << endl;

    // for (ZZ decryAsciiValue : decryAsciiSequence)
    // {
    //     // 先将ZZ（ASCII码）转换为int类型，再将int转换为char类型，conv<>函数实现ZZ和其他类型的转换
    //     decryCipherText1 += static_cast<char>(conv<int>(decryAsciiValue));
    // }

    // // 逐字符的ascii码转换 ------------------------- end

    // // istringstream ss1(plainText1);
    // // ss1 >> mid;
    // // mid = stoi(plainText1);
    // cout << decryCipherText1 << endl;
    // // mid = (int) plainText1; 不能直接强制类型转换
    // // cout << mid << endl;
    // // m1 = ZZ(mid);
    // // cout << m1 << endl;

    // // c1 = encrypt(m1, n, g, r);

    // // 验证同态 ----------------------------------------- start

    // // cout << "请输入需要加密的明文消息2 : ";
    // // // cin >> m2;
    // // // cin >> plainText2;
    // // getline(cin, plainText2, '\n');

    // // istringstream ss2(plainText2);
    // // ss2 >> mid;
    // // m2 = ZZ(mid);
    // // cout << m2;

    // // c2 = encrypt(m2, n, g, r);
    // // m1_d = decrypt(c1, n, lambda, lambdaInverse);
    // // m2_d = decrypt(c2, n, lambda, lambdaInverse);
    // // validHomomorphic(c1, c2, n, lambda, lambdaInverse);

    // // 验证同态 ----------------------------------------- end
    // // 测试加密的部分 ---------------------------------------------------------------------- end

    return 0;
}

// g++ -g -O2 -std=c++11 -pthread -march=native zyltest-ntl.cpp -o zyltest-ntl -lntl -lgmp -lm
// ./zyltest-ntl tsrc tdst encrypt/decrypt